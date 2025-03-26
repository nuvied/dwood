#ifdef __ANDROID__
#include "raymob.h"
#else
#include "raylib.h"
#endif
#include "Global.h"
#include "Game.hpp"


Vector2 Global::mousePos = {0,0};
Vector2  Global::lensPosition = {512,256};

Texture2D ResourcesLoader::boat_tex_page;
Texture2D ResourcesLoader::ui_page;
Texture2D ResourcesLoader::inv_items;
Texture2D ResourcesLoader::light_blob0;
Shader ResourcesLoader::basic_shader;
Shader  ResourcesLoader::lenseShader;
Shader ResourcesLoader::lightOnly;


bool Global::debug = false;
Music ResourcesLoader::bg_music;
bool Global::rotor_puzzle_done = false;
bool Global::lensOn = false;
int Global::selectedItemId = 0;



bool running = false;

const int GAME_WIDTH = 1024;   // Fixed logical width
const int GAME_HEIGHT = 512;   // Fixed logical height
const float ASPECT_RATIO = (float)GAME_WIDTH / GAME_HEIGHT;



Vector2 GetMousePositionScaled(int winWidth, int winHeight, int offsetX, int offsetY, float scaleFactor) {
    Vector2 rawMouse = GetMousePosition();
    return {
        (rawMouse.x - offsetX) * 0.5f / scaleFactor,  // Scale X
        (rawMouse.y - offsetY) * 0.5f / scaleFactor   // Scale Y
    };
}

int main() {
    if(running)return 1;
    //SetTraceLogLevel(LOG_FATAL);
    //SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1024, 512, "Devilwood mystery");
    SetWindowState(FLAG_WINDOW_RESIZABLE); // Allow resizing
    InitAudioDevice();
    
    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    
    ResourcesLoader::Init();
    
    PlayMusicStream(ResourcesLoader::bg_music);
    
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
        UpdateMusicStream(ResourcesLoader::bg_music);
        // Render to texture (game scene)
        BeginTextureMode(target);
            ClearBackground(DARKBLUE); // Game background (blue)
           
            
            
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
    UnloadMusicStream(ResourcesLoader::bg_music);
    UnloadTexture(ResourcesLoader::inv_items);
    UnloadShader(ResourcesLoader::lenseShader);
    UnloadShader(ResourcesLoader::lightOnly);

    CloseWindow();

    return 0;
}

