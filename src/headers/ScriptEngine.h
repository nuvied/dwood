#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class Entity;

using ScriptPropertyValue = std::variant<bool, double, std::string>;
using ScriptPropertyMap = std::map<std::string, ScriptPropertyValue>;
// Object reference properties declared with `name = ref("Transform")`, resolved to scene entities.
using ScriptObjectRefMap = std::map<std::string, Entity*>;

struct ScriptPropertyDefinition
{
    std::string name;
    ScriptPropertyValue defaultValue;
    // Non-empty for object reference properties (canonical type, e.g. "Transform", "Entity").
    std::string refType;
};

class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    // Returns the canonical reference type name ("Entity", "Transform", "Sprite", ...) or empty if unknown.
    static std::string NormalizeRefType(const std::string& type);
    // Scene JSON component key required by a reference type; empty for "Entity" (any object).
    static std::string RefTypeComponentKey(const std::string& canonicalType);
    static bool IsScriptAssetName(const std::string& name);
    static std::vector<std::string> AvailableScripts();
    static void EnsureApiStubs();
    static std::vector<ScriptPropertyDefinition> InspectProperties(
        const std::string& scriptAsset, std::string* error = nullptr);
    static std::filesystem::path CreateTemplateScript(const std::string& scriptId);

    void Reset();
    void Shutdown();

    int CreateInstance(const std::string& scriptAsset, Entity* owner,
                       const ScriptPropertyMap& properties = {},
                       const ScriptObjectRefMap& objectRefs = {});
    void DestroyInstance(int instanceId);
    void CallStart(int instanceId, Entity* owner);
    void CallUpdate(int instanceId, Entity* owner, float dt);
    bool HasClickHandler(int instanceId) const;
    bool CallClick(int instanceId, Entity* owner);
    bool RunScriptAction(const std::string& scriptAsset, const std::string& functionName,
                         Entity* owner);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
