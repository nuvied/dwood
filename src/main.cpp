#ifdef __ANDROID__
#include "raymob.h"
#else
#include "raylib.h"
#endif
#include "Global.h"
#include "Game.hpp"
#include "FrameController.h"


Vector2 Global::mousePos = {0,0};
Vector2  Global::lensPosition = {512,256};

Texture2D ResourcesLoader::boat_tex_page;
Texture2D ResourcesLoader::ui_page;
Texture2D ResourcesLoader::inv_items;
Texture2D ResourcesLoader::light_blob0;
Texture2D ResourcesLoader::light_page;

Shader ResourcesLoader::basic_shader;
Shader  ResourcesLoader::lenseShader;
Shader ResourcesLoader::lightOnly;
Shader ResourcesLoader::glowShader;


bool Global::debug = false;
Music ResourcesLoader::bg_music;
bool Global::rotor_puzzle_done = false;
bool Global::oar_placed = false;
bool Global::lensOn = false;
int Global::selectedItemId = 0;



bool running = false;

int main(int argc, char* argv[]) {
    if(running)return 1;
    //SetTraceLogLevel(LOG_FATAL);
    //SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1024, 512, "Devilwood mystery");
    SetWindowState(FLAG_WINDOW_RESIZABLE); // Allow resizing
    InitAudioDevice();

    ResourcesLoader::Init();
    PlayMusicStream(ResourcesLoader::bg_music);

    auto& game = Game::get_Instance();
    game.Init(argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{});
    const int gameWidth = game.game_width;
    const int gameHeight = game.game_height;
    const float aspectRatio = static_cast<float>(gameWidth) / gameHeight;
    SetWindowSize(gameWidth, gameHeight);

    RenderTexture2D target = LoadRenderTexture(game.RenderTargetWidth(),
                                               game.RenderTargetHeight());
    if (!IsRenderTextureValid(target)) {
        TraceLog(LOG_ERROR, "Unable to create the game render target at %dx%d.",
                 game.RenderTargetWidth(), game.RenderTargetHeight());
        game.Unload();
        ResourcesLoader::UnloadFonts();
        CloseWindow();
        return 1;
    }
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    FrameController frameController;

    
    while (!WindowShouldClose()) {
        // Get current window size
        int winWidth = GetScreenWidth();
        int winHeight = GetScreenHeight();
        float winAspect = static_cast<float>(winWidth) / winHeight;

        // Calculate scaled rendering size
        int scaledWidth, scaledHeight;
        int offsetX = 0, offsetY = 0;

        if (winAspect > aspectRatio) {
            // Window is too wide, pillarboxing
            scaledHeight = winHeight;
            scaledWidth = static_cast<int>(scaledHeight * aspectRatio);
            offsetX = (winWidth - scaledWidth) / 2;
        } else {
            // Window is too tall, letterboxing
            scaledWidth = winWidth;
            scaledHeight = static_cast<int>(scaledWidth / aspectRatio);
            offsetY = (winHeight - scaledHeight) / 2;
        }
        
        const float windowScale = static_cast<float>(scaledWidth) / target.texture.width;
        const Vector2 mouse = GetMousePosition();
        Global::mousePos = {
            (mouse.x - offsetX) / (windowScale * game.RenderScale()),
            (mouse.y - offsetY) / (windowScale * game.RenderScale())
        };

        game.Update(frameController.GetDeltaTime());
        UpdateMusicStream(ResourcesLoader::bg_music);
        // Render to texture (game scene)
        BeginTextureMode(target);
            ClearBackground(DARKBLUE); // Game background (blue)
           
            
            
        game.Draw();
       
        EndTextureMode();

        // Draw the final scene to the window
        BeginDrawing();
            ClearBackground(BLACK); // Fill unused areas with black (letterbox effect)
            DrawTexturePro(target.texture,
                (Rectangle){0, 0, (float)target.texture.width, -(float)target.texture.height}, // Flip Y
                           (Rectangle){(float)offsetX, (float)offsetY, (float)scaledWidth, (float)scaledHeight},
                (Vector2){0, 0}, 0, WHITE);
        EndDrawing();
        frameController.EndFrame();
    }

    // Cleanup
    game.Unload();
    UnloadRenderTexture(target);
    UnloadTexture(ResourcesLoader::boat_tex_page);
    UnloadTexture(ResourcesLoader::ui_page);
    UnloadMusicStream(ResourcesLoader::bg_music);
    UnloadTexture(ResourcesLoader::inv_items);
    UnloadShader(ResourcesLoader::lenseShader);
    UnloadShader(ResourcesLoader::lightOnly);
    UnloadShader(ResourcesLoader::basic_shader);
    UnloadTexture(ResourcesLoader::light_page);

    CloseWindow();

    return 0;
}

