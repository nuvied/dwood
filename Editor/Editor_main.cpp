#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef DrawTextEx
#undef DrawText
#undef LoadImage
#undef ShowCursor
#undef CloseWindow
#undef Rectangle
#endif

#include "raylib.h"
#include "rlImGui.h"

#include "imgui.h"

#include "FrameController.h"
#include "Game.hpp"
#include "SceneEditor.h"
#include "UIEditor.h"
#include "InventoryItemEditor.h"
#include "SceneStack.h"
#include "Screen.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

#if IMGUI_VERSION_NUM < 19200
#error The editor requires Dear ImGui 1.92 or newer.
#endif

class BuildAndRunController
{
public:
    ~BuildAndRunController()
    {
#if defined(_WIN32)
        CloseBuild();
        if (gameProcess)
            CloseHandle(gameProcess);
#endif
    }

    bool IsBuilding() const
    {
#if defined(_WIN32)
        return buildProcess != nullptr;
#else
        return false;
#endif
    }

    bool IsRunning() const
    {
#if defined(_WIN32)
        return gameProcess != nullptr;
#else
        return false;
#endif
    }
    bool IsBusy() const { return IsBuilding() || IsRunning(); }
    bool HasError() const { return hasError; }
    const std::string& Status() const { return status; }
    const std::string& Output() const { return output; }

    bool Start(const std::filesystem::path& buildDirectory,
               const std::filesystem::path& sceneDirectory)
    {
#if defined(_WIN32)
        if (IsBusy())
            return false;

        output.clear();
        hasError = false;
        status = "Building runtime...";
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &security, 0) ||
            !SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
            if (readPipe)
                CloseHandle(readPipe);
            if (writePipe)
                CloseHandle(writePipe);
            SetFailure("Unable to create the build output pipe.");
            return false;
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = writePipe;
        startup.hStdError = writePipe;
        const std::wstring command = Quote(L"cmake") + L" --build " +
                                     Quote(buildDirectory.wstring()) +
                                     L" --target devilwood";
        std::vector<wchar_t> commandLine(command.begin(), command.end());
        commandLine.push_back(L'\0');
        PROCESS_INFORMATION process{};
        const BOOL started = CreateProcessW(
            nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
            nullptr, buildDirectory.c_str(), &startup, &process);
        CloseHandle(writePipe);
        if (!started) {
            CloseHandle(readPipe);
            SetFailure("Unable to start CMake build (Windows error " +
                       std::to_string(GetLastError()) + ").");
            return false;
        }

        CloseHandle(process.hThread);
        buildProcess = process.hProcess;
        buildOutput = readPipe;
        this->buildDirectory = buildDirectory;
        this->sceneDirectory = sceneDirectory;
        return true;
#else
        (void)buildDirectory;
        (void)sceneDirectory;
        SetFailure("Build & Run is not supported on this platform.");
        return false;
#endif
    }

    bool Update()
    {
#if defined(_WIN32)
        if (buildProcess) {
            DrainBuildOutput();
            DWORD exitCode = STILL_ACTIVE;
            if (GetExitCodeProcess(buildProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                DrainBuildOutput();
                CloseBuild();
                if (exitCode != 0) {
                    SetFailure("Runtime build failed with exit code " +
                               std::to_string(exitCode) + ".");
                    return true;
                }
                if (!LaunchGame())
                    return true;
                status = "Game is running in a separate window.";
            }
        }

        if (gameProcess) {
            DWORD exitCode = STILL_ACTIVE;
            if (GetExitCodeProcess(gameProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                CloseHandle(gameProcess);
                gameProcess = nullptr;
                status = exitCode == 0
                    ? "Separate game window closed."
                    : "Separate game exited with code " + std::to_string(exitCode) + ".";
                hasError = exitCode != 0;
                return hasError;
            }
        }
#endif
        return false;
    }

    void Stop()
    {
#if defined(_WIN32)
        if (gameProcess) {
            TerminateProcess(gameProcess, 0);
            WaitForSingleObject(gameProcess, 5000);
            CloseHandle(gameProcess);
            gameProcess = nullptr;
            status = "Separate game stopped.";
            hasError = false;
        }
#endif
    }

private:
    void SetFailure(const std::string& error)
    {
        status = error;
        hasError = true;
    }

#if defined(_WIN32)
    static std::wstring Quote(const std::wstring& argument)
    {
        std::wstring result = L"\"";
        std::size_t backslashes = 0;
        for (const wchar_t character : argument) {
            if (character == L'\\') {
                ++backslashes;
            } else if (character == L'\"') {
                result.append(backslashes * 2 + 1, L'\\');
                result.push_back(L'\"');
                backslashes = 0;
            } else {
                result.append(backslashes, L'\\');
                backslashes = 0;
                result.push_back(character);
            }
        }
        result.append(backslashes * 2, L'\\');
        result.push_back(L'\"');
        return result;
    }

    void DrainBuildOutput()
    {
        char buffer[2048];
        DWORD available = 0;
        while (buildOutput && PeekNamedPipe(buildOutput, nullptr, 0, nullptr,
                                            &available, nullptr) && available > 0) {
            DWORD read = 0;
            const DWORD requested = std::min<DWORD>(available, sizeof(buffer));
            if (!ReadFile(buildOutput, buffer, requested, &read, nullptr) || read == 0)
                break;
            output.append(buffer, read);
            if (output.size() > 32768)
                output.erase(0, output.size() - 32768);
        }
    }

    void CloseBuild()
    {
        if (buildProcess) {
            CloseHandle(buildProcess);
            buildProcess = nullptr;
        }
        if (buildOutput) {
            CloseHandle(buildOutput);
            buildOutput = nullptr;
        }
    }

    bool LaunchGame()
    {
        const auto executable = buildDirectory / "devilwood.exe";
        const std::wstring command = Quote(executable.wstring()) + L" " +
                                     Quote(sceneDirectory.wstring());
        std::vector<wchar_t> commandLine(command.begin(), command.end());
        commandLine.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr,
                            FALSE, CREATE_NO_WINDOW, nullptr, buildDirectory.c_str(),
                            &startup, &process)) {
            SetFailure("Unable to launch the built game (Windows error " +
                       std::to_string(GetLastError()) + ").");
            return false;
        }
        CloseHandle(process.hThread);
        gameProcess = process.hProcess;
        return true;
    }

    HANDLE buildProcess = nullptr;
    HANDLE buildOutput = nullptr;
    HANDLE gameProcess = nullptr;
    std::filesystem::path buildDirectory;
    std::filesystem::path sceneDirectory;
