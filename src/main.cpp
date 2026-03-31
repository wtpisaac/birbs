#include "entt/entity/fwd.hpp"
#include <cstdlib>

#include <ankerl/unordered_dense.h>
#include <entt/entt.hpp>

#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#include <raymath.h>

#include <quill/LogMacros.h>

#include "core/logger.hpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define ARENA_PANEL_PADDING_TOP 0
#define ARENA_PANEL_PADDING_SIDES 0
#define ARENA_PANEL_HEIGHT WINDOW_HEIGHT
#define ARENA_PANEL_WIDTH WINDOW_WIDTH
#define ARENA_PANEL_X 0 + ARENA_PANEL_PADDING_SIDES
#define ARENA_PANEL_Y ARENA_PANEL_PADDING_TOP

#define ENTITY_SIZE 20.0
#define ENTITY_INTERIOR_SCALE 0.8
#define ENTITY_ACCELERATION 0.25
#define ENTITY_DESIRED_MAX_SPEED 3.0

#define PRIORITY_WALL 0.8
#define PRIORITY_SPEED 0.2

#define APPLICATION_TITLE "birbs"

struct Triangle {
    Vector2 v1;
    Vector2 v2;
    Vector2 v3;

    Triangle scaled(float scale) {
        return Triangle {
            .v1 = Vector2Scale(this->v1, scale),
            .v2 = Vector2Scale(this->v2, scale),
            .v3 = Vector2Scale(this->v3, scale),
        };
    }

    Triangle rotate(float angle) {
        return Triangle {
            .v1 = Vector2Rotate(this->v1, angle*DEG2RAD),
            .v2 = Vector2Rotate(this->v2, angle*DEG2RAD),
            .v3 = Vector2Rotate(this->v3, angle*DEG2RAD)
        };
    }

    Triangle move(Vector2 pos) {
        return Triangle {
            .v1 = Vector2Add(this->v1, pos),
            .v2 = Vector2Add(this->v2, pos),
            .v3 = Vector2Add(this->v3, pos),
        };
    }
};

constexpr Triangle TRIANGLE_EQ = Triangle {
    .v1 = Vector2{
        .x = 0.0,
        .y = -1.0 * (ENTITY_SIZE / 2.0)
    },
    .v2 = Vector2 {
        .x = -0.8 * (ENTITY_SIZE / 2.0),
        .y = 1.0 * (ENTITY_SIZE / 2.0)
    },
    .v3 = Vector2{
        .x = 0.8 * (ENTITY_SIZE / 2.0),
        .y = 1.0 * (ENTITY_SIZE / 2.0)
    }
};

constexpr Vector2 ARENA_OFFSET = Vector2 {
    .x = ARENA_PANEL_WIDTH / 2.0,
    .y = ARENA_PANEL_HEIGHT / 2.0
};

float rotationFromVector(
    Vector2 vec
) {
    auto nvec = Vector2Normalize(vec);
    auto rads = Vector2Angle(
        Vector2{.y = -1.0}, 
        nvec
    );
    auto degs = (rads * RAD2DEG);
    return degs;
}

/* === ECS Definitions ===================================================== */

struct Position {
    Vector2 pos;
};

struct Velocity {
    Vector2 velocity;
};

struct SpeedUpForce {
    Vector2 speedUpForce;
};

struct AvoidWallsForce {
    Vector2 avoidWallsForce;
};

struct Following {
    entt::entity following;
};

struct Followed {
    entt::entity follower;
    Vector2 desiredFollowerPosition;
};

struct ToDelete {};

/* AI Behavior Definitions

Space to take some notes from the book:

- Entities should have steering behaviors
- These should probably be weighted but choosing the weights is difficult
- You can do a prioritization algorithm to allocate forces instead. might be worth trying
that up front since balancing the weights dynamically seems like a harder variant of this
- probably worth creating a chaining behavior to push away from entities but target a tail?

start with random flights
*/

/* === Standalone Methods ================================================== */

/* === Leader-Follower Tracking ============================================ */

