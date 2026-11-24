#ifdef __ANDROID__
#include "raymob.h"
#else
#include "raylib.h"
#endif
#if defined(_WIN32)
#include <array>
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(
    void* hModule, wchar_t* lpFilename, unsigned long nSize);
#endif
#include "Global.h"
#include "Game.hpp"
#include "FrameController.h"

namespace
{
std::filesystem::path ExecutableDirectory()
{
#if defined(_WIN32)
    std::array<wchar_t, 32768> executablePath{};
    const unsigned long length = GetModuleFileNameW(
        nullptr, executablePath.data(), static_cast<unsigned long>(executablePath.size()));
    if (length > 0 && length < executablePath.size())
        return std::filesystem::path(executablePath.data()).parent_path();
#endif
    return std::filesystem::current_path();
}
}


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

    std::filesystem::path sceneDirectory;
    if (argc > 1) {
        const std::filesystem::path argument(argv[1]);
        const bool isProjectFile = argument.filename() == ProjectConfig::FileName;
        const bool isProjectDirectory = std::filesystem::is_directory(argument) &&
            std::filesystem::exists(argument / ProjectConfig::FileName);
        if (isProjectFile || isProjectDirectory) {
            const auto project = ProjectConfig::Load(argument);
            AssetPaths::Set(project.ResourcesPath(), project.ShadersPath());
            sceneDirectory = project.ScenesPath();
        } else {
            sceneDirectory = argument;
        }
    } else {
        const auto exeDir = ExecutableDirectory();
        if (std::filesystem::is_regular_file(exeDir / ProjectConfig::FileName)) {
            const auto project = ProjectConfig::Load(exeDir);
            AssetPaths::Set(project.ResourcesPath(), project.ShadersPath());
            sceneDirectory = project.ScenesPath();
        } else if (std::filesystem::is_regular_file(
                       std::filesystem::current_path() / ProjectConfig::FileName)) {
            const auto project = ProjectConfig::Load(std::filesystem::current_path());
            AssetPaths::Set(project.ResourcesPath(), project.ShadersPath());
            sceneDirectory = project.ScenesPath();
        } else if (std::filesystem::is_directory(exeDir / "resource") &&
                   std::filesystem::is_directory(exeDir / "shaders")) {
            AssetPaths::Set(exeDir / "resource", exeDir / "shaders");
            if (std::filesystem::is_directory(exeDir / "scenes"))
                sceneDirectory = exeDir / "scenes";
        }
    }

    ResourcesLoader::Init();

    auto& game = Game::get_Instance();
    game.Init(sceneDirectory);

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
        ResourcesLoader::UnloadTextures();
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
    ResourcesLoader::UnloadTextures();
    UnloadShader(ResourcesLoader::lenseShader);
    UnloadShader(ResourcesLoader::lightOnly);
    UnloadShader(ResourcesLoader::basic_shader);
    CloseWindow();

    return 0;
}