#endif

    std::string status;
    std::string output;
    bool hasError = false;
};


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

int main(int argc, char* argv[])
{
    InitWindow(1280,720, "Game Editor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitAudioDevice();

    const auto sceneDirectory = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(DEVILWOOD_SCENES_DIR);
    UIEditor uiEditor(sceneDirectory);
    InventoryItemEditor inventoryItemEditor(sceneDirectory);
    SceneEditor sceneEditor(sceneDirectory, uiEditor);
    RenderTexture2D target = LoadRenderTexture(sceneEditor.RenderTargetWidth(),
                                               sceneEditor.RenderTargetHeight());
    int failedRenderWidth = 0;
    int failedRenderHeight = 0;
    if (!IsRenderTextureValid(target)) {
        TraceLog(LOG_ERROR, "Unable to create Game View render target at %dx%d.",
                 sceneEditor.RenderTargetWidth(), sceneEditor.RenderTargetHeight());
        sceneEditor.ReportError("Unable to create the configured Game View render target.");
        failedRenderWidth = sceneEditor.RenderTargetWidth();
        failedRenderHeight = sceneEditor.RenderTargetHeight();
    }
    if (IsRenderTextureValid(target))
        SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    bool isPlaying = false;
    BuildAndRunController externalRun;
    rlImGuiSetup(true);
    ResourcesLoader::Init();
    Screen previewLighting;
    bool previewLightingReady = false;
    try {
        previewLighting.InitLighting();
        previewLightingReady = true;
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Scene lighting preview unavailable: %s", e.what());
        sceneEditor.ReportError(e.what());
    }
    PlayMusicStream(ResourcesLoader::bg_music);
    PauseMusicStream(ResourcesLoader::bg_music);
    bool gameReady = false;
    try {
        Game::get_Instance().Init(sceneEditor.SceneDirectory());
        gameReady = true;
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Game initialization failed: %s", e.what());
        sceneEditor.ReportError(std::string("Game initialization failed: ") + e.what());
    }
    FrameController frameController;
    bool showSceneWindow = true;
    bool showSceneSettingsWindow = false;
    bool showSceneManagerWindow = false;
    bool showUIManagerWindow = false;
    bool showInventoryItemWindow = false;
    bool showGameObjectsWindow = true;
    bool showGameViewWindow = true;
    bool showBuildOutputWindow = false;
    bool exitRequested = false;

#if defined(_WIN32)
    std::array<wchar_t, 32768> executablePath{};
    const DWORD executablePathLength = GetModuleFileNameW(
        nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    const std::filesystem::path buildDirectory = executablePathLength > 0 &&
        executablePathLength < executablePath.size()
        ? std::filesystem::path(executablePath.data()).parent_path()
        : std::filesystem::current_path();
#else
    const std::filesystem::path buildDirectory = std::filesystem::current_path();
#endif

    while (!exitRequested)
    {
        if (WindowShouldClose()) {
            if (!inventoryItemEditor.SaveForRun())
                showInventoryItemWindow = true;
            else if (!uiEditor.SaveForRun())
                showUIManagerWindow = true;
            else if (sceneEditor.RequestExit())
                break;
        }
        const float deltaTime = frameController.GetDeltaTime();
        rlImGuiBeginDelta(deltaTime);

        if (externalRun.Update())
            showBuildOutputWindow = true;

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Scenes", nullptr, &showSceneWindow);
                ImGui::MenuItem("Scene Settings", nullptr, &showSceneSettingsWindow);
                ImGui::MenuItem("Scene Manager", nullptr, &showSceneManagerWindow);
                ImGui::MenuItem("UI Manager", nullptr, &showUIManagerWindow);
                ImGui::MenuItem("Inventory Item Editor", nullptr, &showInventoryItemWindow);
                ImGui::MenuItem("Game View", nullptr, &showGameViewWindow);
                ImGui::MenuItem("Game Objects", nullptr, &showGameObjectsWindow);
                ImGui::MenuItem("Build Output", nullptr, &showBuildOutputWindow);
                ImGui::EndMenu();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(externalRun.IsBusy() || !gameReady);
            if (ImGui::Button(isPlaying ? "Stop" : "Play")) {
                if (isPlaying) {
                    isPlaying = false;
                    Game::get_Instance().StopGame();
                    PauseMusicStream(ResourcesLoader::bg_music);
                    Global::mousePos = {-1, -1};
                } else {
                    if (!inventoryItemEditor.SaveForRun()) {
                        showInventoryItemWindow = true;
                    } else if (!uiEditor.SaveForRun()) {
                        showUIManagerWindow = true;
                    } else if (!sceneEditor.SaveForRun()) {
                        showSceneWindow = true;
                    } else {
                        try {
                            Game::get_Instance().StartGame();
                            isPlaying = true;
                            ResumeMusicStream(ResourcesLoader::bg_music);
                        } catch (const std::exception& e) {
                            sceneEditor.ReportError(e.what());
                            showSceneWindow = true;
                        }
                    }
                }
            }
            if (!gameReady && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Game initialization failed; see the Scenes window.");
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (externalRun.IsRunning()) {
                if (ImGui::Button("Stop Build & Run"))
                    externalRun.Stop();
            } else if (externalRun.IsBuilding()) {
                ImGui::BeginDisabled();
                ImGui::Button("Building...");
                ImGui::EndDisabled();
            } else {
                ImGui::BeginDisabled(isPlaying);
                if (ImGui::Button("Build & Run")) {
                    if (!inventoryItemEditor.SaveForRun()) {
                        showInventoryItemWindow = true;
                    } else if (!uiEditor.SaveForRun()) {
                        showUIManagerWindow = true;
                    } else if (!sceneEditor.SaveForRun()) {
                        showSceneWindow = true;
                    } else if (externalRun.Start(buildDirectory, sceneEditor.SceneDirectory()))
                        showBuildOutputWindow = false;
                    else if (externalRun.HasError())
                        showBuildOutputWindow = true;
                }
                ImGui::EndDisabled();
            }
            ImGui::EndMainMenuBar();
        }

        if (isPlaying && showGameObjectsWindow) {
            ImGui::SetNextWindowPos(ImVec2(1040, 120), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(220, 250), ImGuiCond_FirstUseEver);
            ImGui::Begin("Game Objects");
            const auto& stack = Game::get_Instance().scene_stak;
            if (stack && !stack->stack.empty()) {
                for (const auto& entity : stack->stack.back()->entities)
                    ImGui::TextUnformatted(entity->name.c_str());
            }
            ImGui::End();
        }
        sceneEditor.Draw(exitRequested, !isPlaying, showSceneWindow, showSceneSettingsWindow,
                         showSceneManagerWindow);
        if (!isPlaying && showUIManagerWindow)
            uiEditor.Draw(showUIManagerWindow, sceneEditor.RenderTargetWidth(),
                          sceneEditor.RenderTargetHeight());
        if (!isPlaying && showInventoryItemWindow)
            inventoryItemEditor.Draw(showInventoryItemWindow);

        const int requestedWidth = sceneEditor.RenderTargetWidth();
        const int requestedHeight = sceneEditor.RenderTargetHeight();
        if (requestedWidth != failedRenderWidth || requestedHeight != failedRenderHeight) {
            failedRenderWidth = 0;
            failedRenderHeight = 0;
        }
        if ((target.texture.width != requestedWidth || target.texture.height != requestedHeight) &&
            (requestedWidth != failedRenderWidth || requestedHeight != failedRenderHeight)) {
            RenderTexture2D resizedTarget = LoadRenderTexture(requestedWidth, requestedHeight);
            if (IsRenderTextureValid(resizedTarget)) {
                SetTextureFilter(resizedTarget.texture, TEXTURE_FILTER_POINT);
                if (IsRenderTextureValid(target))
                    UnloadRenderTexture(target);
                target = resizedTarget;
            } else {
                TraceLog(LOG_ERROR, "Unable to resize Game View render target to %dx%d.",
                         requestedWidth, requestedHeight);
                sceneEditor.ReportError("Unable to resize the Game View render target.");
                failedRenderWidth = requestedWidth;
                failedRenderHeight = requestedHeight;
            }
        }

        float gameImageScale = 0.0f;
        Vector2 gameImagePosition{};
        bool gameImageHovered = false;
        if (isPlaying && showGameViewWindow) {
            ImGui::SetNextWindowPos(ImVec2(220, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(800, 480), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Game View", &showGameViewWindow)) {
                if (IsRenderTextureValid(target)) {
                    const ImVec2 available = ImGui::GetContentRegionAvail();
                    if (available.x > 0.0f && available.y > 0.0f) {
                        const float scaleX = available.x / target.texture.width;
                        const float scaleY = available.y / target.texture.height;
                        gameImageScale = std::min(scaleX, scaleY);
                        const ImVec2 imageSize(target.texture.width * gameImageScale,
                                               target.texture.height * gameImageScale);
                        const ImVec2 cursor = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(ImVec2(
                            cursor.x + (available.x - imageSize.x) * 0.5f,
                            cursor.y + (available.y - imageSize.y) * 0.5f));
                        ImGui::Image((ImTextureID)target.texture.id, imageSize,
                                     ImVec2(0, 1), ImVec2(1, 0));
                        const ImVec2 imagePosition = ImGui::GetItemRectMin();
                        gameImagePosition = {imagePosition.x, imagePosition.y};
                        gameImageHovered = ImGui::IsItemHovered();
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                                       "Game View render target unavailable");
                }
            }
            ImGui::End();
        }

        Global::mousePos = {-1, -1};
        if (isPlaying)
        {
            Vector2 mouse = GetMousePosition();
            const float worldScale = Game::get_Instance().RenderScale();
            if (gameImageHovered && gameImageScale > 0.0f && worldScale > 0.0f)
                Global::mousePos = {
                    (mouse.x - gameImagePosition.x) / (gameImageScale * worldScale),
                    (mouse.y - gameImagePosition.y) / (gameImageScale * worldScale)
                };

            Game::get_Instance().Update(deltaTime);
            UpdateMusicStream(ResourcesLoader::bg_music);
        }

        if (isPlaying && IsRenderTextureValid(target))
        {
            BeginTextureMode(target);
            ClearBackground(DARKBLUE);
            Game::get_Instance().Draw();
            EndTextureMode();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        if (!isPlaying)
        {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          Color{35, 38, 45, 255});
            sceneEditor.DrawScene(previewLightingReady ? &previewLighting : nullptr);
        }
        if (showBuildOutputWindow) {
            ImGui::SetNextWindowSize(ImVec2(640, 320), ImGuiCond_FirstUseEver);
            ImGui::Begin("Build Output", &showBuildOutputWindow);
            const ImVec4 statusColor = externalRun.HasError()
                ? ImVec4(1, 0.4f, 0.4f, 1)
                : ImVec4(0.5f, 1, 0.5f, 1);
            ImGui::TextColored(statusColor, "%s", externalRun.Status().c_str());
            ImGui::BeginChild("Build log", ImVec2(0, 0), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(externalRun.Output().c_str());
            ImGui::EndChild();
            ImGui::End();
        }
        rlImGuiEnd();
        EndDrawing();
        frameController.EndFrame();
    }

    sceneEditor.ShutdownNodeEditor();
    uiEditor.Shutdown();
    rlImGuiShutdown();
    previewLighting.UnloadLighting();
    Game::get_Instance().scene_stak.reset();
    Game::get_Instance().Unload();
    ResourcesLoader::UnloadFonts();
    UnloadRenderTexture(target);
    UnloadTexture(ResourcesLoader::boat_tex_page);
    UnloadTexture(ResourcesLoader::ui_page);
    UnloadMusicStream(ResourcesLoader::bg_music);
    UnloadTexture(ResourcesLoader::inv_items);
    UnloadShader(ResourcesLoader::lenseShader);
    UnloadShader(ResourcesLoader::lightOnly);
    UnloadShader(ResourcesLoader::basic_shader);
    UnloadShader(ResourcesLoader::glowShader);
    UnloadTexture(ResourcesLoader::light_page);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}