struct GameState {
    entt::registry registry;
    entt::entity pack_leader { entt::null };
    entt::entity pack_trailer { entt::null };
};

/* === Input =============================================================== */

void processMouseInput(
    GameState &gameState
) {
    Vector2 pos;
    Vector2 vel;
    bool isClick;
    entt::entity newEntity;
    float heading;

    pos = GetMousePosition();
    isClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if(
        !isClick
        || pos.x < ARENA_PANEL_X
        || pos.x > ARENA_PANEL_X + ARENA_PANEL_WIDTH 
        || pos.y < ARENA_PANEL_Y
        || pos.y > ARENA_PANEL_Y + ARENA_PANEL_HEIGHT
    ) [[likely]] {
        return;
    }

    newEntity = gameState.registry.create();

    if(gameState.pack_leader == entt::null) [[unlikely]] {
        BIRBS_LOG_INFO("pack leader created!");
        gameState.pack_leader = newEntity;
        gameState.pack_trailer = newEntity;
    } else {
        gameState.registry.emplace<Following>(newEntity, gameState.pack_trailer);
        gameState.registry.emplace<Followed>(gameState.pack_trailer, Followed {
            .desiredFollowerPosition = gameState.registry.get<Position>(gameState.pack_trailer).pos,
            .follower = newEntity
        });

        gameState.pack_trailer = newEntity;
    }

    gameState.registry.emplace<Position>(newEntity, pos);
    
    heading = 
        static_cast<float>(GetRandomValue(0, 359));

    // Create initial velocity vector
    vel = Vector2Scale(
        Vector2Normalize(Vector2Rotate(
            Vector2 { .y = 1.0 },
            heading
        )), 
        0.5
    );
    gameState.registry.emplace<Velocity>(newEntity, vel);
    gameState.registry.emplace<SpeedUpForce>(newEntity, Vector2Zero());
    gameState.registry.emplace<AvoidWallsForce>(newEntity, Vector2Zero());

    BIRBS_LOG_INFO(
        "Created birb at {}, {} with rotation {}",
        pos.x,
        pos.y,
        heading
    );
}

void processInput(
    GameState &gameState
) {
    processMouseInput(gameState);
}

/* === Logic =============================================================== */

void pack_leader_system(
    GameState &gameState
) {
    auto packLeader = gameState.pack_leader;
    if(packLeader == entt::null) [[unlikely]] {
        return;
    }
    auto [velocity] = gameState.registry.get<Velocity>(packLeader);

    auto normalized = Vector2Normalize(velocity);
    auto magnitude = Vector2Length(velocity);
    auto change = Clamp(
        ENTITY_DESIRED_MAX_SPEED - magnitude,
        -1 * ENTITY_ACCELERATION,
        ENTITY_ACCELERATION
    );
    
    gameState.registry.replace<SpeedUpForce>(packLeader, Vector2Scale(normalized, change));
}

void follow_leaders_seek_system(
    GameState &gameState
) {
    auto view = gameState.registry.view<const Following, const Velocity, const Position, SpeedUpForce>();

    view.each([&gameState](const Following &following, const Velocity &vel, const Position &pos, SpeedUpForce &force) {
        auto followed = gameState.registry.get<Followed>(following.following);
        
        auto displacement = Vector2Subtract(followed.desiredFollowerPosition, pos.pos);
        auto normalized = Vector2Normalize(displacement);
        auto magnitude = Vector2Length(displacement);
        auto change = Clamp(
            magnitude,
            -1 * ENTITY_DESIRED_MAX_SPEED,
            ENTITY_DESIRED_MAX_SPEED
        );

        // Attempt #3, follow what the book says and compute _change_ in desired velocity
        Vector2 appliedSpeedForce = Vector2Subtract(
            Vector2Scale(normalized, change),
            vel.velocity
        );
        force.speedUpForce = appliedSpeedForce;
    });
}

