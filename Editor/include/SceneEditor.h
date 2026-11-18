#pragma once

#include "SceneDocument.h"
#include "raylib.h"

#include <array>
#include <optional>
#include <string>

class Screen;
class UIEditor;
namespace ax { namespace NodeEditor { struct EditorContext; } }

class SceneEditor
{
public:
    SceneEditor(std::filesystem::path directory, UIEditor& uiEditor);
    ~SceneEditor();
    SceneEditor(const SceneEditor&) = delete;
    SceneEditor& operator=(const SceneEditor&) = delete;
    void Draw(bool& exitRequested, bool sceneMode, bool showPanel, bool showSettings,
              bool showManager);
    void DrawScene(Screen* lighting) const;
    bool RequestExit();
    bool SaveForRun();
    const SceneDocument* Current() const;
    const std::filesystem::path& SceneDirectory() const { return directory; }
    int RenderTargetWidth() const;
    int RenderTargetHeight() const;
    void ReportError(const std::string& error);
    void ShutdownNodeEditor();

private:
    enum class Action { None, New, Open, Exit };

    void LoadSettings();
    void SaveSettings();
    void Refresh();
    void Request(Action action, const std::filesystem::path& path = {});
    void Execute(bool& exitRequested);
    void HandleSceneInput();
    void DrawInteractionGraph();
    bool Save();

    std::filesystem::path directory;
    UIEditor& uiEditor;
    std::vector<std::filesystem::path> files;
    std::optional<SceneDocument> current;
    std::array<char, 65> idInput{};
    std::array<char, 128> nameInput{};
    std::array<char, 128> objectNameInput{};
    std::array<char, 1024> textInput{};
    std::optional<std::size_t> selectedObject;
    std::optional<std::size_t> draggingObject;
    std::optional<std::size_t> contextObject;
    std::optional<nlohmann::json> copiedObject;
    bool openObjectContextMenu = false;
    float dragOffsetX = 0;
    float dragOffsetY = 0;
    Vector2 sceneOffset{};
    float sceneZoom = 1.0f;
    bool panningScene = false;
    bool lightPreview = true;
    Vector2 panStartMouse{};
    Vector2 panStartOffset{};
    int renderWidth = 1024;
    int renderHeight = 512;
    int appliedRenderWidth = 1024;
    int appliedRenderHeight = 512;
    bool settingsDirty = false;
    std::string message;
    bool messageIsError = false;
    Action pending = Action::None;
    std::filesystem::path pendingPath;
    bool openConfirmation = false;
    bool showInteractionGraph = false;
    ax::NodeEditor::EditorContext* nodeEditor = nullptr;
    std::filesystem::path nodeScene;
    std::optional<std::size_t> nodeObject;
    std::size_t positionedActionNodes = 0;
    bool initializeNodeView = false;
};
