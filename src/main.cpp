#include <cstdlib>
#include <entt/entt.hpp>

#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#include <raymath.h>

#include <quill/LogMacros.h>

#include "core/logger.hpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define ARENA_PANEL_PADDING_TOP (20 + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT)
#define ARENA_PANEL_PADDING_SIDES (20)
#define ARENA_PANEL_HEIGHT (400)
#define ARENA_PANEL_WIDTH (WINDOW_WIDTH - (ARENA_PANEL_PADDING_SIDES * 2))
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

struct Deletable {
    bool shouldDelete;
};

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

/* === Input =============================================================== */

void processMouseInput(
    entt::registry& registry
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

    newEntity = registry.create();
    registry.emplace<Deletable>(newEntity, false);
    registry.emplace<Position>(newEntity, pos);
    
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
    registry.emplace<Velocity>(newEntity, vel);
    registry.emplace<SpeedUpForce>(newEntity, Vector2Zero());
    registry.emplace<AvoidWallsForce>(newEntity, Vector2Zero());

    BIRBS_LOG_INFO(
        "Created birb at {}, {} with rotation {}",
        pos.x,
        pos.y,
        heading
    );
}

void processInput(
    entt::registry& registry
) {
    processMouseInput(registry);
}

/* === Logic =============================================================== */

void speed_up_force_system(
    entt::registry &registry
) {
    auto view = registry.view<SpeedUpForce, const Velocity>();

    view.each([&registry](SpeedUpForce &force, const Velocity &vel) {
        auto normalized = Vector2Normalize(vel.velocity);
        auto magnitude = Vector2Length(vel.velocity);
        auto change = Clamp(
            ENTITY_DESIRED_MAX_SPEED - magnitude,
            -1 * ENTITY_ACCELERATION,
            ENTITY_ACCELERATION
        );
        force.speedUpForce = Vector2Scale(normalized, change);
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

void delete_outside_bounds(
    entt::registry& registry
) {
    auto view = registry.view<const Position, Deletable>();

    view.each([&registry](const Position &pos, Deletable &del) {
        if(
            pos.pos.x < ARENA_PANEL_X - (ENTITY_SIZE) 
            || pos.pos.x > (ARENA_PANEL_X + ARENA_PANEL_WIDTH + ENTITY_SIZE)
            || pos.pos.y < ARENA_PANEL_Y - ENTITY_SIZE
            || pos.pos.y > (ARENA_PANEL_Y + ARENA_PANEL_HEIGHT + ENTITY_SIZE)
        ) [[unlikely]] {
            del.shouldDelete = true;
        }
    });
}

void deletable_system(
    entt::registry& registry
) {
    auto view = registry.view<const Deletable>();

    view.each([&registry](const entt::entity entity, const Deletable &deletable) {
        if(deletable.shouldDelete) {
            registry.destroy(entity);
            BIRBS_LOG_INFO("deleting birb");
        }
    });
}

void update(
    entt::registry &registry
) {
    avoid_walls_force_system(registry);
    speed_up_force_system(registry);
    acceleration_system(registry);

    basic_flight(registry);

    delete_outside_bounds(registry);
    deletable_system(registry);
}

/* === Rendering =========================================================== */

void renderUI(
    bool& shouldExit
) {
    shouldExit = GuiWindowBox(
        Rectangle { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT },
        APPLICATION_TITLE
    );
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
    entt::registry registry;

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
        processInput(registry);
        update(registry);
        render(
            shouldExit,
            registry
        );
    }

    // Close up
    BIRBS_LOG_INFO(
        "birbs closing!"
    );
    CloseWindow();
    return 0;
}
