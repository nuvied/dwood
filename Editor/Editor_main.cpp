#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#include <shobjidl.h>
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
#include "ProjectConfig.h"

#include "SceneEditor.h"
#include "UIEditor.h"
#include "InventoryItemEditor.h"
#include "SoundManagerEditor.h"
#include "SceneStack.h"
#include "Screen.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

#if IMGUI_VERSION_NUM < 19200
#error The editor requires Dear ImGui 1.92 or newer.
#endif
#if !defined(IMGUI_HAS_DOCK)
#error The editor requires the Dear ImGui docking branch; install vcpkg imgui[docking-experimental].
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
               const std::filesystem::path& projectFile)
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
        this->projectFile = projectFile;



        return true;
#else
        (void)buildDirectory;
        (void)projectFile;



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
                                     Quote(projectFile.wstring());


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

    std::filesystem::path projectFile;


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
bool Global::rotor_puzzle_done = false;
bool Global::oar_placed = false;
bool Global::lensOn = false;
int Global::selectedItemId = 0;

namespace
{
struct EditorState
{
    std::filesystem::path lastProject;
    std::vector<std::filesystem::path> recentProjects;
};

struct DebugLogEntry
{
    int level = LOG_INFO;
    bool isGameLog = false;
    std::string text;
};

class DebugLogBuffer
{
public:
    static constexpr std::size_t MaxEntries = 2000;

    static DebugLogBuffer& Instance()
    {
        static DebugLogBuffer instance;
        return instance;
    }

    void Append(int level, bool isGameLog, std::string text)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (entries.size() >= MaxEntries)
            entries.pop_front();
        entries.push_back({level, isGameLog, std::move(text)});
        ++revision;
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        entries.clear();
        ++revision;
    }

    std::vector<DebugLogEntry> Snapshot(std::uint64_t& outRevision) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        outRevision = revision;
        return {entries.begin(), entries.end()};
    }

private:
    mutable std::mutex mutex;
    std::deque<DebugLogEntry> entries;
    std::uint64_t revision = 0;
};

const char* LogLevelPrefix(int level)
{
    switch (level) {
        case LOG_TRACE: return "[TRACE] ";
        case LOG_DEBUG: return "[DEBUG] ";
        case LOG_INFO: return "[INFO] ";
        case LOG_WARNING: return "[WARNING] ";
        case LOG_ERROR: return "[ERROR] ";
        case LOG_FATAL: return "[FATAL] ";
        default: return "[LOG] ";
    }
}

void EditorTraceLogCallback(int logLevel, const char* text, va_list args)
{
    if (!text)
        return;
    va_list copy;
    va_copy(copy, args);
    const int needed = std::vsnprintf(nullptr, 0, text, copy);
    va_end(copy);
    std::string formatted;
    if (needed > 0) {
        formatted.resize(static_cast<std::size_t>(needed));
        std::vsnprintf(formatted.data(), formatted.size() + 1, text, args);
    }
    const std::string gameTag = "[GAME] ";
    const bool isGameLog = formatted.rfind(gameTag, 0) == 0;
    const std::string displayMessage = isGameLog ? formatted.substr(gameTag.size()) : formatted;
    const std::string line = (isGameLog ? std::string("[GAME] ") : std::string(LogLevelPrefix(logLevel))) + displayMessage;
    std::fprintf(logLevel >= LOG_WARNING ? stderr : stdout, "%s\n", line.c_str());
    DebugLogBuffer::Instance().Append(logLevel, isGameLog, line);
}

struct ProjectEditorSettings
{
    bool showSceneWindow = true;
    bool showSceneSettingsWindow = false;
    bool showSceneManagerWindow = false;
    bool showUIManagerWindow = false;
    bool showSoundManagerWindow = false;
    bool showInventoryItemWindow = false;
    bool showObjectListWindow = true;
    bool showVariableManagerWindow = false;
    bool showGameObjectsWindow = true;
    bool showGameViewWindow = true;
    bool showBuildOutputWindow = false;
    bool showDebugLogWindow = false;
};

std::filesystem::path ProjectEditorSettingsPath(const std::filesystem::path& sceneDirectory)
{
    return sceneDirectory / ".devilwood" / "editor_settings.json";
}