void avoid_walls_force_system(
    entt::registry &registry
) {
    auto view = registry.view<const Position, AvoidWallsForce>();

    view.each([](const Position &pos, AvoidWallsForce &avoidWallsForce) {
        bool avoidLeftWall = (pos.pos.x < (ARENA_PANEL_X + (2 * ENTITY_SIZE)));
        bool avoidRightWall = (pos.pos.x > ((ARENA_PANEL_X + ARENA_PANEL_WIDTH) - (2 * ENTITY_SIZE)));
        bool avoidTopWall = (pos.pos.y < (ARENA_PANEL_Y + (2 * ENTITY_SIZE)));
        bool avoidBottomWall = (pos.pos.y > ((ARENA_PANEL_Y + ARENA_PANEL_HEIGHT) - (2 * ENTITY_SIZE)));

        Vector2 avoid = Vector2Zero();

        if(avoidLeftWall) { avoid = Vector2Add(avoid, Vector2 { .x = 1.0 }); };
        if(avoidRightWall) { avoid = Vector2Add(avoid, Vector2 { .x = -1.0 }); };
        if(avoidTopWall) { avoid = Vector2Add(avoid, Vector2 { .y = 1.0 }); };
        if(avoidBottomWall) { avoid = Vector2Add(avoid, Vector2 { .y = -1.0 }); };

        avoidWallsForce.avoidWallsForce = Vector2Scale(Vector2Normalize(avoid), ENTITY_ACCELERATION);
    });
}

void acceleration_system(
    entt::registry &registry
) {
    auto view = registry.view<const SpeedUpForce, const AvoidWallsForce, Velocity>();

    view.each([&registry](const SpeedUpForce &speedUp, const AvoidWallsForce &avoid, Velocity &vel) {
        auto magnitude = Vector2Length(vel.velocity);
        auto normalized = Vector2Normalize(vel.velocity);

        auto resultantForce = Vector2Add(
            Vector2Scale(
                speedUp.speedUpForce, PRIORITY_SPEED),
            Vector2Scale(
                avoid.avoidWallsForce, PRIORITY_WALL
            )
        );

        auto newVelocity = Vector2Add(vel.velocity, resultantForce);
        vel.velocity = newVelocity;
    });
}

void update_follower_positions(
    entt::registry &registry
) {
    auto view = registry.view<Followed, const Position, const Velocity>();

    view.each([](Followed &followed, const Position &pos, const Velocity &vel) {
        auto normalized = Vector2Normalize(vel.velocity);

        auto newFollowerPosition = Vector2Add(
            pos.pos,
            Vector2Scale(
                normalized,
                -2 * ENTITY_SIZE
            )
        );
        followed.desiredFollowerPosition = newFollowerPosition;
    });
}

void basic_flight(
    entt::registry& registry
) {
    auto view = registry.view<Position, const Velocity>();

    view.each([&registry](Position &pos, const Velocity &vel) {
        pos.pos = Vector2Add(
            pos.pos,
            vel.velocity
        );
    });
}

// TODO: Deletion support for following system

void delete_outside_bounds(
    entt::registry& registry
) {
    auto view = registry.view<const Position>();

    view.each([&registry](const entt::entity entity, const Position &pos) {
        if(
            pos.pos.x < ARENA_PANEL_X - (ENTITY_SIZE) 
            || pos.pos.x > (ARENA_PANEL_X + ARENA_PANEL_WIDTH + ENTITY_SIZE)
            || pos.pos.y < ARENA_PANEL_Y - ENTITY_SIZE
            || pos.pos.y > (ARENA_PANEL_Y + ARENA_PANEL_HEIGHT + ENTITY_SIZE)
        ) [[unlikely]] {
            registry.emplace<ToDelete>(entity);
        }
    });
}

// FIXME: There is a better way
// Right now we manually compare to game state, but,
// a followed entity with no following is the leader
// a following entity with no followed is the trail
// every other entity is in the middle, by definition
// maybe better to just take advantage of that with three view.each() blocks
// but i am too tired to do that right now and have to put off this much more interesting
// programming than my day job until later :(

