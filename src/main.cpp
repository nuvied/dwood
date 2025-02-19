#include "raylib.h"
#include "Global.h"
#include "Game.hpp"


Vector2 Global::mousePos = {0,0};
Texture2D ResourcesLoader::boat_tex_page;
Texture2D ResourcesLoader::ui_page;
bool Global::debug = true;

bool running = false;

const int GAME_WIDTH = 1024;   // Fixed logical width
const int GAME_HEIGHT = 512;   // Fixed logical height
const float ASPECT_RATIO = (float)GAME_WIDTH / GAME_HEIGHT;


Vector2 GetMousePositionScaled(int winWidth, int winHeight, int offsetX, int offsetY, float scaleFactor) {
    Vector2 rawMouse = GetMousePosition();
    return (Vector2){
        (rawMouse.x - offsetX) * 0.5f / scaleFactor,  // Scale X
        (rawMouse.y - offsetY) * 0.5f / scaleFactor   // Scale Y
    };
}

int main() {
    if(running)return 1;
    InitWindow(1024, 512, "Raylib Letterboxing Example");
    SetWindowState(FLAG_WINDOW_RESIZABLE); // Allow resizing

    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    
    ResourcesLoader::Init();
    
    
    Game::get_Instance().Init();
    
    
    
    while (!WindowShouldClose()) {
        // Get current window size
        int winWidth = GetScreenWidth();
        int winHeight = GetScreenHeight();
        float winAspect = (float)winWidth / winHeight;

        // Calculate scaled rendering size
        int scaledWidth, scaledHeight;
        int offsetX = 0, offsetY = 0;

        if (winAspect > ASPECT_RATIO) {
            // Window is too wide, pillarboxing
            scaledHeight = winHeight;
            scaledWidth = (int)(scaledHeight * ASPECT_RATIO);
            offsetX = (winWidth - scaledWidth) / 2;
        } else {
            // Window is too tall, letterboxing
            scaledWidth = winWidth;
            scaledHeight = (int)(scaledWidth / ASPECT_RATIO);
            offsetY = (winHeight - scaledHeight) / 2;
        }
        
        float scaleFactor = scaledWidth / GAME_WIDTH;
        Vector2 p = GetMousePositionScaled(winWidth, winHeight, offsetX, offsetY, scaleFactor);
        Global::mousePos = { p.x * (float)GAME_WIDTH / (float)winWidth, p.y * (float)GAME_WIDTH / (float)winWidth };

        Game::get_Instance().Update(GetFrameTime());
        
        // Render to texture (game scene)
        BeginTextureMode(target);
            ClearBackground(DARKBLUE); // Game background (blue)
            //DrawRectangle(100, 100, 200, 150, RED); // Example game object
            
            DrawText("Letterboxing in Raylib", 10, 10, 10, WHITE);
        Game::get_Instance().Draw();
       
        EndTextureMode();

        // Draw the final scene to the window
        BeginDrawing();
            ClearBackground(BLACK); // Fill unused areas with black (letterbox effect)
            DrawTexturePro(target.texture,
                (Rectangle){0, 0, (float)target.texture.width, -(float)target.texture.height}, // Flip Y
                           (Rectangle){(float)offsetX, (float)offsetY, (float)scaledWidth, (float)scaledHeight},
                (Vector2){0, 0}, 0, WHITE);
        EndDrawing();
    }

    // Cleanup
    Game::get_Instance().Unload();
    UnloadRenderTexture(target);
    UnloadTexture(ResourcesLoader::boat_tex_page);
    UnloadTexture(ResourcesLoader::ui_page);
    CloseWindow();

    return 0;
}

