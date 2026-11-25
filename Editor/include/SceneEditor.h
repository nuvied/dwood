#pragma once

#include "SceneDocument.h"
#include "SceneObject.h"
#include "raylib.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

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
    void Draw(bool& exitRequested, bool sceneMode, bool& showPanel, bool& showObjectList,
              bool& showVariableManager, bool showSettings, bool showManager);
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
    void LoadEditorSettings();
    void SaveEditorSettings() const;
    void Refresh();
    void Request(Action action, const std::filesystem::path& path = {});
    void Execute(bool& exitRequested);
    void HandleSceneInput();
    void DrawInteractionGraph();
    void DrawVariableManager(bool& showWindow, bool sceneMode);
    void LoadGlobalVariables();
    bool SaveGlobalVariables();
    bool CommitSceneVariables(bool coalesce = false);
    void UpdateSceneVariableDraft();
    bool Save();

    std::filesystem::path directory;
    UIEditor& uiEditor;
    std::vector<VariableDefinition> globalVariables;
    std::vector<VariableDefinition> sceneVariableDraft;
    std::filesystem::path sceneVariableDraftPath;
    std::array<char, 128> newGlobalVariableName{};
    std::array<char, 128> newSceneVariableName{};
    bool globalVariablesDirty = false;
    bool globalVariablesLoadFailed = false;
    bool sceneVariablesDirty = false;
    std::string variableMessage;
    bool variableMessageIsError = false;
    std::vector<std::filesystem::path> files;
    struct CachedSceneFile
    {
        std::filesystem::path path;
        SceneType type = SceneType::Main;
        std::optional<std::string> parentScene;
        std::string error;
    };
    std::vector<CachedSceneFile> cachedSceneFiles;
    mutable std::vector<nlohmann::json> cachedPreviewData;
    mutable EditorSceneGraph cachedPreviewGraph;
    mutable std::filesystem::path cachedPreviewPath;
    mutable std::uint64_t cachedPreviewRevision = 0;
    mutable bool cachedPreviewValid = false;
    bool EnsurePreviewGraph() const;
    std::optional<SceneDocument> current;
    std::array<char, 65> idInput{};
    std::array<char, 128> nameInput{};
    std::array<char, 128> objectNameInput{};
    std::array<char, 1024> textInput{};
    std::optional<std::size_t> selectedObject;
    std::optional<std::size_t> draggingObject;
    enum class ResizeHandle
    {
        None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
    };
    struct ResizeDragState
    {
        std::size_t objectIndex = 0;
        ResizeHandle handle = ResizeHandle::None;
        Rectangle startBounds{};
        Vector2 startPosition{};
        Vector2 startScale{1.0f, 1.0f};
        Vector2 startRectSize{};
        Vector2 startColliderSize{};
        Vector2 startMouseWorld{};
        bool hasRectangle = false;
        bool hasColliderOnly = false;
    };
    std::optional<ResizeDragState> resizingObject;
    ResizeHandle hoveredResizeHandle = ResizeHandle::None;
    std::optional<std::size_t> contextObject;
    struct ScriptRefPickTarget
    {
        std::size_t objectIndex = 0;
        std::string propertyName;
        std::string refType;
    };
    std::optional<ScriptRefPickTarget> pickingScriptRef;
    std::optional<nlohmann::json> copiedObject;
    bool openObjectContextMenu = false;
    bool focusCurrentSceneTab = false;
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
    Vector2 lensOffset{0.0f, 0.0f};
    Vector2 appliedLensOffset{0.0f, 0.0f};
    bool lensDragToMove = false;
    bool appliedLensDragToMove = false;
    bool textureFiltering = false;
    bool appliedTextureFiltering = false;
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
    std::optional<std::uintptr_t> selectedNodeForDetails;
    std::string nodeSettingsFile;
    std::size_t positionedActionNodes = 0;
    std::unordered_set<std::uintptr_t> positionedBranchNodes;
    bool initializeNodeView = false;
    std::uintptr_t contextActionNode = 0;
    Vector2 actionContextPosition{};
};