ProjectEditorSettings LoadProjectEditorSettings(
    const std::filesystem::path& sceneDirectory)
{
    ProjectEditorSettings state;
    const auto path = ProjectEditorSettingsPath(sceneDirectory);
    if (!std::filesystem::exists(path))
        return state;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open editor settings: " + path.string());
        nlohmann::json data;
        input >> data;
        if (!data.is_object() || data.value("version", 0) != 1)
            throw std::runtime_error("Editor settings must use version 1.");
        const auto readFlag = [&data](const char* name, bool& value) {
            if (!data.contains(name))
                return;
            if (!data[name].is_boolean())
                throw std::runtime_error(std::string("Editor setting must be a boolean: ") + name);
            value = data[name].get<bool>();
        };
        readFlag("show_scene_window", state.showSceneWindow);
        readFlag("show_scene_settings_window", state.showSceneSettingsWindow);
        readFlag("show_scene_manager_window", state.showSceneManagerWindow);
        readFlag("show_ui_manager_window", state.showUIManagerWindow);
        readFlag("show_sound_manager_window", state.showSoundManagerWindow);
        readFlag("show_inventory_item_window", state.showInventoryItemWindow);
        readFlag("show_object_list_window", state.showObjectListWindow);
        readFlag("show_variable_manager_window", state.showVariableManagerWindow);
        readFlag("show_game_objects_window", state.showGameObjectsWindow);
        readFlag("show_game_view_window", state.showGameViewWindow);
        readFlag("show_build_output_window", state.showBuildOutputWindow);
        readFlag("show_debug_log_window", state.showDebugLogWindow);
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Cannot load project editor settings: %s", e.what());
    }
    return state;
}

void SaveProjectEditorSettings(const std::filesystem::path& sceneDirectory,
                               const ProjectEditorSettings& state)
{
    const auto path = ProjectEditorSettingsPath(sceneDirectory);
    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Cannot write editor settings: " + path.string());
        output << nlohmann::json{
            {"version", 1},
            {"show_scene_window", state.showSceneWindow},
            {"show_scene_settings_window", state.showSceneSettingsWindow},
            {"show_scene_manager_window", state.showSceneManagerWindow},
            {"show_ui_manager_window", state.showUIManagerWindow},
            {"show_sound_manager_window", state.showSoundManagerWindow},
            {"show_inventory_item_window", state.showInventoryItemWindow},
            {"show_object_list_window", state.showObjectListWindow},
            {"show_variable_manager_window", state.showVariableManagerWindow},
            {"show_game_objects_window", state.showGameObjectsWindow},
            {"show_game_view_window", state.showGameViewWindow},
            {"show_build_output_window", state.showBuildOutputWindow},
            {"show_debug_log_window", state.showDebugLogWindow}
        }.dump(2) << '\n';
        output.flush();
        if (!output)
            throw std::runtime_error("Cannot finish writing editor settings: " + path.string());
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Cannot save project editor settings: %s", e.what());
    }
}

std::filesystem::path EditorStatePath()
{
#if defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA"))
        return std::filesystem::path(appData) / "Devilwood" / "editor_state.json";
#else
    if (const char* configHome = std::getenv("XDG_CONFIG_HOME"))
        return std::filesystem::path(configHome) / "devilwood" / "editor_state.json";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "devilwood" / "editor_state.json";
#endif
    return std::filesystem::current_path() / ".devilwood" / "editor_state.json";
}

EditorState LoadEditorState()
{
    EditorState state;
    try {
        std::ifstream input(EditorStatePath(), std::ios::binary);
        if (!input)
            return state;
        const auto data = nlohmann::json::parse(input);
                if (!data.is_object())
            return state;
        if (data.contains("last_project") && data["last_project"].is_string())
            state.lastProject = std::filesystem::u8path(data["last_project"].get<std::string>());
        if (data.contains("recent_projects") && data["recent_projects"].is_array()) {
            for (const auto& path : data["recent_projects"])
                if (path.is_string())
                    state.recentProjects.emplace_back(std::filesystem::u8path(path.get<std::string>()));
        }
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Cannot load editor project history: %s", e.what());
    }
    return state;
}

