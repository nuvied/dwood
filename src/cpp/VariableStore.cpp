#include "VariableStore.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace
{
bool HasNonWhitespace(const std::string& name)
{
    return std::any_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isspace(c) == 0;
    });
}

VariableValue ParseTypedValue(const std::string& type, const nlohmann::json& value)
{
    if (type == "bool" && value.is_boolean())
        return value.get<bool>();
    if (type == "int" && value.is_number_integer()) {
        if (value.is_number_unsigned()) {
            const auto number = value.get<std::uint64_t>();
            if (number > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                throw std::runtime_error("Integer variable default is out of range.");
            return static_cast<int>(number);
        }
        const auto number = value.get<std::int64_t>();
        if (number < std::numeric_limits<int>::min() ||
            number > std::numeric_limits<int>::max())
            throw std::runtime_error("Integer variable default is out of range.");
        return static_cast<int>(number);
    }
    if (type == "string" && value.is_string())
        return value.get<std::string>();
    throw std::runtime_error("Variable default value does not match its type.");
}

nlohmann::json ValueToJson(const VariableValue& value)
{
    return std::visit([](const auto& typedValue) -> nlohmann::json {
        return typedValue;
    }, value);
}

std::filesystem::path GlobalVariablesPath(const std::filesystem::path& sceneDirectory)
{
    return sceneDirectory / ".devilwood" / "global_variables.json";
}
}

std::string VariableStore::NewVariableId()
{
    static std::uint64_t nextId = 0;
    std::ostringstream result;
    result << "variable_" << std::hex
           << std::chrono::steady_clock::now().time_since_epoch().count()
           << '_' << nextId++;
    return result.str();
}

std::string VariableStore::TypeName(const VariableValue& value)
{
    if (std::holds_alternative<bool>(value))
        return "bool";
    if (std::holds_alternative<int>(value))
        return "int";
    return "string";
}

VariableValue VariableStore::ParseValue(const nlohmann::json& value)
{
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return ParseTypedValue("int", value);
    if (value.is_string())
        return value.get<std::string>();
    throw std::runtime_error("Variable value must be a bool, int, or string.");
}

nlohmann::json VariableStore::SerializeValue(const VariableValue& value)
{
    return ValueToJson(value);
}

nlohmann::json VariableStore::SerializeDefinitions(
    const std::vector<VariableDefinition>& definitions)
{
    ValidateDefinitions(definitions);
    auto serialized = nlohmann::json::array();
    for (const auto& definition : definitions) {
        serialized.push_back({
            {"id", definition.id},
            {"name", definition.name},
            {"type", TypeName(definition.defaultValue)},
            {"default", ValueToJson(definition.defaultValue)}
        });
    }
    return serialized;
}

std::vector<VariableDefinition> VariableStore::ParseDefinitions(const nlohmann::json& data)
{
    if (!data.is_array())
        throw std::runtime_error("Variables must be an array.");
    std::vector<VariableDefinition> definitions;
    definitions.reserve(data.size());
    for (const auto& value : data) {
        if (!value.is_object() || !value.contains("id") || !value["id"].is_string() ||
            !value.contains("name") || !value["name"].is_string() ||
            !value.contains("type") || !value["type"].is_string() ||
            !value.contains("default"))
            throw std::runtime_error("Each variable needs an ID, name, type, and default value.");
        const auto type = value["type"].get<std::string>();
        if (type != "bool" && type != "int" && type != "string")
            throw std::runtime_error("Variable type must be bool, int, or string.");
        definitions.push_back({
            value["id"].get<std::string>(),
            value["name"].get<std::string>(),
            ParseTypedValue(type, value["default"])
        });
    }
    ValidateDefinitions(definitions);
    return definitions;
}

std::vector<VariableDefinition> VariableStore::LoadGlobalDefinitions(
    const std::filesystem::path& sceneDirectory)
{
    const auto path = GlobalVariablesPath(sceneDirectory);
    if (!std::filesystem::exists(path))
        return {};
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open global variables: " + path.string());
    nlohmann::json data;
    try {
        input >> data;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid global variables file: " + std::string(e.what()));
    }
    if (!data.is_object() || data.value("version", 0) != 1 ||
        !data.contains("variables"))
        throw std::runtime_error("Global variables file must contain version 1 and variables.");
    return ParseDefinitions(data["variables"]);
}

void VariableStore::SaveGlobalDefinitions(
    const std::filesystem::path& sceneDirectory,
    const std::vector<VariableDefinition>& definitions)
{
    const auto path = GlobalVariablesPath(sceneDirectory);
    const nlohmann::json data = {
        {"version", 1},
        {"variables", SerializeDefinitions(definitions)}
    };
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot write global variables: " + path.string());
    output << data.dump(2) << '\n';
    output.flush();
    if (!output)
        throw std::runtime_error("Cannot finish writing global variables: " + path.string());
}

