#include "raylib.h"
#include "rlImGui.h"

#include "imgui.h"

int main()
{
    InitWindow(1280,720, "Game Editor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    rlImGuiSetup(true);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        rlImGuiBegin();
        ImGui::Begin("test window");
        ImGui::End();

        rlImGuiEnd();
        
        EndDrawing();
    }
    CloseWindow();
    rlImGuiShutdown();

    return 0;
}