// void repair_pack_state_for_deletions(
//     GameState &gameState
// ) {
//     auto view = gameState.registry.view<ToDelete>();

//     view.each([&gameState](const entt::entity entity) {
//         // Case: Head of pack
//         if(gameState.pack_leader == entity) {
//             // do we have a follower?
//             auto followed = gameState.registry.try_get<Followed>(entity);
//             if(followed == nullptr) {
//                 gameState.pack_leader = entt::null;
//                 gameState.pack_trailer = entt::null;
//                 return;
//             } else {
//                 gameState.pack_leader = followed->follower;
//                 gameState.registry.remove<Following>(followed->follower);
//             }
//         } else if(gameState.pack_trailer{
//             // Case: Tail of pack
//             // Do we have any follower?

//         }
//     });
// }

void deletable_system(
    entt::registry& registry
) {
    auto view = registry.view<const ToDelete>();

    view.each([&registry](const entt::entity entity) {
        registry.destroy(entity);
        BIRBS_LOG_INFO("deleting birb");
    });
}

void update(
    GameState &gameState
) {
    avoid_walls_force_system(gameState.registry);
    pack_leader_system(gameState);
    follow_leaders_seek_system(gameState);
    acceleration_system(gameState.registry);

    basic_flight(gameState.registry);
    update_follower_positions(gameState.registry);

    delete_outside_bounds(gameState.registry);
    deletable_system(gameState.registry);
}

/* === Rendering =========================================================== */

void renderUI(
    bool& shouldExit
) {
    // shouldExit = GuiWindowBox(
    //     Rectangle { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT },
    //     APPLICATION_TITLE
    // );
    shouldExit = false;
}

void renderBirbs(
    entt::registry& registry
) {
    auto view = registry.view<const Position, const Velocity>();

    view.each([](const Position &pos, const Velocity &vel){
        auto rotation = rotationFromVector(vel.velocity);

        auto entityInteriorTriangle = TRIANGLE_EQ;
        entityInteriorTriangle = entityInteriorTriangle.scaled(ENTITY_INTERIOR_SCALE);
        entityInteriorTriangle = entityInteriorTriangle.rotate(rotation);
        entityInteriorTriangle = entityInteriorTriangle.move(
            pos.pos
        );

        auto entityStrokeTriangle = TRIANGLE_EQ;
        entityStrokeTriangle = entityStrokeTriangle.rotate(rotation);
        entityStrokeTriangle = entityStrokeTriangle.move(
            pos.pos
        );

        DrawTriangle(
            entityStrokeTriangle.v1,
            entityStrokeTriangle.v2,
            entityStrokeTriangle.v3,
            BLACK
        );
        DrawTriangle(
            entityInteriorTriangle.v1,
            entityInteriorTriangle.v2,
            entityInteriorTriangle.v3,
            WHITE
        );
    });
}

void renderArena(
    entt::registry& registry
) {
    BeginScissorMode(
        ARENA_PANEL_X, 
        ARENA_PANEL_Y,
        ARENA_PANEL_WIDTH,
        ARENA_PANEL_HEIGHT
    );

    ClearBackground(BLUE);
    renderBirbs(registry);

    EndScissorMode();
}

void render(
    bool& shouldExit,
    entt::registry& registry
) {
    BeginDrawing();
    ClearBackground(RED);

    renderUI(shouldExit);
    renderArena(registry);

    EndDrawing();
}

/* === Main Loop =========================================================== */
int main()
{
    bool shouldExit {false};
    GameState gameState = {
        .registry = entt::registry()
    };

    BIRBS_LOG_INFO(
        "birbs!"
    );

    // Start Raylib rendering
    InitWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        APPLICATION_TITLE
    );
    SetTargetFPS(60);

    // Run loop
    while(!WindowShouldClose() && !shouldExit) {
        processInput(gameState);
        update(gameState);
        render(
            shouldExit,
            gameState.registry
        );
    }

    // Close up
    BIRBS_LOG_INFO(
        "birbs closing!"
    );
    CloseWindow();
    return 0;
}
