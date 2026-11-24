#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

enum class VariableScope
{
    Global,
    Scene
};

using VariableValue = std::variant<bool, int, std::string>;

struct VariableDefinition
{
    std::string id;
    std::string name;
    VariableValue defaultValue = false;
};

class VariableStore
{
public:
    struct SceneState
    {
        std::vector<VariableDefinition> definitions;
        std::unordered_map<std::string, VariableValue> values;
    };

    static std::string NewVariableId();
    static std::string TypeName(const VariableValue& value);
    static VariableValue ParseValue(const nlohmann::json& value);
    static nlohmann::json SerializeValue(const VariableValue& value);
    static nlohmann::json SerializeDefinitions(
        const std::vector<VariableDefinition>& definitions);
    static std::vector<VariableDefinition> ParseDefinitions(const nlohmann::json& data);
    static std::vector<VariableDefinition> LoadGlobalDefinitions(
        const std::filesystem::path& sceneDirectory);
    static void SaveGlobalDefinitions(const std::filesystem::path& sceneDirectory,
                                      const std::vector<VariableDefinition>& definitions);
    static bool Compare(const VariableValue& left, const std::string& operation,
                        const VariableValue& right);

    void ResetGlobals(const std::vector<VariableDefinition>& definitions);
    void ResetScene(const std::vector<VariableDefinition>& definitions);
    SceneState CaptureSceneState() const;
    void RestoreSceneState(const SceneState& state);
    bool Has(VariableScope scope, const std::string& id) const;
    const VariableValue& Get(VariableScope scope, const std::string& id) const;
    void Set(VariableScope scope, const std::string& id, const VariableValue& value);
    const std::vector<VariableDefinition>& Definitions(VariableScope scope) const;

private:
    static void ValidateDefinitions(const std::vector<VariableDefinition>& definitions);
    std::unordered_map<std::string, VariableValue>& Values(VariableScope scope);
    const std::unordered_map<std::string, VariableValue>& Values(VariableScope scope) const;

    std::unordered_map<std::string, VariableValue> globals;
    std::unordered_map<std::string, VariableValue> scene;
    std::vector<VariableDefinition> globalDefinitions;
    std::vector<VariableDefinition> sceneDefinitions;
};
