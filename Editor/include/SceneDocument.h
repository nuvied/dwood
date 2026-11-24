#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "VariableStore.h"

enum class SceneType
{
    Main,
    Popup
};

class SceneDocument
{
public:
    static std::vector<std::filesystem::path> List(const std::filesystem::path& directory);
    static SceneDocument Create(const std::filesystem::path& directory, const std::string& id);
    static SceneDocument Open(const std::filesystem::path& path);

    const std::string& Name() const { return name; }
    const std::filesystem::path& Path() const { return path; }
    SceneType Type() const;
    std::optional<std::string> ParentScene() const;
    std::size_t ObjectCount() const { return data.at("objects").size(); }
    float BaseDarkness() const;
    std::vector<VariableDefinition> Variables() const;
    void SetVariables(const std::vector<VariableDefinition>& variables,
                      bool coalesce = false);
    bool IsDirty() const { return !saved || name != savedName || data != savedData; }
    bool CanUndo() const;
    bool CanRedo() const;
    bool Undo();
    bool Redo();
    void EndUndoGroup();
    void SetName(const std::string& value, bool coalesce = false);
    void SetType(SceneType type, const std::string& parentScene = {},
                 bool coalesce = false);
    void SetBaseDarkness(float value, bool coalesce = false);
    nlohmann::json Object(std::size_t index) const;
    void AddObject();
    void AddObject(const nlohmann::json& object);
    void SetObject(std::size_t index, const nlohmann::json& object, bool coalesce = false);
    std::optional<std::size_t> Parent(std::size_t index) const;
    void SetParent(std::size_t index, std::optional<std::size_t> parent);
    void RemoveObject(std::size_t index);
    void Save();

private:
    struct Snapshot
    {
        nlohmann::json data;
        std::string name;
    };

    void ResetHistory();
    void RecordHistory(bool coalesce = false);
    void RestoreHistory();

    std::filesystem::path path;
    nlohmann::json data;
    nlohmann::json savedData;
    std::string name;
    std::string savedName;
    std::string savedContents;
    bool saved = false;
    std::vector<Snapshot> history;
    std::size_t historyPosition = 0;
    bool undoGroupOpen = false;
};