bool VariableStore::Compare(const VariableValue& left, const std::string& operation,
                            const VariableValue& right)
{
    if (left.index() != right.index())
        throw std::runtime_error("Variable condition values must have matching types.");
    if (std::holds_alternative<bool>(left)) {
        if (operation == "==" || operation == "!=") {
            const bool equal = std::get<bool>(left) == std::get<bool>(right);
            return operation == "==" ? equal : !equal;
        }
    } else if (std::holds_alternative<int>(left)) {
        const int lhs = std::get<int>(left);
        const int rhs = std::get<int>(right);
        if (operation == "==") return lhs == rhs;
        if (operation == "!=") return lhs != rhs;
        if (operation == "<") return lhs < rhs;
        if (operation == "<=") return lhs <= rhs;
        if (operation == ">") return lhs > rhs;
        if (operation == ">=") return lhs >= rhs;
    } else {
        const auto& lhs = std::get<std::string>(left);
        const auto& rhs = std::get<std::string>(right);
        if (operation == "==") return lhs == rhs;
        if (operation == "!=") return lhs != rhs;
    }
    throw std::runtime_error("Variable comparison operator is not supported for this type.");
}

void VariableStore::ResetGlobals(const std::vector<VariableDefinition>& definitions)
{
    ValidateDefinitions(definitions);
    std::unordered_map<std::string, VariableValue> updated;
    for (const auto& definition : definitions)
        updated.emplace(definition.id, definition.defaultValue);
    globals = std::move(updated);
    globalDefinitions = definitions;
}

void VariableStore::ResetScene(const std::vector<VariableDefinition>& definitions)
{
    ValidateDefinitions(definitions);
    std::unordered_map<std::string, VariableValue> updated;
    for (const auto& definition : definitions)
        updated.emplace(definition.id, definition.defaultValue);
    scene = std::move(updated);
    sceneDefinitions = definitions;
}

VariableStore::SceneState VariableStore::CaptureSceneState() const
{
    return {sceneDefinitions, scene};
}

void VariableStore::RestoreSceneState(const SceneState& state)
{
    ValidateDefinitions(state.definitions);
    if (state.values.size() != state.definitions.size())
        throw std::runtime_error("Scene variable snapshot does not match its definitions.");
    for (const auto& definition : state.definitions) {
        const auto value = state.values.find(definition.id);
        if (value == state.values.end() ||
            value->second.index() != definition.defaultValue.index())
            throw std::runtime_error("Scene variable snapshot contains an invalid value.");
    }
    sceneDefinitions = state.definitions;
    scene = state.values;
}

bool VariableStore::Has(VariableScope scope, const std::string& id) const
{
    return Values(scope).find(id) != Values(scope).end();
}

const VariableValue& VariableStore::Get(VariableScope scope, const std::string& id) const
{
    const auto& values = Values(scope);
    const auto value = values.find(id);
    if (value == values.end())
        throw std::runtime_error("Variable is not defined in the selected scope: " + id);
    return value->second;
}

void VariableStore::Set(VariableScope scope, const std::string& id,
                        const VariableValue& value)
{
    auto& values = Values(scope);
    const auto existing = values.find(id);
    if (existing == values.end())
        throw std::runtime_error("Variable is not defined in the selected scope: " + id);
    if (existing->second.index() != value.index())
        throw std::runtime_error("Variable assignment value does not match the declared type.");
    existing->second = value;
}

const std::vector<VariableDefinition>& VariableStore::Definitions(
    VariableScope scope) const
{
    return scope == VariableScope::Global ? globalDefinitions : sceneDefinitions;
}

nlohmann::json VariableStore::SerializeValues(VariableScope scope) const
{
    nlohmann::json result = nlohmann::json::object();
    const auto& values = Values(scope);
    for (const auto& definition : Definitions(scope)) {
        const auto it = values.find(definition.id);
        if (it != values.end())
            result[definition.id] = ValueToJson(it->second);
    }
    return result;
}

void VariableStore::ApplySerializedValues(VariableScope scope, const nlohmann::json& data)
{
    if (!data.is_object())
        return;
    auto& values = Values(scope);
    for (auto it = data.begin(); it != data.end(); ++it) {
        const auto existing = values.find(it.key());
        if (existing == values.end())
            continue;
        try {
            const auto parsed = ParseValue(it.value());
            if (parsed.index() == existing->second.index())
                existing->second = parsed;
        } catch (const std::exception&) {
        }
    }
}

void VariableStore::ValidateDefinitions(
    const std::vector<VariableDefinition>& definitions)
{
    std::set<std::string> ids;
    std::set<std::string> names;
    for (const auto& definition : definitions) {
        if (definition.id.empty() || !HasNonWhitespace(definition.name))
            throw std::runtime_error("Variable IDs must be non-empty and names cannot be blank.");
        if (!ids.insert(definition.id).second)
            throw std::runtime_error("Variable IDs must be unique within their scope.");
        if (!names.insert(definition.name).second)
            throw std::runtime_error("Variable names must be unique within their scope.");
    }
}

std::unordered_map<std::string, VariableValue>& VariableStore::Values(VariableScope scope)
{
    return scope == VariableScope::Global ? globals : scene;
}

const std::unordered_map<std::string, VariableValue>& VariableStore::Values(
    VariableScope scope) const
{
    return scope == VariableScope::Global ? globals : scene;
}