void RememberProject(EditorState& state, const ProjectConfig& project)
{
    const auto path = std::filesystem::absolute(project.FilePath()).lexically_normal();
    state.lastProject = path;
    state.recentProjects.erase(std::remove(state.recentProjects.begin(),
                                           state.recentProjects.end(), path),
                               state.recentProjects.end());
    state.recentProjects.insert(state.recentProjects.begin(), path);
    if (state.recentProjects.size() > 10)
        state.recentProjects.resize(10);
    try {
        const auto file = EditorStatePath();
        std::filesystem::create_directories(file.parent_path());
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Cannot write editor project history: " + file.string());
                nlohmann::json recent = nlohmann::json::array();
        for (const auto& entry : state.recentProjects)
            recent.push_back(entry.u8string());
        output << nlohmann::json{{"version", 1},
                                 {"last_project", path.u8string()},
                                 {"recent_projects", std::move(recent)}}.dump(2) << '\n';
        if (!output)
            throw std::runtime_error("Cannot finish writing editor project history: " + file.string());
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Cannot save editor project history: %s", e.what());
    }
}

#if defined(_WIN32)
std::optional<std::filesystem::path> ChooseProjectFolder()
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitialize = initialized == S_OK || initialized == S_FALSE;
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return std::nullopt;

    IFileOpenDialog* dialog = nullptr;
    std::optional<std::filesystem::path> selected;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog)))) {
        FILEOPENDIALOGOPTIONS options{};
        if (SUCCEEDED(dialog->GetOptions(&options)) &&
            SUCCEEDED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                                         FOS_PATHMUSTEXIST)) &&
            SUCCEEDED(dialog->Show(GetActiveWindow()))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    selected = std::filesystem::path(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (uninitialize)
        CoUninitialize();
    return selected;
}
#endif
}

