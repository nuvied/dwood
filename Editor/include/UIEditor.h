#pragma once

#include "ProjectUI.h"
#include "SceneDocument.h"
#include "raylib.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class UIEditor
{
public:
    explicit UIEditor(std::filesystem::path sceneDirectory);
    ~UIEditor();
    UIEditor(const UIEditor&) = delete;
    UIEditor& operator=(const UIEditor&) = delete;

    void Draw(bool& open, int targetWidth, int targetHeight);
    void Shutdown();
    bool SaveForRun();
    bool HasManager() const { return document.has_value(); }
    std::vector<std::pair<std::string, std::string>> Targets(bool groupsOnly = false) const;
    std::vector<std::pair<int, std::string>> InventoryTargets() const;

private:
    bool Save();
    void Create();
    void AddElement(const char* type);
    void DrawPreview(int width, int height);

    std::filesystem::path sceneDirectory;
    std::optional<SceneDocument> document;
    std::optional<std::size_t> selected;
    std::optional<std::size_t> dragging;
    Vector2 dragOffset{};
    float previewZoom = 1.0f;
    Vector2 previewOffset{};
    bool panningPreview = false;
    Vector2 panStartMouse{};
    Vector2 panStartOffset{};
    RenderTexture2D preview{};
    std::unique_ptr<ProjectUI> cachedPreviewUI;
    std::uint64_t cachedPreviewRevision = 0;
    std::optional<std::size_t> cachedPreviewSubtitle;
    std::string message;
    bool messageIsError = false;
};

class SceneDocument;
bool DrawUIAction(nlohmann::json& action, const UIEditor& uiEditor,
                  const SceneDocument* scene = nullptr,
                  const std::vector<VariableDefinition>* globalVariables = nullptr,
                  const std::vector<VariableDefinition>* sceneVariables = nullptr,
                  int selectedBranch = -1,
                  bool selectIfElseTab = false);
bool DrawAddInteractionAction(nlohmann::json& actions, const UIEditor& uiEditor,
                              const SceneDocument* scene, bool allowChangeScene,
                              bool enabled);
