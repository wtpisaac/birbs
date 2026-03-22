#include <raylib.h>
#include <quill/LogMacros.h>

#include "core/logger.hpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define APPLICATION_TITLE "birbs"

int main()
{
    QUILL_LOG_INFO(
        BIRBS_LOGGER,
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
    while(!WindowShouldClose()) {
        // Logic

        // Drawing
        BeginDrawing();
        ClearBackground(DARKBLUE);
        EndDrawing();
    }

    // Close up
    QUILL_LOG_INFO(
        BIRBS_LOGGER,
        "birbs closing!"
    );
    CloseWindow();
    return 0;
}