int RunProjectEditor(const ProjectConfig& project,
                     std::optional<ProjectConfig>& nextProject,
                     const EditorState& editorState)
{
    AssetPaths::Set(project.ResourcesPath(), project.ShadersPath());
    InitWindow(1280,720, "Game Editor");

    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitAudioDevice();

    const auto sceneDirectory = project.ScenesPath();
    const auto projectEditorSettings = LoadProjectEditorSettings(sceneDirectory);
    const auto layoutIniPath = sceneDirectory / ".devilwood" / "editor_layout.ini";
    const auto layoutIniFilename = layoutIniPath.u8string();
    bool layoutDirectoryReady = true;
    try {
        std::filesystem::create_directories(layoutIniPath.parent_path());
    } catch (const std::exception& e) {
        layoutDirectoryReady = false;
        TraceLog(LOG_ERROR, "Cannot create project editor settings directory: %s", e.what());
    }

    UIEditor uiEditor(sceneDirectory);
    InventoryItemEditor inventoryItemEditor(sceneDirectory);
    SoundManagerEditor soundManagerEditor;
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
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (layoutDirectoryReady)
        ImGui::GetIO().IniFilename = layoutIniFilename.c_str();
    else
        ImGui::GetIO().IniFilename = nullptr;
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
    bool gameReady = false;
    try {
        Game::get_Instance().Init(sceneEditor.SceneDirectory());
        gameReady = true;
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Game initialization failed: %s", e.what());
        sceneEditor.ReportError(std::string("Game initialization failed: ") + e.what());
    }
    FrameController frameController;
    bool showSceneWindow = projectEditorSettings.showSceneWindow;
    bool showSceneSettingsWindow = projectEditorSettings.showSceneSettingsWindow;
    bool showSceneManagerWindow = projectEditorSettings.showSceneManagerWindow;
    bool showUIManagerWindow = projectEditorSettings.showUIManagerWindow;
    bool showSoundManagerWindow = projectEditorSettings.showSoundManagerWindow;
    bool showInventoryItemWindow = projectEditorSettings.showInventoryItemWindow;
    bool showObjectListWindow = projectEditorSettings.showObjectListWindow;
    bool showVariableManagerWindow = projectEditorSettings.showVariableManagerWindow;
    bool showGameObjectsWindow = projectEditorSettings.showGameObjectsWindow;
    bool showGameViewWindow = projectEditorSettings.showGameViewWindow;
    bool showBuildOutputWindow = projectEditorSettings.showBuildOutputWindow;
    bool showDebugLogWindow = projectEditorSettings.showDebugLogWindow;
    bool showRuntimeVariablesWindow = false;
    bool debugLogAutoScroll = true;
    bool debugLogShowGame = true;
    bool debugLogShowEngine = false;
    bool debugLogShowInfo = true;
    bool debugLogShowWarning = true;
    bool debugLogShowError = true;
    bool debugLogShowDebug = false;
    std::array<char, 128> debugLogFilter{};
    std::uint64_t lastDebugLogRevision = 0;

    bool exitRequested = false;
    enum class ProjectDialog { None, Open, New };
    ProjectDialog projectDialog = ProjectDialog::None;
    std::array<char, 1024> openProjectPath{};
    std::array<char, 1024> newProjectDirectory{};
    std::array<char, 256> newProjectName{};
    std::string projectDialogError;


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

    auto SaveActiveProject = [&]() {
        if (!inventoryItemEditor.SaveForRun()) {
            showInventoryItemWindow = true;
            return false;
        }
        if (!soundManagerEditor.SaveForRun()) {
            showSoundManagerWindow = true;
            return false;
        }
        if (!uiEditor.SaveForRun()) {
            showUIManagerWindow = true;
            return false;
        }
        if (!sceneEditor.SaveForRun()) {
            showSceneWindow = true;
            return false;
        }
        try {
            project.Save();
            return true;
        } catch (const std::exception& e) {
            sceneEditor.ReportError(e.what());
            showSceneWindow = true;
            return false;
        }
    };

    while (!exitRequested)

    {
        if (WindowShouldClose()) {
            if (!inventoryItemEditor.SaveForRun())
                showInventoryItemWindow = true;
            else if (!soundManagerEditor.SaveForRun())
                showSoundManagerWindow = true;
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

            if (ImGui::BeginMenu("Project")) {

                ImGui::BeginDisabled(externalRun.IsBusy() || isPlaying);
                if (ImGui::MenuItem("New Project...")) {
                    projectDialog = ProjectDialog::New;
                    projectDialogError.clear();
                    newProjectDirectory.fill('\0');
                    newProjectName.fill('\0');
                }
                                if (ImGui::MenuItem("Open Project...")) {
                    projectDialog = ProjectDialog::Open;
                    projectDialogError.clear();
                    openProjectPath.fill('\0');
                }
                if (ImGui::BeginMenu("Recent Projects", !editorState.recentProjects.empty())) {
                    for (const auto& recent : editorState.recentProjects) {
                        try {
                            if (!std::filesystem::is_regular_file(recent))
                                continue;
                            const auto recentConfig = ProjectConfig::Load(recent);
                            const std::string label = recentConfig.name + "  (" + recent.parent_path().u8string() + ")";
                            if (ImGui::MenuItem(label.c_str())) {
                                auto loaded = ProjectConfig::Load(recent);
                                if (SaveActiveProject()) {
                                    nextProject = std::move(loaded);
                                    exitRequested = true;
                                }
                            }
                        } catch (const std::exception&) {
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Save Project"))
                    SaveActiveProject();
                ImGui::EndDisabled();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {

                ImGui::MenuItem("Scenes", nullptr, &showSceneWindow);
                ImGui::MenuItem("Project Settings", nullptr, &showSceneSettingsWindow);
                ImGui::MenuItem("Scene Manager", nullptr, &showSceneManagerWindow);
                ImGui::MenuItem("UI Manager", nullptr, &showUIManagerWindow);
                ImGui::MenuItem("Sound Manager", nullptr, &showSoundManagerWindow);
                ImGui::MenuItem("Inventory Item Editor", nullptr, &showInventoryItemWindow);
                ImGui::MenuItem("Object List", nullptr, &showObjectListWindow);
                ImGui::MenuItem("Variable Manager", nullptr, &showVariableManagerWindow);
                if (isPlaying)
                    ImGui::MenuItem("Runtime Variables", nullptr,
                                    &showRuntimeVariablesWindow);
                ImGui::MenuItem("Game View", nullptr, &showGameViewWindow);
                ImGui::MenuItem("Game Objects", nullptr, &showGameObjectsWindow);
                ImGui::MenuItem("Build Output", nullptr, &showBuildOutputWindow);
                ImGui::MenuItem("Debug Log", nullptr, &showDebugLogWindow);
                ImGui::EndMenu();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(externalRun.IsBusy() || !gameReady);
            if (ImGui::Button(isPlaying ? "Stop" : "Play")) {
                if (isPlaying) {
                    isPlaying = false;
                    showRuntimeVariablesWindow = false;
                    Game::get_Instance().StopGame();
                    Global::mousePos = {-1, -1};
                } else {
                    if (!inventoryItemEditor.SaveForRun()) {
                        showInventoryItemWindow = true;
                    } else if (!soundManagerEditor.SaveForRun()) {
                        showSoundManagerWindow = true;
                    } else if (!uiEditor.SaveForRun()) {
                        showUIManagerWindow = true;
                    } else if (!sceneEditor.SaveForRun()) {

                        showSceneWindow = true;
                    } else {
                        try {
                            Game::get_Instance().StartGame();
                            isPlaying = true;
                            showRuntimeVariablesWindow = true;
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
                    if (SaveActiveProject()) {
                        if (externalRun.Start(buildDirectory, project.FilePath()))
                            showBuildOutputWindow = false;
                        else if (externalRun.HasError())
                            showBuildOutputWindow = true;
                    }
                }

                ImGui::EndDisabled();
        }
            ImGui::EndMainMenuBar();
        }

        ImGui::DockSpaceOverViewport(
            0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        if (projectDialog == ProjectDialog::Open) {

            ImGui::OpenPopup("Open Project");
            projectDialog = ProjectDialog::None;
        } else if (projectDialog == ProjectDialog::New) {
            ImGui::OpenPopup("New Project");
            projectDialog = ProjectDialog::None;
        }

        if (ImGui::BeginPopupModal("Open Project", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Enter a project folder or its devilwood.project.json file.");
                        ImGui::SetNextItemWidth(440.0f);
            ImGui::InputText("Project path", openProjectPath.data(), openProjectPath.size());
#if defined(_WIN32)
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                                if (const auto folder = ChooseProjectFolder()) {
                    const auto text = folder->u8string();
                    openProjectPath.fill('\0');
                    std::copy_n(text.begin(), std::min(text.size(), openProjectPath.size() - 1),
                                openProjectPath.begin());
                }
            }
#endif
            if (!projectDialogError.empty())
                ImGui::TextWrapped("%s", projectDialogError.c_str());
            if (ImGui::Button("Open")) {
                try {
                    const std::filesystem::path inputPath =
                        std::filesystem::u8path(openProjectPath.data());
                    if (inputPath.empty())
                        throw std::runtime_error("Choose a project folder or project file.");
                    const auto candidate = std::filesystem::is_directory(inputPath)
                        ? inputPath / ProjectConfig::FileName : inputPath;
                    if (candidate.filename() != ProjectConfig::FileName ||
                        !std::filesystem::is_regular_file(candidate))
                        throw std::runtime_error("The selected path does not contain a Devilwood project file.");
                    auto loaded = ProjectConfig::Load(candidate);
                    if (!std::filesystem::is_directory(loaded.ScenesPath()) ||
                        !std::filesystem::is_directory(loaded.ResourcesPath()) ||
                        !std::filesystem::is_directory(loaded.ShadersPath()))
                                                                        throw std::runtime_error("The project must contain its configured scenes, resource, and shaders directories.");

                                        if (SaveActiveProject()) {
                        nextProject = std::move(loaded);
                        exitRequested = true;
                        ImGui::CloseCurrentPopup();
                    }



                } catch (const std::exception& e) {
                    projectDialogError = e.what();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

                if (ImGui::BeginPopupModal("New Project", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {



            ImGui::TextWrapped("Create a project folder with the built-in resource and shader templates.");
                        ImGui::SetNextItemWidth(440.0f);
            ImGui::InputText("Project folder", newProjectDirectory.data(),
                             newProjectDirectory.size());
#if defined(_WIN32)
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                                if (const auto folder = ChooseProjectFolder()) {
                    const auto text = folder->u8string();
                    newProjectDirectory.fill('\0');
                    std::copy_n(text.begin(), std::min(text.size(), newProjectDirectory.size() - 1),
                                newProjectDirectory.begin());
                }
            }
#endif
            ImGui::SetNextItemWidth(520.0f);
            ImGui::InputText("Project name", newProjectName.data(),
                             newProjectName.size());
            if (!projectDialogError.empty())
                ImGui::TextWrapped("%s", projectDialogError.c_str());
            if (ImGui::Button("Create")) {
                try {
                    const std::filesystem::path directory =
                        std::filesystem::u8path(newProjectDirectory.data());
                    if (directory.empty())
                        throw std::runtime_error("Choose a project folder.");
                    if (std::string(newProjectName.data()).empty())
                        throw std::runtime_error("Enter a project name.");
                    if (SaveActiveProject()) {
                        auto created = ProjectConfig::Create(
                            directory, newProjectName.data(),
                            DEVILWOOD_RESOURCE_TEMPLATE_DIR,
                            DEVILWOOD_SHADER_TEMPLATE_DIR);
                        nextProject = std::move(created);
                        exitRequested = true;
                        ImGui::CloseCurrentPopup();
                    } else {
                        projectDialogError = "Save or resolve errors in the current project before switching.";
                    }
                } catch (const std::exception& e) {
                    projectDialogError = e.what();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (isPlaying && showRuntimeVariablesWindow) {
            ImGui::SetNextWindowSize(ImVec2(440, 300), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Runtime Variables", &showRuntimeVariablesWindow)) {
                ImGui::TextDisabled("Edit live values during Play (editor only); saved defaults are unchanged.");
                auto& variables = Game::get_Instance().variables;
                if (ImGui::BeginTable("RuntimeVariableValues", 4,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Scope");
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();
                    for (const auto scope : {VariableScope::Global, VariableScope::Scene}) {
                        for (const auto& definition : variables.Definitions(scope)) {
                            ImGui::PushID(scope == VariableScope::Global ? "global" : "scene");
                            ImGui::PushID(definition.id.c_str());
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(scope == VariableScope::Global
                                ? "Global" : "Scene");
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(definition.name.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(
                                VariableStore::TypeName(definition.defaultValue).c_str());
                            ImGui::TableSetColumnIndex(3);
                            const auto& value = variables.Get(scope, definition.id);
                            if (const auto* boolean = std::get_if<bool>(&value)) {
                                bool edited = *boolean;
                                if (ImGui::Checkbox("##Value", &edited)) {
                                    variables.Set(scope, definition.id, edited);
                                    TraceLog(LOG_INFO, "[GAME] Runtime variable set (%s.%s = %s)",
                                             scope == VariableScope::Global ? "Global" : "Scene",
                                             definition.name.c_str(), edited ? "true" : "false");
                                }
                            } else if (const auto* integer = std::get_if<int>(&value)) {
                                int edited = *integer;
                                ImGui::SetNextItemWidth(-1.0f);
                                if (ImGui::InputInt("##Value", &edited)) {
                                    variables.Set(scope, definition.id, edited);
                                    TraceLog(LOG_INFO, "[GAME] Runtime variable set (%s.%s = %d)",
                                             scope == VariableScope::Global ? "Global" : "Scene",
                                             definition.name.c_str(), edited);
                                }
                            } else {
                                const auto& currentText = std::get<std::string>(value);
                                std::array<char, 512> buffer{};
                                std::copy_n(currentText.begin(),
                                            std::min(currentText.size(), buffer.size() - 1),
                                            buffer.begin());
                                ImGui::SetNextItemWidth(-1.0f);
                                if (ImGui::InputText("##Value", buffer.data(), buffer.size())) {
                                    variables.Set(scope, definition.id, std::string(buffer.data()));
                                    TraceLog(LOG_INFO, "[GAME] Runtime variable set (%s.%s = \"%s\")",
                                             scope == VariableScope::Global ? "Global" : "Scene",
                                             definition.name.c_str(), buffer.data());
                                }
                            }
                            ImGui::PopID();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
                if (variables.Definitions(VariableScope::Global).empty() &&
                    variables.Definitions(VariableScope::Scene).empty())
                    ImGui::TextDisabled("No runtime variables are defined.");
            }
            ImGui::End();
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
        sceneEditor.Draw(exitRequested, !isPlaying, showSceneWindow, showObjectListWindow,
                         showVariableManagerWindow, showSceneSettingsWindow,
                         showSceneManagerWindow);
        if (!isPlaying && showUIManagerWindow)
            uiEditor.Draw(showUIManagerWindow, sceneEditor.RenderTargetWidth(),
                          sceneEditor.RenderTargetHeight());
        if (!isPlaying && showSoundManagerWindow)
            soundManagerEditor.Draw(showSoundManagerWindow);
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
            if (ImGui::Begin("Build Output", &showBuildOutputWindow)) {
                const ImVec4 statusColor = externalRun.HasError()
                    ? ImVec4(1, 0.4f, 0.4f, 1)
                    : ImVec4(0.5f, 1, 0.5f, 1);
                ImGui::TextColored(statusColor, "%s", externalRun.Status().c_str());
                ImGui::BeginChild("Build log", ImVec2(0, 0), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(externalRun.Output().c_str());
                ImGui::EndChild();
            }
            ImGui::End();
        }
        if (showDebugLogWindow) {
            ImGui::SetNextWindowSize(ImVec2(680, 260), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Debug Log", &showDebugLogWindow)) {
                if (ImGui::Button("Clear"))
                    DebugLogBuffer::Instance().Clear();
                ImGui::SameLine();
                ImGui::Checkbox("Auto-scroll", &debugLogAutoScroll);
                ImGui::SameLine();
                ImGui::Checkbox("Game", &debugLogShowGame);
                ImGui::SameLine();
                ImGui::Checkbox("Engine", &debugLogShowEngine);
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                ImGui::Checkbox("Info", &debugLogShowInfo);
                ImGui::SameLine();
                ImGui::Checkbox("Warning", &debugLogShowWarning);
                ImGui::SameLine();
                ImGui::Checkbox("Error", &debugLogShowError);
                ImGui::SameLine();
                ImGui::Checkbox("Debug", &debugLogShowDebug);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160.0f);
                ImGui::InputTextWithHint("##DebugLogFilter", "Filter...",
                                         debugLogFilter.data(), debugLogFilter.size());
                ImGui::Separator();

                std::uint64_t currentRevision = 0;
                const auto entries = DebugLogBuffer::Instance().Snapshot(currentRevision);
                std::string filterLower(debugLogFilter.data());
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (ImGui::BeginChild("DebugLogEntries", ImVec2(0, 0), ImGuiChildFlags_Borders,
                                      ImGuiWindowFlags_HorizontalScrollbar)) {
                    for (const auto& entry : entries) {
                        const bool sourceVisible =
                            (entry.isGameLog && debugLogShowGame) ||
                            (!entry.isGameLog && debugLogShowEngine);
                        if (!sourceVisible)
                            continue;
                        const bool levelVisible =
                            (entry.level <= LOG_DEBUG && debugLogShowDebug) ||
                            (entry.level == LOG_INFO && debugLogShowInfo) ||
                            (entry.level == LOG_WARNING && debugLogShowWarning) ||
                            (entry.level >= LOG_ERROR && debugLogShowError);
                        if (!levelVisible)
                            continue;
                        if (!filterLower.empty()) {
                            std::string lower = entry.text;
                            std::transform(lower.begin(), lower.end(), lower.begin(),
                                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            if (lower.find(filterLower) == std::string::npos)
                                continue;
                        }
                        ImVec4 color = entry.isGameLog
                            ? ImVec4(0.55f, 0.95f, 0.72f, 1.0f)
                            : ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
                        if (entry.level >= LOG_ERROR)
                            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                        else if (entry.level == LOG_WARNING)
                            color = ImVec4(1.0f, 0.85f, 0.35f, 1.0f);
                        else if (entry.level <= LOG_DEBUG)
                            color = ImVec4(0.6f, 0.75f, 0.9f, 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                        ImGui::TextUnformatted(entry.text.c_str());
                        ImGui::PopStyleColor();
                    }
                    if (debugLogAutoScroll && currentRevision != lastDebugLogRevision)
                        ImGui::SetScrollHereY(1.0f);
                    lastDebugLogRevision = currentRevision;
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
        rlImGuiEnd();
        EndDrawing();
        frameController.EndFrame();
    }

    ProjectEditorSettings updatedEditorSettings;
    updatedEditorSettings.showSceneWindow = showSceneWindow;
    updatedEditorSettings.showSceneSettingsWindow = showSceneSettingsWindow;
    updatedEditorSettings.showSceneManagerWindow = showSceneManagerWindow;
    updatedEditorSettings.showUIManagerWindow = showUIManagerWindow;
    updatedEditorSettings.showSoundManagerWindow = showSoundManagerWindow;
    updatedEditorSettings.showInventoryItemWindow = showInventoryItemWindow;
    updatedEditorSettings.showObjectListWindow = showObjectListWindow;
    updatedEditorSettings.showVariableManagerWindow = showVariableManagerWindow;
    updatedEditorSettings.showGameObjectsWindow = showGameObjectsWindow;
    updatedEditorSettings.showGameViewWindow = showGameViewWindow;
    updatedEditorSettings.showBuildOutputWindow = showBuildOutputWindow;
    updatedEditorSettings.showDebugLogWindow = showDebugLogWindow;
    SaveProjectEditorSettings(sceneDirectory, updatedEditorSettings);
    if (layoutDirectoryReady)
        ImGui::SaveIniSettingsToDisk(layoutIniFilename.c_str());

    sceneEditor.ShutdownNodeEditor();
    uiEditor.Shutdown();
    rlImGuiShutdown();
    previewLighting.UnloadLighting();
    Game::get_Instance().scene_stak.reset();
    Game::get_Instance().Unload();
    ResourcesLoader::UnloadFonts();
    UnloadRenderTexture(target);
    ResourcesLoader::UnloadTextures();
    UnloadShader(ResourcesLoader::lenseShader);
    UnloadShader(ResourcesLoader::lightOnly);
    UnloadShader(ResourcesLoader::basic_shader);
    UnloadShader(ResourcesLoader::glowShader);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}

int main(int argc, char* argv[])
{
    SetTraceLogCallback(EditorTraceLogCallback);
    try {
        EditorState editorState = LoadEditorState();
        ProjectConfig project;
        if (argc > 1) {
            const std::filesystem::path inputPath =
                std::filesystem::u8path(argv[1]);
            const bool isProjectFile = inputPath.filename() == ProjectConfig::FileName;
            const bool isProjectDirectory =
                std::filesystem::is_directory(inputPath) &&
                std::filesystem::exists(inputPath / ProjectConfig::FileName);
            if (isProjectFile || isProjectDirectory) {
                project = ProjectConfig::Load(inputPath);
            } else {
                const auto sceneDirectory = std::filesystem::absolute(inputPath).lexically_normal();
                project = ProjectConfig::Load(DEVILWOOD_PROJECT_DIR);
                project.root = sceneDirectory.parent_path();
                                project.scenes = sceneDirectory.filename();
            }
        } else {
            bool loadedLastProject = false;
            if (!editorState.lastProject.empty() &&
                std::filesystem::is_regular_file(editorState.lastProject)) {
                try {
                    project = ProjectConfig::Load(editorState.lastProject);
                    loadedLastProject = true;
                } catch (const std::exception& e) {
                    TraceLog(LOG_WARNING, "Cannot reopen last project: %s", e.what());
                }
            }
            if (!loadedLastProject)
                project = ProjectConfig::Load(DEVILWOOD_PROJECT_DIR);
        }
        for (;;) {
            RememberProject(editorState, project);
            std::optional<ProjectConfig> nextProject;
            const int result = RunProjectEditor(project, nextProject, editorState);

            if (!nextProject)
                return result;
            project = std::move(*nextProject);
        }

    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unable to start the editor: %s\n", error.what());
        return 1;
    }
}