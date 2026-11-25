#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#include <shellapi.h>
#undef DrawTextEx
#undef DrawText
#undef LoadImage
#undef ShowCursor
#undef CloseWindow
#undef Rectangle
#endif

#include "SceneEditor.h"
#include "UIEditor.h"

#include "imgui.h"
#include <imgui-node-editor/imgui_node_editor.h>
#include "Game.hpp"
#include "SceneManager.h"
#include "SceneObject.h"
#include "Screen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>

namespace ed = ax::NodeEditor;

namespace
{
constexpr int MinRenderSize = 1;
constexpr int MaxRenderSize = 8192;

bool OpenScriptInExternalEditor(const std::string& scriptAsset, std::string& error)
{
    if (!ScriptEngine::IsScriptAssetName(scriptAsset)) {
        error = "Choose a valid .lua script file first.";
        return false;
    }
    ScriptEngine::EnsureApiStubs();
    const auto scriptPath = std::filesystem::absolute(AssetPaths::Script(scriptAsset)).lexically_normal();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(scriptPath, ec)) {
        error = "Script file does not exist: " + scriptPath.string();
        return false;
    }
#if defined(_WIN32)
    auto stripTrailingSlash = [](std::wstring value) {
        while (value.size() > 3 && (value.back() == L'\\' || value.back() == L'/'))
            value.pop_back();
        return value;
    };
    const std::wstring scriptDir = stripTrailingSlash(scriptPath.parent_path().wstring());
    const std::wstring scriptFile = scriptPath.wstring();

    auto launchProcess = [&scriptDir](const std::filesystem::path& exe,
                                      const std::wstring& commandLine) -> bool {
        SetEnvironmentVariableW(L"ELECTRON_RUN_AS_NODE", nullptr);
        std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
        cmd.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, scriptDir.c_str(), &startup, &process))
            return false;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    };

    std::vector<std::filesystem::path> vscodeCandidates;
    if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        vscodeCandidates.push_back(
            std::filesystem::path(localAppData) / "Programs" / "Microsoft VS Code" / "Code.exe");
    vscodeCandidates.push_back("C:/Program Files/Microsoft VS Code/Code.exe");
    for (const auto& candidate : vscodeCandidates) {
        if (std::filesystem::is_regular_file(candidate, ec)) {
            const std::wstring cmd =
                L"\"" + candidate.wstring() + L"\" \"" + scriptDir + L"\" \"" + scriptFile + L"\"";
            if (launchProcess(candidate, cmd)) {
                TraceLog(LOG_INFO, "[GAME] Opened script in VS Code: %s", scriptAsset.c_str());
                return true;
            }
        }
    }

    const auto shellResult = reinterpret_cast<std::intptr_t>(
        ShellExecuteW(nullptr, L"open", scriptFile.c_str(), nullptr,
                      scriptDir.c_str(), SW_SHOWNORMAL));
    if (shellResult > 32) {
        TraceLog(LOG_INFO, "[GAME] Opened script: %s", scriptAsset.c_str());
        return true;
    }

    const std::filesystem::path notepad = "C:/Windows/System32/notepad.exe";
    if (std::filesystem::is_regular_file(notepad, ec) &&
        launchProcess(notepad, L"\"" + notepad.wstring() + L"\" \"" + scriptFile + L"\"")) {
        TraceLog(LOG_INFO, "[GAME] Opened script in Notepad: %s", scriptAsset.c_str());
        return true;
    }

    error = "Unable to open script in external editor.";
    return false;
#else
    error = "Open script in external editor is currently supported on Windows.";
    return false;
#endif
}

std::optional<nlohmann::json> LoadInventoryItemSprite(
    const std::filesystem::path& sceneDirectory, int itemId)
{
    const auto path = sceneDirectory / ".devilwood" / "inventory_items.json";
    if (!std::filesystem::exists(path))
        return std::nullopt;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return std::nullopt;
        const auto data = nlohmann::json::parse(input);
        if (!data.contains("items") || !data["items"].is_array())
            return std::nullopt;
        for (const auto& item : data["items"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_number_integer() || item["id"].get<int>() != itemId)
                continue;
            if (!item.contains("icon") || !item["icon"].is_object())
                return std::nullopt;
            const auto& icon = item["icon"];
            if (!icon.contains("source") || !icon["source"].is_object())
                return std::nullopt;
            const auto& source = icon["source"];
            if (!source.contains("x") || !source["x"].is_number() ||
                !source.contains("y") || !source["y"].is_number() ||
                !source.contains("width") || !source["width"].is_number() ||
                !source.contains("height") || !source["height"].is_number())
                return std::nullopt;
            const int x = static_cast<int>(std::round(source["x"].get<double>()));
            const int y = static_cast<int>(std::round(source["y"].get<double>()));
            const int width = static_cast<int>(std::round(source["width"].get<double>()));
            const int height = static_cast<int>(std::round(source["height"].get<double>()));
            if (x < 0 || y < 0 || width <= 0 || height <= 0)
                return std::nullopt;
            nlohmann::json sprite = {
                {"atlas", icon.value("atlas", std::string("inv_items.png"))},
                {"source", {{"x", x}, {"y", y}, {"width", width}, {"height", height}}}
            };
            if (icon.contains("frame") && icon["frame"].is_string() &&
                !icon["frame"].get<std::string>().empty())
                sprite["frame"] = icon["frame"].get<std::string>();
            return sprite;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

bool DrawActionTarget(nlohmann::json& target, const std::vector<std::filesystem::path>& scenes)
{
    const auto selectedScene = target.get<std::string>();
    const std::string label = selectedScene.empty() ? "Choose scene" : selectedScene;
    const bool registered = std::any_of(scenes.begin(), scenes.end(),
        [&selectedScene](const auto& scene) { return scene.filename().string() == selectedScene; });
    if (!selectedScene.empty() && !registered)
        ImGui::TextWrapped("Target scene is not registered.");
    bool changed = false;
    if (ImGui::BeginCombo("Scene", label.c_str())) {
        for (const auto& scene : scenes) {
            const auto filename = scene.filename().string();
            if (ImGui::Selectable(filename.c_str(), filename == selectedScene)) {
                target = filename;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool DrawUIElementTarget(
    nlohmann::json& target,
    const std::vector<std::pair<std::string, std::string>>& targets,
    const char* label)
{
    const auto selectedId = target.get<std::string>();
    const auto selected = std::find_if(targets.begin(), targets.end(),
        [&selectedId](const auto& entry) { return entry.first == selectedId; });
    const auto preview = selected == targets.end()
        ? (selectedId.empty() ? std::string("Choose UI") : "Missing: " + selectedId)
        : selected->second + " (" + selected->first + ")";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const auto& entry : targets) {
            const auto option = entry.second + " (" + entry.first + ")";
            if (ImGui::Selectable(option.c_str(), entry.first == selectedId)) {
                target = entry.first;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool IsAudioAction(const std::string& type)
{
    return type == "PlayAudio" || type == "PauseAudio" || type == "StopAudio";
}

bool IsManagedSoundAction(const std::string& type)
{
    return type == "SoundOn" || type == "SoundOff" ||
           type == "SoundFadeIn" || type == "SoundFadeOut";
}

bool IsManagedSoundFadeAction(const std::string& type)
{
    return type == "SoundFadeIn" || type == "SoundFadeOut";
}

struct ActionChoice
{
    const char* type;
    const char* label;
    const char* category;
    bool sceneOnly = false;
    bool requiresChangeScene = false;
};

const ActionChoice ActionChoices[] = {
    {"ChangeScene", "Change scene", "Scene", true, true},
    {"OpenPopupScene", "Open popup scene", "Scene", true},
    {"ClosePopupScene", "Close popup scene", "Scene", true},
    {"ShowObject", "Show object", "Scene", true},
    {"HideObject", "Hide object", "Scene", true},
    {"DisableHotspot", "Disable hotspot", "Scene", true},
    {"ChangeHotspotSetting", "Change hotspot setting", "Scene", true},
    {"RunScript", "Run Lua script", "Scene", true},
    {"PlayAudio", "Play scene audio", "Scene", true},
    {"PauseAudio", "Pause scene audio", "Scene", true},
    {"StopAudio", "Stop scene audio", "Scene", true},
    {"ShowUI", "Show UI", "UI"},
    {"ShowSubtitle", "Show subtitle", "UI"},
    {"HideUI", "Hide UI", "UI"},
    {"ToggleUI", "Toggle UI", "UI"},
    {"EnableUI", "Enable UI", "UI"},
    {"DisableUI", "Disable UI", "UI"},
    {"FadeInUI", "Fade UI in", "UI"},
    {"FadeOutUI", "Fade UI out", "UI"},
    {"ToggleFadeUI", "Toggle UI with fade", "UI"},
    {"AddItem", "Add inventory item", "Inventory"},
    {"RemoveItem", "Remove inventory item", "Inventory"},
    {"SoundOn", "Turn managed sound on", "Sound Manager"},
    {"SoundOff", "Turn managed sound off", "Sound Manager"},
    {"SoundFadeIn", "Fade managed sound in", "Sound Manager"},
    {"SoundFadeOut", "Fade managed sound out", "Sound Manager"},
    {"SetVariable", "Set variable", "Variables", true},
    {"IfElse", "If / Else", "Variables", true},
    {"SaveGame", "Save game", "Save"},
    {"LoadGame", "Load game", "Save"},
    {"DeleteSave", "Delete save", "Save"},
    {"EnableLens", "Enable lens", "Lens"},
    {"DisableLens", "Disable lens", "Lens"},
    {"ToggleLens", "Toggle lens", "Lens"}
};

const char* ActionLabel(const std::string& type)
{
    const auto selected = std::find_if(std::begin(ActionChoices), std::end(ActionChoices),
        [&type](const ActionChoice& choice) { return type == choice.type; });
    return selected == std::end(ActionChoices) ? "Unknown" : selected->label;
}

bool DrawVariableDefinitions(std::vector<VariableDefinition>& definitions,
                             std::array<char, 128>& newName, const char* scope,
                             std::string& message, bool& messageIsError)
{
    bool changed = false;
    std::optional<std::size_t> deleteIndex;
    if (ImGui::BeginTable(scope, 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Default");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            auto& definition = definitions[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::array<char, 128> name{};
            std::copy_n(definition.name.begin(),
                        std::min(definition.name.size(), name.size() - 1), name.begin());
            if (ImGui::InputText("##Name", name.data(), name.size())) {
                definition.name = name.data();
                changed = true;
            }

            ImGui::TableSetColumnIndex(1);
            const std::string type = VariableStore::TypeName(definition.defaultValue);
            if (ImGui::BeginCombo("##Type", type.c_str())) {
                const char* types[] = {"bool", "int", "string"};
                for (const char* nextType : types) {
                    if (ImGui::Selectable(nextType, type == nextType)) {
                        if (std::string(nextType) == "bool")
                            definition.defaultValue = false;
                        else if (std::string(nextType) == "int")
                            definition.defaultValue = 0;
                        else
                            definition.defaultValue = std::string{};
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(2);
            if (auto* value = std::get_if<bool>(&definition.defaultValue)) {
                if (ImGui::Checkbox("##Default", value))
                    changed = true;
            } else if (auto* value = std::get_if<int>(&definition.defaultValue)) {
                if (ImGui::InputInt("##Default", value))
                    changed = true;
            } else {
                auto& stringValue = std::get<std::string>(definition.defaultValue);
                std::array<char, 256> text{};
                std::copy_n(stringValue.begin(),
                            std::min(stringValue.size(), text.size() - 1),
                            text.begin());
                if (ImGui::InputText("##Default", text.data(), text.size())) {
                    stringValue = text.data();
                    changed = true;
                }
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("Remove"))
                deleteIndex = i;
            ImGui::PopID();
            if (deleteIndex)
                break;
        }
        ImGui::EndTable();
    }

    if (deleteIndex) {
        definitions.erase(definitions.begin() + static_cast<std::ptrdiff_t>(*deleteIndex));
        changed = true;
    }

    ImGui::PushID(scope);
    ImGui::SetNextItemWidth(-1.0f);
    const bool submitByKeyboard = ImGui::InputTextWithHint(
        "##NewVariableName", "New variable", newName.data(), newName.size(),
        ImGuiInputTextFlags_EnterReturnsTrue);
    const bool submitByButton = ImGui::Button("Add variable");
    if (submitByKeyboard || submitByButton) {
        const std::string name = newName.data();
        if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
            message = "Enter a variable name before adding it.";
            messageIsError = true;
        } else if (std::any_of(definitions.begin(), definitions.end(),
                               [&name](const auto& definition) {
                                   return definition.name == name;
                               })) {
            message = "A variable with this name already exists in the selected scope.";
            messageIsError = true;
        } else {
            definitions.push_back({VariableStore::NewVariableId(), name, false});
            newName.fill('\0');
            message.clear();
            messageIsError = false;
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

bool DrawActionTypeCombo(const char* label, std::string& type,
                         bool sceneActions, bool allowChangeScene)
{
    bool changed = false;
    const char* preview = type.empty() ? "Select action type" : ActionLabel(type);
    if (ImGui::BeginCombo(label, preview)) {
        std::string category;
        for (const auto& choice : ActionChoices) {
            if ((choice.sceneOnly && !sceneActions) ||
                (choice.requiresChangeScene && !allowChangeScene))
                continue;
            if (category != choice.category) {
                if (!category.empty())
                    ImGui::Separator();
                ImGui::TextDisabled("%s", choice.category);
                category = choice.category;
            }
            if (ImGui::Selectable(choice.label, type == choice.type)) {
                type = choice.type;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool IsActionChoiceAvailable(const ActionChoice& choice, bool sceneActions,
                             bool allowChangeScene)
{
    return (!choice.sceneOnly || sceneActions) &&
           (!choice.requiresChangeScene || allowChangeScene);
}

nlohmann::json CreateInteractionAction(const std::string& type,
                                      const UIEditor& uiEditor)
{
    nlohmann::json action = {{"type", type}};
    if (type == "ChangeScene") {
        action["scene"] = "";
    } else if (type == "OpenPopupScene") {
        action["scene"] = "";
        action["duration"] = 0.5f;
    } else if (type == "ClosePopupScene") {
        action["duration"] = 0.5f;
    } else if (type == "SetVariable") {
        action["scope"] = "global";
        action["id"] = "";
        action["value"] = false;
    } else if (type == "ShowSubtitle") {
        action["text"] = "";
    } else if (type == "IfElse") {
        action["condition"] = {
            {"scope", "global"}, {"id", ""}, {"operator", "=="}, {"value", false}
        };
        action["then"] = nlohmann::json::array();
        action["else"] = nlohmann::json::array();
    } else if (type == "ShowObject" || type == "HideObject") {
        action["object"] = nullptr;
    } else if (type == "DisableHotspot") {
        action["hotspot"] = nullptr;
    } else if (type == "ChangeHotspotSetting") {
        action["hotspot"] = nullptr;
        action["interaction"] = nullptr;
        action["check_variable"] = false;
        action["variable"] = {{"scope", "global"}, {"id", ""}};
        action["require_item"] = false;
        action["item"] = 0;
        action["consume_item"] = false;
        action["disable_on_success"] = false;
        action["fail_message"] = "";
    } else if (type == "SaveGame" || type == "LoadGame" || type == "DeleteSave") {
        action["slot"] = 1;
    } else if (type == "EnableLens" || type == "DisableLens" || type == "ToggleLens") {
        // No extra parameters needed.
    } else if (type == "RunScript") {
        const auto scripts = ScriptEngine::AvailableScripts();
        action["script"] = scripts.empty() ? std::string{} : scripts.front();
        action["function"] = "";
    } else if (IsAudioAction(type)) {
        action["audio"] = nullptr;
    } else if (type == "AddItem" || type == "RemoveItem") {
        const auto items = uiEditor.InventoryTargets();
        action["item"] = items.empty() ? 0 : items.front().first;
    } else if (IsManagedSoundAction(type)) {
        const auto& sounds = Game::get_Instance().soundManager.Sounds();
        action["sound"] = sounds.empty() ? 0 : sounds.front().id;
    } else {
        action["ui"] = "";
    }
    if (type == "FadeInUI" || type == "FadeOutUI" || type == "ToggleFadeUI" ||
        IsManagedSoundFadeAction(type))
        action["duration"] = 1.0f;
    return action;
}

std::string AudioObjectLabel(const SceneDocument& scene, std::size_t index)
{
    const auto object = scene.Object(index);
    if (!object.is_object() || !object.contains("name") || !object["name"].is_string())
        return "Invalid audio object (" + std::to_string(index) + ")";
    return object["name"].get<std::string>() + " (" + std::to_string(index) + ")";
}

bool DrawAudioActionTarget(nlohmann::json& action, const SceneDocument& scene)
{
    std::optional<std::size_t> selected;
    if (action.contains("audio") && action["audio"].is_number_integer()) {
        const auto value = action["audio"];
        if (value.is_number_unsigned())
            selected = value.get<std::size_t>();
        else if (value.get<std::int64_t>() >= 0)
            selected = static_cast<std::size_t>(value.get<std::int64_t>());
    }
    const auto label = selected && *selected < scene.ObjectCount()
        ? AudioObjectLabel(scene, *selected)
        : (selected ? std::string("Missing audio object") : std::string("Choose audio object"));
    bool changed = false;
    if (ImGui::BeginCombo("Audio object", label.c_str())) {
        if (ImGui::Selectable("None", !selected)) {
            action["audio"] = nullptr;
            changed = true;
        }
        bool hasAudio = false;
        for (std::size_t i = 0; i < scene.ObjectCount(); ++i) {
            const auto object = scene.Object(i);
            if (!object.is_object() || !object.contains("components") ||
                !object["components"].is_object() ||
                !object["components"].contains("AudioComp"))
                continue;
            hasAudio = true;
            const auto option = AudioObjectLabel(scene, i);
            if (ImGui::Selectable(option.c_str(), selected && *selected == i)) {
                action["audio"] = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
        if (!hasAudio)
            ImGui::TextWrapped("Add an Audio component to a scene object first.");
    }
    return changed;
}

bool DrawManagedSoundTarget(nlohmann::json& action)
{
    auto& manager = Game::get_Instance().soundManager;
    const int selected = action.value("sound", 0);
    const auto* sound = manager.Find(selected);
    const auto label = sound
        ? sound->name + " (" + std::to_string(sound->id) + ")"
        : std::string("Choose managed sound");
    bool changed = false;
    if (ImGui::BeginCombo("Managed sound", label.c_str())) {
        for (const auto& option : manager.Sounds()) {
            const auto optionLabel = option.name + " (" + std::to_string(option.id) + ")";
            if (ImGui::Selectable(optionLabel.c_str(), option.id == selected)) {
                action["sound"] = option.id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (manager.Sounds().empty())
        ImGui::TextWrapped("Add a sound in the Sound Manager window first.");
    return changed;
}

const VariableDefinition* DrawVariableReference(
    nlohmann::json& reference,
    const std::vector<VariableDefinition>* globalVariables,
    const std::vector<VariableDefinition>* sceneVariables,
    bool& changed)
{
    std::string scope = reference.value("scope", std::string("global"));
    if (ImGui::BeginCombo("Scope", scope == "scene" ? "Scene" : "Global")) {
        if (ImGui::Selectable("Global", scope == "global")) {
            scope = "global";
            changed = true;
        }
        if (ImGui::Selectable("Scene", scope == "scene")) {
            scope = "scene";
            changed = true;
        }
        ImGui::EndCombo();
    }
    reference["scope"] = scope;

    const auto* definitions = scope == "scene" ? sceneVariables : globalVariables;
    const std::string id = reference.value("id", std::string{});
    const VariableDefinition* selected = nullptr;
    if (definitions) {
        const auto found = std::find_if(definitions->begin(), definitions->end(),
            [&id](const auto& definition) { return definition.id == id; });
        if (found != definitions->end())
            selected = &*found;
    }
    const std::string label = selected ? selected->name
        : (id.empty() ? std::string("Choose variable") : std::string("Missing variable"));
    if (ImGui::BeginCombo("Variable", label.c_str())) {
        if (definitions) {
            for (const auto& definition : *definitions) {
                if (ImGui::Selectable(definition.name.c_str(), definition.id == id)) {
                    reference["id"] = definition.id;
                    selected = &definition;
                    changed = true;
                }
            }
        }
        ImGui::EndCombo();
    }
    if (!selected)
        ImGui::TextDisabled("Add a variable in Variable Manager first.");
    return selected;
}

bool DrawVariableValue(nlohmann::json& value, const VariableDefinition& definition)
{
    bool changed = false;
    VariableValue current;
    try {
        current = VariableStore::ParseValue(value);
        if (current.index() != definition.defaultValue.index())
            current = definition.defaultValue;
    } catch (const std::exception&) {
        current = definition.defaultValue;
    }

    if (auto* boolean = std::get_if<bool>(&current)) {
        if (ImGui::Checkbox("Value", boolean))
            changed = true;
    } else if (auto* integer = std::get_if<int>(&current)) {
        if (ImGui::InputInt("Value", integer))
            changed = true;
    } else {
        auto& string = std::get<std::string>(current);
        std::array<char, 256> text{};
        std::copy_n(string.begin(), std::min(string.size(), text.size() - 1), text.begin());
        if (ImGui::InputText("Value", text.data(), text.size())) {
            string = text.data();
            changed = true;
        }
    }
    if (changed)
        value = VariableStore::SerializeValue(current);
    return changed;
}

bool DrawVariableAssignment(nlohmann::json& action,
                            const std::vector<VariableDefinition>* globalVariables,
                            const std::vector<VariableDefinition>* sceneVariables)
{
    bool changed = false;
    const auto* definition = DrawVariableReference(
        action, globalVariables, sceneVariables, changed);
    if (definition) {
        if (changed)
            action["value"] = VariableStore::SerializeValue(definition->defaultValue);
        changed |= DrawVariableValue(action["value"], *definition);
    }
    return changed;
}

bool DrawNestedActions(nlohmann::json& actions, const UIEditor& uiEditor,
                       const SceneDocument& scene,
                       const std::vector<VariableDefinition>* globalVariables,
                       const std::vector<VariableDefinition>* sceneVariables,
                       unsigned int depth)
{
    if (depth >= 8) {
        ImGui::TextDisabled("Maximum If / Else nesting depth reached.");
        return false;
    }
    if (!actions.is_array()) {
        ImGui::TextWrapped("Branch actions must be an array.");
        return false;
    }
    bool changed = false;
    std::optional<std::size_t> removeIndex;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Separator();
        changed |= DrawUIAction(actions[i], uiEditor, &scene,
                                globalVariables, sceneVariables);
        if (ImGui::SmallButton("Remove branch action"))
            removeIndex = i;
        ImGui::PopID();
        if (removeIndex)
            break;
    }
    if (removeIndex) {
        actions.erase(*removeIndex);
        changed = true;
    }
    std::string newType;
    if (DrawActionTypeCombo("Add branch action", newType, true, false)) {
        actions.push_back(CreateInteractionAction(newType, uiEditor));
        changed = true;
    }
    return changed;
}

bool DrawIfElseCondition(nlohmann::json& condition,
                         const std::vector<VariableDefinition>* globalVariables,
                         const std::vector<VariableDefinition>* sceneVariables)
{
    bool changed = false;
    const auto* definition = DrawVariableReference(
        condition, globalVariables, sceneVariables, changed);
    if (!definition)
        return changed;
    if (changed) {
        condition["value"] = VariableStore::SerializeValue(definition->defaultValue);
        condition["operator"] = "==";
    }

    std::string operation = condition.value("operator", std::string("=="));
    const char* operators[] = {"==", "!="};
    int operatorCount = 2;
    if (std::holds_alternative<int>(definition->defaultValue)) {
        static const char* integerOperators[] = {"==", "!=", "<", "<=", ">", ">="};
        if (ImGui::BeginCombo("Operator", operation.c_str())) {
            for (const char* option : integerOperators) {
                if (ImGui::Selectable(option, operation == option)) {
                    operation = option;
                    condition["operator"] = operation;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    } else {
        if (operation != "==" && operation != "!=") {
            operation = "==";
            condition["operator"] = operation;
            changed = true;
        }
        if (ImGui::BeginCombo("Operator", operation.c_str())) {
            for (int i = 0; i < operatorCount; ++i) {
                if (ImGui::Selectable(operators[i], operation == operators[i])) {
                    operation = operators[i];
                    condition["operator"] = operation;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    }
    changed |= DrawVariableValue(condition["value"], *definition);
    return changed;
}

bool DrawIfElseDetails(nlohmann::json& action, const UIEditor& uiEditor,
                       const SceneDocument& scene,
                       const std::vector<VariableDefinition>* globalVariables,
                       const std::vector<VariableDefinition>* sceneVariables,
                       unsigned int depth, int selectedBranch,
                       bool selectIfElseTab)
{
    bool changed = false;
    if (ImGui::BeginTabBar("IfElseDetails")) {
        if (ImGui::BeginTabItem("Condition", nullptr,
                                selectIfElseTab && selectedBranch < 0
                                    ? ImGuiTabItemFlags_SetSelected
                                    : ImGuiTabItemFlags_None)) {
            changed |= DrawIfElseCondition(action["condition"], globalVariables,
                                           sceneVariables);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("True branch", nullptr,
                                selectIfElseTab && selectedBranch == 0
                                    ? ImGuiTabItemFlags_SetSelected
                                    : ImGuiTabItemFlags_None)) {
            changed |= DrawNestedActions(action["then"], uiEditor, scene,
                                         globalVariables, sceneVariables, depth + 1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("False branch", nullptr,
                                selectIfElseTab && selectedBranch == 1
                                    ? ImGuiTabItemFlags_SetSelected
                                    : ImGuiTabItemFlags_None)) {
            changed |= DrawNestedActions(action["else"], uiEditor, scene,
                                         globalVariables, sceneVariables, depth + 1);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    return changed;
}
}

bool DrawUIAction(nlohmann::json& action, const UIEditor& uiEditor,
                  const SceneDocument* scene,
                  const std::vector<VariableDefinition>* globalVariables,
                  const std::vector<VariableDefinition>* sceneVariables,
                  int selectedBranch, bool selectIfElseTab)
{
    bool changed = false;
    auto type = action["type"].get<std::string>();
    if (DrawActionTypeCombo("Operation", type, scene != nullptr, scene != nullptr)) {
        action = CreateInteractionAction(type, uiEditor);
        changed = true;
    }
    if (type == "SetVariable")
        return DrawVariableAssignment(action, globalVariables, sceneVariables) || changed;
    if (type == "IfElse" && scene)
        return DrawIfElseDetails(action, uiEditor, *scene, globalVariables,
                                 sceneVariables, 0, selectedBranch,
                                 selectIfElseTab) || changed;
    if (type == "ShowSubtitle") {
        changed |= action.erase("ui") != 0;
        std::array<char, 2048> text{};
        const auto currentText = action.value("text", std::string{});
        std::copy_n(currentText.begin(),
                    std::min(currentText.size(), text.size() - 1), text.begin());
        if (ImGui::InputTextMultiline("Subtitle text", text.data(), text.size(),
                                      ImVec2(-1.0f, 90.0f))) {
            action["text"] = text.data();
            changed = true;
        }
        return changed;
    }
    if (type == "ChangeScene" && scene) {
        auto& manager = Game::get_Instance().scene_manager;
        if (!manager) {
            ImGui::TextWrapped("Scene manager is unavailable.");
            return changed;
        }
        return DrawActionTarget(action["scene"], manager->MainScenes()) || changed;
    }
    if (type == "OpenPopupScene" && scene) {
        auto& manager = Game::get_Instance().scene_manager;
        if (!manager) {
            ImGui::TextWrapped("Scene manager is unavailable.");
            return changed;
        }
        std::filesystem::path owner = scene->Path();
        if (scene->Type() == SceneType::Popup) {
            const auto parent = scene->ParentScene();
            if (parent)
                owner = scene->Path().parent_path() / *parent;
        }
        const auto popupScenes = manager->PopupScenes(owner);
        if (popupScenes.empty())
            ImGui::TextWrapped("Register a popup belonging to this main scene in Scene Manager.");
        changed |= DrawActionTarget(action["scene"], popupScenes);
        float duration = action.value("duration", 0.5f);
        if (ImGui::DragFloat("Duration per fade (seconds)", &duration, 0.05f,
                             0.0f, 3600.0f)) {
            action["duration"] = std::clamp(duration, 0.0f, 3600.0f);
            changed = true;
        }
        return changed;
    }
    if (type == "ClosePopupScene") {
        ImGui::TextDisabled("Fades out, closes the top popup, then fades back in.");
        float duration = action.value("duration", 0.5f);
        if (ImGui::DragFloat("Duration per fade (seconds)", &duration, 0.05f,
                             0.0f, 3600.0f)) {
            action["duration"] = std::clamp(duration, 0.0f, 3600.0f);
            changed = true;
        }
        return changed;
    }
    if ((type == "ShowObject" || type == "HideObject") && scene) {
        std::optional<std::size_t> selected;
        if (action.contains("object") && action["object"].is_number_integer()) {
            const auto value = action["object"];
            if (value.is_number_unsigned())
                selected = value.get<std::size_t>();
            else if (value.get<std::int64_t>() >= 0)
                selected = static_cast<std::size_t>(value.get<std::int64_t>());
        }
        const auto label = selected && *selected < scene->ObjectCount()
            ? AudioObjectLabel(*scene, *selected)
            : (selected ? std::string("Missing object") : std::string("Choose object"));
        if (ImGui::BeginCombo("Object", label.c_str())) {
            if (ImGui::Selectable("None", !selected)) {
                action["object"] = nullptr;
                changed = true;
            }
            for (std::size_t i = 0; i < scene->ObjectCount(); ++i) {
                const auto option = AudioObjectLabel(*scene, i);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(option.c_str(), selected && *selected == i)) {
                    action["object"] = i;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    if ((type == "DisableHotspot" || type == "ChangeHotspotSetting") && scene) {
        std::optional<std::size_t> selectedHotspot;
        if (action.contains("hotspot") && action["hotspot"].is_number_integer()) {
            const auto value = action["hotspot"];
            if (value.is_number_unsigned())
                selectedHotspot = value.get<std::size_t>();
            else if (value.get<std::int64_t>() >= 0)
                selectedHotspot = static_cast<std::size_t>(value.get<std::int64_t>());
        }
        const auto hotspotLabel = selectedHotspot && *selectedHotspot < scene->ObjectCount()
            ? AudioObjectLabel(*scene, *selectedHotspot)
            : (selectedHotspot ? std::string("Missing hotspot") : std::string("Choose hotspot"));
        bool hasHotspot = false;
        if (ImGui::BeginCombo("Hotspot", hotspotLabel.c_str())) {
            if (ImGui::Selectable("None", !selectedHotspot)) {
                action["hotspot"] = nullptr;
                changed = true;
            }
            for (std::size_t i = 0; i < scene->ObjectCount(); ++i) {
                const auto object = scene->Object(i);
                if (!object.is_object() || !object.contains("components") ||
                    !object["components"].is_object() ||
                    !object["components"].contains("HotspotComp"))
                    continue;
                hasHotspot = true;
                const auto option = AudioObjectLabel(*scene, i);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(option.c_str(), selectedHotspot && *selectedHotspot == i)) {
                    action["hotspot"] = i;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
            if (!hasHotspot)
                ImGui::TextWrapped("Add a Hotspot object to this scene first.");
        }
        if (type == "DisableHotspot")
            return changed;

        bool checkVariable = action.value("check_variable", false);
        if (ImGui::Checkbox("Check variable", &checkVariable)) {
            action["check_variable"] = checkVariable;
            if (checkVariable && (!action.contains("variable") || !action["variable"].is_object()))
                action["variable"] = {{"scope", "global"}, {"id", ""}};
            changed = true;
        }
        if (checkVariable) {
            if (!action.contains("variable") || !action["variable"].is_object())
                action["variable"] = {{"scope", "global"}, {"id", ""}};
            ImGui::PushID("HotspotSettingVar");
            DrawVariableReference(action["variable"], globalVariables, sceneVariables, changed);
            ImGui::PopID();
        }
        bool requireItem = action.value("require_item", false);
        if (ImGui::Checkbox("Require selected item", &requireItem)) {
            action["require_item"] = requireItem;
            if (requireItem && (!action.contains("item") || !action["item"].is_number_integer())) {
                const auto items = uiEditor.InventoryTargets();
                action["item"] = items.empty() ? 0 : items.front().first;
            }
            changed = true;
        }
        if (requireItem) {
            const auto items = uiEditor.InventoryTargets();
            const int selectedItemId = action.value("item", 0);
            const auto foundItem = std::find_if(items.begin(), items.end(),
                [selectedItemId](const auto& item) { return item.first == selectedItemId; });
            const std::string itemPreview = foundItem != items.end()
                ? foundItem->second + " (" + std::to_string(foundItem->first) + ")"
                : (selectedItemId == 0 ? std::string("Choose item")
                                       : "Missing item (" + std::to_string(selectedItemId) + ")");
            if (ImGui::BeginCombo("Required item", itemPreview.c_str())) {
                for (const auto& item : items) {
                    const std::string option =
                        item.second + " (" + std::to_string(item.first) + ")";
                    ImGui::PushID(item.first);
                    if (ImGui::Selectable(option.c_str(), item.first == selectedItemId)) {
                        action["item"] = item.first;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                if (items.empty())
                    ImGui::TextWrapped("Add items in Window > Inventory Item Editor.");
                ImGui::EndCombo();
            }
            bool consumeItem = action.value("consume_item", false);
            if (ImGui::Checkbox("Consume item on success", &consumeItem)) {
                action["consume_item"] = consumeItem;
                changed = true;
            }
        }
        std::optional<std::size_t> selectedInteraction;
        if (action.contains("interaction") && action["interaction"].is_number_integer()) {
            const auto value = action["interaction"];
            if (value.is_number_unsigned())
                selectedInteraction = value.get<std::size_t>();
            else if (value.get<std::int64_t>() >= 0)
                selectedInteraction = static_cast<std::size_t>(value.get<std::int64_t>());
        }
        const auto interactionLabel = selectedInteraction && *selectedInteraction < scene->ObjectCount()
            ? AudioObjectLabel(*scene, *selectedInteraction)
            : (selectedInteraction ? std::string("Missing interaction") : std::string("None"));
        const char* comboName = (checkVariable || requireItem) ? "Success interaction" : "Interaction";
        if (ImGui::BeginCombo(comboName, interactionLabel.c_str())) {
            if (ImGui::Selectable("None", !selectedInteraction)) {
                action["interaction"] = nullptr;
                changed = true;
            }
            for (std::size_t i = 0; i < scene->ObjectCount(); ++i) {
                const auto candidate = scene->Object(i);
                if (!candidate.is_object() || !candidate.contains("components") ||
                    !candidate["components"].is_object() ||
                    !candidate["components"].contains("InteractionComp"))
                    continue;
                const auto option = AudioObjectLabel(*scene, i);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(option.c_str(),
                                      selectedInteraction && *selectedInteraction == i)) {
                    action["interaction"] = i;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (checkVariable || requireItem) {
            std::array<char, 1024> failBuf{};
            const auto currentFail = action.value("fail_message", std::string{});
            std::copy_n(currentFail.begin(),
                        std::min(currentFail.size(), failBuf.size() - 1), failBuf.begin());
            if (ImGui::InputText("Fail message", failBuf.data(), failBuf.size())) {
                action["fail_message"] = failBuf.data();
                changed = true;
            }
        }
        bool disableOnSuccess = action.value("disable_on_success", false);
        if (ImGui::Checkbox("Disable hotspot on success", &disableOnSuccess)) {
            action["disable_on_success"] = disableOnSuccess;
            changed = true;
        }
        return changed;
    }
    if (type == "SaveGame" || type == "LoadGame" || type == "DeleteSave") {
        int slot = std::clamp(action.value("slot", 1), 1, 99);
        if (ImGui::DragInt("Save slot", &slot, 1, 1, 99)) {
            action["slot"] = std::clamp(slot, 1, 99);
            changed = true;
        }
        ImGui::TextDisabled("Stored in scenes/.devilwood/saves/slot_%d.json", slot);
        return changed;
    }
    if (type == "EnableLens" || type == "DisableLens" || type == "ToggleLens") {
        ImGui::TextDisabled("%s",
            type == "EnableLens" ? "Enables the lens shader in the active scene."
            : (type == "DisableLens" ? "Disables the lens shader in the active scene."
                                     : "Toggles the lens shader on/off in the active scene."));
        return false;
    }
    if (type == "RunScript") {
        const auto scripts = ScriptEngine::AvailableScripts();
        const auto selectedScript = action.value("script", std::string{});
        const std::string label = selectedScript.empty() ? "Choose script" : selectedScript;
        if (ImGui::BeginCombo("Script", label.c_str())) {
            for (const auto& script : scripts) {
                if (ImGui::Selectable(script.c_str(), script == selectedScript)) {
                    action["script"] = script;
                    changed = true;
                }
            }
            if (scripts.empty())
                ImGui::TextWrapped("Add .lua files to the project scripts/ directory.");
            ImGui::EndCombo();
        }
        std::array<char, 128> fnBuf{};
        const auto currentFn = action.value("function", std::string{});
        std::copy_n(currentFn.begin(), std::min(currentFn.size(), fnBuf.size() - 1), fnBuf.begin());
        if (ImGui::InputTextWithHint("Function (optional)", "Leave empty to run file",
                                     fnBuf.data(), fnBuf.size())) {
            action["function"] = fnBuf.data();
            changed = true;
        }
        ImGui::BeginDisabled(selectedScript.empty());
        if (ImGui::Button("Open script")) {
            std::string openError;
            if (!OpenScriptInExternalEditor(selectedScript, openError))
                TraceLog(LOG_WARNING, "%s", openError.c_str());
        }
        ImGui::EndDisabled();
        return changed;
    }
    if (IsAudioAction(type) && scene)
        return DrawAudioActionTarget(action, *scene) || changed;
    if (IsManagedSoundAction(type)) {
        changed |= DrawManagedSoundTarget(action);
        if (IsManagedSoundFadeAction(type)) {
            float duration = action.value("duration", 1.0f);
            if (ImGui::DragFloat("Duration (seconds)", &duration, 0.05f, 0.0f, 3600.0f)) {
                action["duration"] = std::clamp(duration, 0.0f, 3600.0f);
                changed = true;
            }
        }
        return changed;
    }
    if (type == "AddItem" || type == "RemoveItem") {
        const auto items = uiEditor.InventoryTargets();
        const int itemId = action.value("item", 0);
        const auto selectedItem = std::find_if(items.begin(), items.end(),
            [itemId](const auto& item) { return item.first == itemId; });
        const auto label = selectedItem == items.end()
            ? std::string("Choose item") : selectedItem->second;
        if (items.empty())
            ImGui::TextWrapped("Add inventory items in the Inventory Item Editor first.");
        if (ImGui::BeginCombo("Inventory item", label.c_str())) {
            for (const auto& item : items) {
                if (ImGui::Selectable(item.second.c_str(), item.first == itemId)) {
                    action["item"] = item.first;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    const bool fade = type == "FadeInUI" || type == "FadeOutUI" || type == "ToggleFadeUI";
    const auto targets = uiEditor.Targets(fade);
    const auto id = action["ui"].get<std::string>();
    const auto selected = std::find_if(targets.begin(), targets.end(),
        [&id](const auto& target) { return target.first == id; });
    const auto label = selected == targets.end()
        ? (id.empty() ? std::string("Choose UI") : std::string("Missing: ") + id)
        : selected->second + " (" + selected->first + ")";
    if (!uiEditor.HasManager())
        ImGui::TextWrapped("Create the project UI manager first.");
    else if (!id.empty() && selected == targets.end())
        ImGui::TextWrapped("The selected UI element no longer exists.");
    else if (targets.empty())
        ImGui::TextWrapped(fade ? "Add a UI group in UI Manager first." : "Add a UI element in UI Manager first.");
    ImGui::BeginDisabled(!uiEditor.HasManager());
    if (ImGui::BeginCombo("UI element", label.c_str())) {
        for (const auto& target : targets) {
            const auto option = target.second + " (" + target.first + ")";
            if (ImGui::Selectable(option.c_str(), target.first == id)) {
                action["ui"] = target.first;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (fade) {
        float duration = action.value("duration", 1.0f);
        if (ImGui::DragFloat("Duration (seconds)", &duration, 0.05f, 0.0f, 3600.0f)) {
            action["duration"] = std::clamp(duration, 0.0f, 3600.0f);
            changed = true;
        }
    }
    return changed;
}

bool DrawAddInteractionAction(nlohmann::json& actions, const UIEditor& uiEditor,
                              const SceneDocument* scene, bool allowChangeScene,
                              bool enabled)
{
    static std::string selectedCategory = "UI";
    const bool sceneActions = scene != nullptr;
    std::vector<std::string> categories;
    for (const auto& choice : ActionChoices) {
        if (!IsActionChoiceAvailable(choice, sceneActions, allowChangeScene))
            continue;
        if (std::find(categories.begin(), categories.end(), choice.category) == categories.end())
            categories.emplace_back(choice.category);
    }
    if (std::find(categories.begin(), categories.end(), selectedCategory) == categories.end())
        selectedCategory = categories.empty() ? std::string{} : categories.front();

    std::string type;
    ImGui::BeginDisabled(!enabled);
    if (ImGui::BeginCombo("Category", selectedCategory.empty()
                          ? "No action categories" : selectedCategory.c_str())) {
        for (const auto& category : categories)
            if (ImGui::Selectable(category.c_str(), category == selectedCategory))
                selectedCategory = category;
        ImGui::EndCombo();
    }
    bool selected = false;
    if (ImGui::BeginCombo("Action", "Select action type")) {
        for (const auto& choice : ActionChoices) {
            if (choice.category != selectedCategory ||
                !IsActionChoiceAvailable(choice, sceneActions, allowChangeScene))
                continue;
            if (ImGui::Selectable(choice.label)) {
                type = choice.type;
                selected = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (!selected)
        return false;

    actions.push_back(CreateInteractionAction(type, uiEditor));
    return true;
}

namespace
{
bool CanAppendAction(const nlohmann::json& actions)
{
    return actions.empty() || actions.back()["type"] != "ChangeScene";
}

std::uintptr_t ActionNodeId(std::size_t index) { return 4 + index * 4; }
std::uintptr_t BranchNodeId(std::size_t index, bool trueBranch)
{
    return 0x60000000u + index * 2 + (trueBranch ? 0 : 1);
}
std::uintptr_t BranchInputPinId(std::size_t index, bool trueBranch)
{
    return 0x70000000u + index * 2 + (trueBranch ? 0 : 1);
}
std::uintptr_t BranchOutputPinId(std::size_t index, bool trueBranch)
{
    return 0x71000000u + index * 2 + (trueBranch ? 0 : 1);
}
std::uintptr_t IfElseBranchPinId(std::size_t index, bool trueBranch)
{
    return (trueBranch ? 0x40000000u : 0x50000000u) + index;
}
std::uintptr_t IfElseBranchLinkId(std::size_t index, std::size_t connection)
{
    return 0x72000000u + index * 4 + connection;
}

bool DragFloat2Field(const char* label, float values[2], float speed, const char* format)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float labelWidth = ImGui::CalcTextSize("X").x;
    const float fieldWidth = std::max(
        1.0f, (ImGui::GetContentRegionAvail().x - 2 * labelWidth - 3 * spacing) * 0.5f);
    ImGui::TextUnformatted("X");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fieldWidth);
    bool changed = ImGui::DragFloat("##X", &values[0], speed, 0.0f, 0.0f, format);
    ImGui::SameLine();
    ImGui::TextUnformatted("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fieldWidth);
    changed |= ImGui::DragFloat("##Y", &values[1], speed, 0.0f, 0.0f, format);
    ImGui::PopID();
    return changed;
}

bool DragInt2Field(const char* label, int values[2], float speed)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float labelWidth = ImGui::CalcTextSize("X").x;
    const float fieldWidth = std::max(
        1.0f, (ImGui::GetContentRegionAvail().x - 2 * labelWidth - 3 * spacing) * 0.5f);
    ImGui::TextUnformatted("X");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fieldWidth);
    bool changed = ImGui::DragInt("##X", &values[0], speed);
    ImGui::SameLine();
    ImGui::TextUnformatted("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(fieldWidth);
    changed |= ImGui::DragInt("##Y", &values[1], speed);
    ImGui::PopID();
    return changed;
}

nlohmann::json DefaultRectangleComponent()
{
    return {
        {"width", 128.0},
        {"height", 64.0},
        {"color", {{"r", 80}, {"g", 160}, {"b", 255}, {"a", 255}}},
        {"opacity", 0.75}
    };
}

std::size_t SceneLightCount(const SceneDocument& scene)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < scene.ObjectCount(); ++i) {
        const auto object = scene.Object(i);
        if (object.is_object() && object.contains("components") &&
            object["components"].is_object() &&
            object["components"].contains("LightComp"))
            ++count;
    }
    return count;
}

bool ContainsPoint(Entity& object, const nlohmann::json& data, Vector2 point)
{
    const auto* transform = object.getComponent<TransformComp>();
    const Vector2 position = transform ? transform->getWorldPosition() : Vector2{0, 0};
    const Vector2 scale = transform ? transform->getWorldScale() : Vector2{1, 1};
    const auto* sprite = object.getComponent<Sprite>();
    const Vector2 spritePosition = sprite ? sprite->getWorldPosition() : position;
    const auto& components = data["components"];

    if (const auto* panel = object.getComponent<Panel_Sprite>()) {
        if (CheckCollisionPointRec(point, {spritePosition.x, spritePosition.y,
                                           panel->targetRect.width * scale.x,
                                           panel->targetRect.height * scale.y}))
            return true;
    } else if (sprite) {
        if (CheckCollisionPointRec(point, {spritePosition.x, spritePosition.y,
                                           sprite->src_rec.width * scale.x,
                                           sprite->src_rec.height * scale.y}))
            return true;
    }
    if (const auto* rectangle = object.getComponent<RectangleComp>()) {
        const auto matrix = transform ? transform->getWorldMatrix()
                                      : Matrix2D::Transform({0, 0}, 0, {1, 1});
        const Vector2 local{
            point.x - matrix.m[0][2], point.y - matrix.m[1][2]
        };
        const float determinant = matrix.m[0][0] * matrix.m[1][1] -
                                  matrix.m[0][1] * matrix.m[1][0];
        if (std::abs(determinant) > 1e-8f) {
            const float x = (local.x * matrix.m[1][1] -
                             local.y * matrix.m[0][1]) / determinant;
            const float y = (local.y * matrix.m[0][0] -
                             local.x * matrix.m[1][0]) / determinant;
            if (x >= 0 && x <= rectangle->width &&
                y >= 0 && y <= rectangle->height)
                return true;
        }
    }
    if (const auto* hotspot = object.getComponent<HotspotComp>()) {
        if (hotspot->Contains(point))
            return true;
    } else if (const auto* collider = object.getComponent<ColliderComp>()) {
        if (CheckCollisionPointRec(point, collider->col_rect))
            return true;
    }
    if (const auto* circle = object.getComponent<CircleCol>()) {
        if (CheckCollisionPointCircle(point, {circle->c.x, circle->c.y}, circle->c.radius))
            return true;
    }
    if (const auto* textComponent = object.getComponent<TextComp>()) {
        const auto& text = components["TextComp"];
        const auto& margin = text.value("margin", nlohmann::json::object());
        const float x = position.x + margin.value("x", 0.0f);
        const float y = position.y + margin.value("y", 0.0f);
        const Vector2 size = textComponent->Measure();
        if (CheckCollisionPointRec(point, {x, y, size.x, size.y}))
            return true;
    }
    if (object.getComponent<LightComp>() &&
        CheckCollisionPointCircle(point, position, 8.0f))
        return true;
    return false;
}

bool ObjectBounds(Entity& object, const nlohmann::json& data, Rectangle& bounds)
{
    const auto* transform = object.getComponent<TransformComp>();
    const Vector2 position = transform ? transform->getWorldPosition() : Vector2{0, 0};
    const Vector2 scale = transform ? transform->getWorldScale() : Vector2{1, 1};
    const auto* sprite = object.getComponent<Sprite>();
    const Vector2 spritePosition = sprite ? sprite->getWorldPosition() : position;
    const auto& components = data["components"];
    bool hasBounds = false;
    auto include = [&bounds, &hasBounds](Rectangle next) {
        if (next.width <= 0 || next.height <= 0)
            return;
        if (!hasBounds) {
            bounds = next;
            hasBounds = true;
            return;
        }
        const float left = std::min(bounds.x, next.x);
        const float top = std::min(bounds.y, next.y);
        const float right = std::max(bounds.x + bounds.width, next.x + next.width);
        const float bottom = std::max(bounds.y + bounds.height, next.y + next.height);
        bounds = {left, top, right - left, bottom - top};
    };

    if (const auto* panel = object.getComponent<Panel_Sprite>())
        include({spritePosition.x, spritePosition.y, panel->targetRect.width * scale.x,
                 panel->targetRect.height * scale.y});
    else if (sprite)
        include({spritePosition.x, spritePosition.y, sprite->src_rec.width * scale.x,
                 sprite->src_rec.height * scale.y});
    if (const auto* rectangle = object.getComponent<RectangleComp>()) {
        const auto matrix = transform ? transform->getWorldMatrix()
                                      : Matrix2D::Transform({0, 0}, 0, {1, 1});
        const Vector2 corners[] = {
            matrix.TransformVector({0, 0}),
            matrix.TransformVector({rectangle->width, 0}),
            matrix.TransformVector({rectangle->width, rectangle->height}),
            matrix.TransformVector({0, rectangle->height})
        };
        float left = corners[0].x, right = corners[0].x;
        float top = corners[0].y, bottom = corners[0].y;
        for (const auto& corner : corners) {
            left = std::min(left, corner.x);
            right = std::max(right, corner.x);
            top = std::min(top, corner.y);
            bottom = std::max(bottom, corner.y);
        }
        include({left, top, right - left, bottom - top});
    }
    if (const auto* hotspot = object.getComponent<HotspotComp>()) {
        const auto corners = hotspot->Corners();
        const auto [left, right] = std::minmax_element(corners.begin(), corners.end(),
            [](Vector2 a, Vector2 b) { return a.x < b.x; });
        const auto [top, bottom] = std::minmax_element(corners.begin(), corners.end(),
            [](Vector2 a, Vector2 b) { return a.y < b.y; });
        include({left->x, top->y, right->x - left->x, bottom->y - top->y});
    } else if (const auto* collider = object.getComponent<ColliderComp>())
        include(collider->col_rect);
    if (const auto* circle = object.getComponent<CircleCol>())
        include({circle->c.x - circle->c.radius, circle->c.y - circle->c.radius,
                 circle->c.radius * 2, circle->c.radius * 2});
    if (const auto* textComponent = object.getComponent<TextComp>()) {
        const auto& text = components["TextComp"];
        const auto& margin = text.value("margin", nlohmann::json::object());
        const Vector2 size = textComponent->Measure();
        include({position.x + margin.value("x", 0.0f), position.y + margin.value("y", 0.0f),
                 size.x, size.y});
    }
    if (object.getComponent<LightComp>())
        include({position.x - 8.0f, position.y - 8.0f, 16.0f, 16.0f});
    return hasBounds;
}

}

SceneEditor::SceneEditor(std::filesystem::path directory, UIEditor& uiEditor)
    : directory(std::move(directory)), uiEditor(uiEditor)
{
    LoadGlobalVariables();
    LoadSettings();
    LoadEditorSettings();
    Refresh();
}

SceneEditor::~SceneEditor()
{
    SaveEditorSettings();
    ShutdownNodeEditor();
}

void SceneEditor::ShutdownNodeEditor()
{
    if (nodeEditor) {
        ed::DestroyEditor(nodeEditor);
        nodeEditor = nullptr;
    }
    nodeSettingsFile.clear();
    nodeScene.clear();
    nodeObject.reset();
    positionedActionNodes = 0;
    positionedBranchNodes.clear();
    selectedNodeForDetails.reset();
    initializeNodeView = false;
}

const SceneDocument* SceneEditor::Current() const
{
    return current ? &*current : nullptr;
}

void SceneEditor::LoadGlobalVariables()
{
    try {
        globalVariables = VariableStore::LoadGlobalDefinitions(directory);
        globalVariablesLoadFailed = false;
    } catch (const std::exception& e) {
        globalVariablesLoadFailed = true;
        variableMessage = e.what();
        variableMessageIsError = true;
        ReportError(e.what());
    }
}

bool SceneEditor::SaveGlobalVariables()
{
    try {
        VariableStore::SaveGlobalDefinitions(directory, globalVariables);
        globalVariablesDirty = false;
        globalVariablesLoadFailed = false;
        variableMessage = "Global variables saved.";
        variableMessageIsError = false;
        message = variableMessage;
        messageIsError = false;
        return true;
    } catch (const std::exception& e) {
        variableMessage = e.what();
        variableMessageIsError = true;
        ReportError(e.what());
        return false;
    }
}

void SceneEditor::UpdateSceneVariableDraft()
{
    if (!current) {
        sceneVariableDraft.clear();
        sceneVariableDraftPath.clear();
        sceneVariablesDirty = false;
        return;
    }
    if (sceneVariableDraftPath == current->Path())
        return;
    sceneVariableDraftPath = current->Path();
    sceneVariableDraft = current->Variables();
    sceneVariablesDirty = false;
}

bool SceneEditor::CommitSceneVariables(bool coalesce)
{
    if (!current)
        return true;
    UpdateSceneVariableDraft();
    if (!sceneVariablesDirty)
        return true;
    try {
        current->SetVariables(sceneVariableDraft, coalesce);
        sceneVariablesDirty = false;
        return true;
    } catch (const std::exception& e) {
        variableMessage = e.what();
        variableMessageIsError = true;
        ReportError(e.what());
        return false;
    }
}

int SceneEditor::RenderTargetWidth() const
{
    return appliedRenderWidth * RenderSettings::SupersampleFactor;
}

int SceneEditor::RenderTargetHeight() const
{
    return appliedRenderHeight * RenderSettings::SupersampleFactor;
}

void SceneEditor::LoadSettings()
{
    const auto path = directory / ".devilwood" / "game_settings.json";
    try {
        if (!std::filesystem::exists(path))
            return;
        std::ifstream input(path);
        if (!input)
            throw std::runtime_error("Cannot open game settings: " + path.string());
        nlohmann::json settings;
        input >> settings;
        if (!settings.is_object() || settings.value("version", 0) != 1 ||
            !settings.contains("render_width") || !settings["render_width"].is_number_integer() ||
            !settings.contains("render_height") || !settings["render_height"].is_number_integer())
            throw std::runtime_error("Game settings must contain version 1 and integer render dimensions.");

        const auto width = settings["render_width"].get<std::int64_t>();
        const auto height = settings["render_height"].get<std::int64_t>();
        if (width < MinRenderSize || width > MaxRenderSize ||
            height < MinRenderSize || height > MaxRenderSize)
            throw std::runtime_error("Game render dimensions must be between 1 and 8192 pixels.");

        renderWidth = appliedRenderWidth = static_cast<int>(width);
        renderHeight = appliedRenderHeight = static_cast<int>(height);
        lensOffset = appliedLensOffset = {
            settings.contains("lens_offset_x") && settings["lens_offset_x"].is_number()
                ? settings["lens_offset_x"].get<float>() : 0.0f,
            settings.contains("lens_offset_y") && settings["lens_offset_y"].is_number()
                ? settings["lens_offset_y"].get<float>() : 0.0f
        };
        Game::get_Instance().SetLensOffset(lensOffset);
        lensDragToMove = appliedLensDragToMove =
            settings.contains("lens_drag_to_move") && settings["lens_drag_to_move"].is_boolean()
                ? settings["lens_drag_to_move"].get<bool>() : false;
        Game::get_Instance().SetLensDragToMove(lensDragToMove);
        textureFiltering = appliedTextureFiltering =
            settings.contains("texture_filtering") && settings["texture_filtering"].is_boolean()
                ? settings["texture_filtering"].get<bool>() : false;
        ResourcesLoader::SetTextureFiltering(textureFiltering);
    } catch (const std::exception& e) {
        ReportError(e.what());
    }
}

void SceneEditor::LoadEditorSettings()
{
    const auto path = directory / ".devilwood" / "scene_editor_settings.json";
    try {
        if (!std::filesystem::exists(path))
            return;
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open scene editor settings: " + path.string());
        nlohmann::json settings;
        input >> settings;
        if (!settings.is_object() || settings.value("version", 0) != 1)
            throw std::runtime_error("Scene editor settings must use version 1.");
        const auto readNumber = [&settings](const char* key, float fallback) {
            if (!settings.contains(key))
                return fallback;
            if (!settings[key].is_number())
                throw std::runtime_error(std::string("Scene editor setting must be numeric: ") + key);
            const double value = settings[key].get<double>();
            if (!std::isfinite(value) || std::abs(value) > 10000000.0)
                throw std::runtime_error(std::string("Scene editor setting is out of range: ") + key);
            return static_cast<float>(value);
        };
        const float zoom = readNumber("scene_zoom", sceneZoom);
        const float offsetX = readNumber("scene_offset_x", sceneOffset.x);
        const float offsetY = readNumber("scene_offset_y", sceneOffset.y);
        if (zoom < 0.1f || zoom > 8.0f)
            throw std::runtime_error("Scene editor zoom must be between 0.1 and 8.");
        if (settings.contains("light_preview")) {
            if (!settings["light_preview"].is_boolean())
                throw std::runtime_error("Scene editor light_preview setting must be a boolean.");
            lightPreview = settings["light_preview"].get<bool>();
        }
        if (settings.contains("interaction_graph_open")) {
            if (!settings["interaction_graph_open"].is_boolean())
                throw std::runtime_error(
                    "Scene editor interaction_graph_open setting must be a boolean.");
            showInteractionGraph = settings["interaction_graph_open"].get<bool>();
        }
        sceneZoom = zoom;
        sceneOffset = {offsetX, offsetY};
    } catch (const std::exception& e) {
        ReportError(e.what());
    }
}

void SceneEditor::SaveEditorSettings() const
{
    const auto path = directory / ".devilwood" / "scene_editor_settings.json";
    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Cannot write scene editor settings: " + path.string());
        output << nlohmann::json{
            {"version", 1},
            {"scene_zoom", sceneZoom},
            {"scene_offset_x", sceneOffset.x},
            {"scene_offset_y", sceneOffset.y},
            {"light_preview", lightPreview},
            {"interaction_graph_open", showInteractionGraph}
        }.dump(2) << '\n';
        output.flush();
        if (!output)
            throw std::runtime_error("Cannot finish writing scene editor settings: " + path.string());
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Cannot save scene editor settings: %s", e.what());
    }
}

void SceneEditor::SaveSettings()
{
    const auto path = directory / ".devilwood" / "game_settings.json";
    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::trunc);
        if (!output)
            throw std::runtime_error("Cannot write game settings: " + path.string());
        const nlohmann::json settings = {
            {"version", 1},
            {"render_width", renderWidth},
            {"render_height", renderHeight},
            {"lens_offset_x", lensOffset.x},
            {"lens_offset_y", lensOffset.y},
            {"lens_drag_to_move", lensDragToMove},
            {"texture_filtering", textureFiltering}
        };
        output << settings.dump(2) << '\n';
        output.flush();
        if (!output)
            throw std::runtime_error("Cannot finish writing game settings: " + path.string());

        Game::get_Instance().SetRenderResolution(renderWidth, renderHeight);
        Game::get_Instance().SetLensOffset(lensOffset);
        Game::get_Instance().SetLensDragToMove(lensDragToMove);
        ResourcesLoader::SetTextureFiltering(textureFiltering);
        appliedRenderWidth = renderWidth;
        appliedRenderHeight = renderHeight;
        appliedLensOffset = lensOffset;
        appliedLensDragToMove = lensDragToMove;
        appliedTextureFiltering = textureFiltering;
        settingsDirty = false;
        message = "Game settings saved.";
        messageIsError = false;
    } catch (const std::exception& e) {
        ReportError(e.what());
    }
}

void SceneEditor::ReportError(const std::string& error)
{
    message = error;
    messageIsError = true;
}

void SceneEditor::Refresh()
{
    try {
        files = SceneDocument::List(directory);
        cachedSceneFiles.clear();
        cachedSceneFiles.reserve(files.size());
        for (const auto& file : files) {
            CachedSceneFile info;
            info.path = file;
            try {
                const auto document = SceneDocument::Open(file);
                info.type = document.Type();
                info.parentScene = document.ParentScene();
            } catch (const std::exception& e) {
                info.error = e.what();
            }
            cachedSceneFiles.push_back(std::move(info));
        }
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
}

bool SceneEditor::EnsurePreviewGraph() const
{
    if (!current) {
        cachedPreviewData.clear();
        cachedPreviewGraph = {};
        cachedPreviewPath.clear();
        cachedPreviewRevision = 0;
        cachedPreviewValid = false;
        return false;
    }
    const auto rev = current->Revision();
    const auto& path = current->Path();
    if (cachedPreviewValid && cachedPreviewRevision == rev && cachedPreviewPath == path)
        return true;

    cachedPreviewData.clear();
    cachedPreviewData.reserve(current->ObjectCount());
    for (std::size_t i = 0; i < current->ObjectCount(); ++i)
        cachedPreviewData.push_back(current->Object(i));
    cachedPreviewGraph = LoadSceneGraph(cachedPreviewData);
    cachedPreviewPath = path;
    cachedPreviewRevision = rev;
    cachedPreviewValid = true;
    return true;
}

void SceneEditor::Request(Action action, const std::filesystem::path& path)
{
    if (pending != Action::None)
        return;
    pending = action;
    pendingPath = path;
    if ((current && current->IsDirty()) || sceneVariablesDirty)
        openConfirmation = true;
}

bool SceneEditor::RequestExit()
{
    if ((!current || !current->IsDirty()) && !sceneVariablesDirty &&
        !globalVariablesDirty)
        return true;
    pending = Action::Exit;
    pendingPath.clear();
    openConfirmation = true;
    return false;
}

bool SceneEditor::Save()
{
    if (globalVariablesDirty && !SaveGlobalVariables())
        return false;
    if (!current)
        return true;
    if (!CommitSceneVariables())
        return false;
    try {
        current->Save();
        message = "Saved " + current->Path().filename().string();
        messageIsError = false;
        Refresh();
        return true;
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
        return false;
    }
}

bool SceneEditor::SaveForRun()
{
    SaveEditorSettings();
    if (globalVariablesLoadFailed && !globalVariablesDirty) {
        variableMessage = "Fix or replace the invalid global variables before starting Play.";
        variableMessageIsError = true;
        ReportError(variableMessage);
        return false;
    }
    if (globalVariablesDirty && !SaveGlobalVariables())
        return false;
    if (!current || (!current->IsDirty() && !sceneVariablesDirty))
        return true;
    return Save();
}

void SceneEditor::Execute(bool& exitRequested)
{
    try {
        if (pending == Action::Exit) {
            exitRequested = true;
        } else if (pending == Action::New || pending == Action::Open) {
            UpdateSceneVariableDraft();
            SceneDocument next = pending == Action::New
                ? SceneDocument::Create(directory, idInput.data())
                : SceneDocument::Open(pendingPath);
            nameInput.fill('\0');
            std::copy(next.Name().begin(), next.Name().end(), nameInput.begin());
            current = std::move(next);
            sceneVariableDraftPath.clear();
            UpdateSceneVariableDraft();
            selectedObject.reset();
            draggingObject.reset();
            contextObject.reset();
            message = pending == Action::New ? "New scene: save to create its JSON file." : "Scene opened.";
            messageIsError = false;
        }
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
    pending = Action::None;
    pendingPath.clear();
}

void SceneEditor::DrawScene(Screen* lighting) const
{
    const bool hasGraph = EnsurePreviewGraph();
    const auto& graph = cachedPreviewGraph;
    const auto& data = cachedPreviewData;
    (void)hasGraph;

    Camera2D camera{};
    camera.offset = sceneOffset;
    camera.target = {0, 0};
    camera.zoom = sceneZoom;

    const Rectangle renderBounds{0, 0, static_cast<float>(renderWidth),
                                 static_cast<float>(renderHeight)};
    const Vector2 screenTopLeft = GetWorldToScreen2D({renderBounds.x, renderBounds.y}, camera);
    const Vector2 screenBottomRight = GetWorldToScreen2D(
        {renderBounds.x + renderBounds.width, renderBounds.y + renderBounds.height}, camera);
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const int clipLeft = std::clamp(
        static_cast<int>(std::floor(std::min(screenTopLeft.x, screenBottomRight.x))),
        0, screenWidth);
    const int clipTop = std::clamp(
        static_cast<int>(std::floor(std::min(screenTopLeft.y, screenBottomRight.y))),
        0, screenHeight);
    const int clipRight = std::clamp(
        static_cast<int>(std::ceil(std::max(screenTopLeft.x, screenBottomRight.x))),
        0, screenWidth);
    const int clipBottom = std::clamp(
        static_cast<int>(std::ceil(std::max(screenTopLeft.y, screenBottomRight.y))),
        0, screenHeight);

    BeginMode2D(camera);
    if (clipRight > clipLeft && clipBottom > clipTop) {
        BeginScissorMode(clipLeft, clipTop, clipRight - clipLeft, clipBottom - clipTop);
        DrawRectangleRec(renderBounds, Color{30, 33, 40, 255});
        Entity::DrawLayer(graph.roots, false);
        EndMode2D();
        if (lightPreview && current && lighting)
            lighting->DrawLighting(graph.roots, camera, true, current->BaseDarkness());
        BeginMode2D(camera);
        Entity::DrawLayer(graph.roots, true);
        for (std::size_t i = 0; i < graph.objects.size(); ++i) {
            auto* object = graph.objects[i];
            if (!object)
                continue;
            if (const auto* hotspot = object->getComponent<HotspotComp>()) {
                const auto corners = hotspot->Corners();
                for (std::size_t corner = 0; corner < corners.size(); ++corner)
                    DrawLineV(corners[corner], corners[(corner + 1) % corners.size()],
                              Color{230, 90, 90, 180});
            } else if (const auto* collider = object->getComponent<ColliderComp>())
                DrawRectangleLinesEx(collider->col_rect, 1, Color{230, 90, 90, 180});
            if (const auto* circle = object->getComponent<CircleCol>())
                DrawCircleLinesV({circle->c.x, circle->c.y}, circle->c.radius,
                                 Color{230, 90, 90, 180});
            if (const auto* light = object->getComponent<LightComp>()) {
                const auto* transform = object->getComponent<TransformComp>();
                const Vector2 position = transform
                    ? transform->getWorldPosition() : Vector2{0, 0};
                if (!light->useSpriteShape)
                    DrawCircleLinesV(position, light->radius, Fade(light->color, 0.65f));
                DrawCircleV(position, 5.0f / sceneZoom, light->color);
                DrawLineV({position.x - 8.0f / sceneZoom, position.y},
                          {position.x + 8.0f / sceneZoom, position.y}, light->color);
                DrawLineV({position.x, position.y - 8.0f / sceneZoom},
                          {position.x, position.y + 8.0f / sceneZoom}, light->color);
            }
            if (selectedObject && *selectedObject == i) {
                Rectangle bounds{};
                if (ObjectBounds(*object, data[i], bounds)) {
                    const float lineThickness = 1.5f / sceneZoom;
                    const bool locked = data[i].value("locked", false);
                    DrawRectangleLinesEx(bounds, lineThickness,
                                         locked ? YELLOW : Color{80, 195, 255, 255});
                    if (!locked) {
                        const float half = 4.5f / sceneZoom;
                        const float x0 = bounds.x;
                        const float xm = bounds.x + bounds.width * 0.5f;
                        const float x1 = bounds.x + bounds.width;
                        const float y0 = bounds.y;
                        const float ym = bounds.y + bounds.height * 0.5f;
                        const float y1 = bounds.y + bounds.height;
                        const std::array<std::pair<ResizeHandle, Vector2>, 8> handles = {{
                            {ResizeHandle::TopLeft, {x0, y0}},
                            {ResizeHandle::Top, {xm, y0}},
                            {ResizeHandle::TopRight, {x1, y0}},
                            {ResizeHandle::Right, {x1, ym}},
                            {ResizeHandle::BottomRight, {x1, y1}},
                            {ResizeHandle::Bottom, {xm, y1}},
                            {ResizeHandle::BottomLeft, {x0, y1}},
                            {ResizeHandle::Left, {x0, ym}}
                        }};
                        const ResizeHandle activeHandle = resizingObject
                            ? resizingObject->handle : hoveredResizeHandle;
                        for (const auto& [handle, pos] : handles) {
                            const Rectangle box{pos.x - half, pos.y - half, half * 2.0f, half * 2.0f};
                            const bool highlight = activeHandle == handle;
                            DrawRectangleRec(box, highlight ? YELLOW : WHITE);
                            DrawRectangleLinesEx(box, 1.0f / sceneZoom, Color{25, 30, 40, 255});
                        }
                    }
                }
            }
        }
        EndScissorMode();
    }
    DrawRectangleLinesEx(renderBounds, 1.5f / sceneZoom, Color{70, 190, 255, 255});
    EndMode2D();

    if (!current)
        DrawText("Create or open a scene", 30, 30, 24, LIGHTGRAY);
    else if (current->ObjectCount() == 0)
        DrawText("Empty scene", 30, 30, 24, LIGHTGRAY);
}

void SceneEditor::HandleSceneInput()
{
    const Vector2 mouse = GetMousePosition();
    const bool insideScene = mouse.x >= 0 && mouse.x < GetScreenWidth() &&
                             mouse.y >= 0 && mouse.y < GetScreenHeight();
    const bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;

    if (!mouseCaptured && insideScene && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        panningScene = true;
        panStartMouse = mouse;
        panStartOffset = sceneOffset;
        draggingObject.reset();
    }
    if (panningScene) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            sceneOffset = {
                panStartOffset.x + mouse.x - panStartMouse.x,
                panStartOffset.y + mouse.y - panStartMouse.y
            };
        } else {
            panningScene = false;
        }
    }

    const float wheelMove = GetMouseWheelMove();
    if (!mouseCaptured && insideScene && !panningScene && wheelMove != 0.0f) {
        Camera2D camera{};
        camera.offset = sceneOffset;
        camera.target = {0, 0};
        camera.zoom = sceneZoom;
        const Vector2 worldUnderMouse = GetScreenToWorld2D(mouse, camera);
        sceneZoom = std::clamp(sceneZoom * std::pow(1.1f, wheelMove), 0.1f, 8.0f);
        sceneOffset = {
            mouse.x - worldUnderMouse.x * sceneZoom,
            mouse.y - worldUnderMouse.y * sceneZoom
        };
    }

    if (!current)
        return;

    try {
        EnsurePreviewGraph();
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
        return;
    }
    const auto& data = cachedPreviewData;
    auto& graph = cachedPreviewGraph;

    Camera2D camera{};
    camera.offset = sceneOffset;
    camera.target = {0, 0};
    camera.zoom = sceneZoom;
    const Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);

    if (pickingScriptRef && IsKeyPressed(KEY_ESCAPE)) {
        pickingScriptRef.reset();
        message = "Cancelled object pick.";
        messageIsError = false;
    }

    auto hitTestResizeHandle = [&](const Rectangle& bounds, Vector2 worldPt) -> ResizeHandle {
        const float hitHalf = 7.0f / sceneZoom;
        const float x0 = bounds.x;
        const float xm = bounds.x + bounds.width * 0.5f;
        const float x1 = bounds.x + bounds.width;
        const float y0 = bounds.y;
        const float ym = bounds.y + bounds.height * 0.5f;
        const float y1 = bounds.y + bounds.height;
        const std::array<std::pair<ResizeHandle, Vector2>, 8> handles = {{
            {ResizeHandle::TopLeft, {x0, y0}},
            {ResizeHandle::TopRight, {x1, y0}},
            {ResizeHandle::BottomRight, {x1, y1}},
            {ResizeHandle::BottomLeft, {x0, y1}},
            {ResizeHandle::Top, {xm, y0}},
            {ResizeHandle::Right, {x1, ym}},
            {ResizeHandle::Bottom, {xm, y1}},
            {ResizeHandle::Left, {x0, ym}}
        }};
        for (const auto& [handle, pos] : handles) {
            if (std::abs(worldPt.x - pos.x) <= hitHalf &&
                std::abs(worldPt.y - pos.y) <= hitHalf)
                return handle;
        }
        return ResizeHandle::None;
    };

    hoveredResizeHandle = ResizeHandle::None;
    if (!mouseCaptured && insideScene && !panningScene && !pickingScriptRef &&
        !resizingObject && selectedObject && *selectedObject < graph.objects.size() &&
        graph.objects[*selectedObject] && !data[*selectedObject].value("locked", false)) {
        Rectangle selectedBounds{};
        if (ObjectBounds(*graph.objects[*selectedObject], data[*selectedObject], selectedBounds))
            hoveredResizeHandle = hitTestResizeHandle(selectedBounds, worldMouse);
    }

    if (!mouseCaptured && insideScene && !panningScene &&
        !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (pickingScriptRef && current) {
            std::optional<std::size_t> picked;
            for (int layer = 1; layer >= 0 && !picked; --layer) {
                for (std::size_t i = graph.objects.size(); i > 0; --i) {
                    const std::size_t index = i - 1;
                    auto* object = graph.objects[index];
                    if (!object || object->IsUiLayer() != (layer == 1) ||
                        !ContainsPoint(*object, data[index], worldMouse))
                        continue;
                    picked = index;
                    break;
                }
            }
            if (picked && pickingScriptRef->objectIndex < current->ObjectCount()) {
                const auto requiredKey = ScriptEngine::RefTypeComponentKey(pickingScriptRef->refType);
                const auto pickedObj = current->Object(*picked);
                const bool matches = requiredKey.empty() ||
                    (pickedObj.is_object() && pickedObj.contains("components") &&
                     pickedObj["components"].is_object() &&
                     pickedObj["components"].contains(requiredKey));
                if (matches) {
                    auto ownerObj = current->Object(pickingScriptRef->objectIndex);
                    if (ownerObj.is_object() && ownerObj.contains("components") &&
                        ownerObj["components"].is_object() &&
                        ownerObj["components"].contains("ScriptComp")) {
                        auto& scriptData = ownerObj["components"]["ScriptComp"];
                        if (!scriptData.contains("properties") || !scriptData["properties"].is_object())
                            scriptData["properties"] = nlohmann::json::object();
                        scriptData["properties"][pickingScriptRef->propertyName] = {{"object", *picked}};
                        current->SetObject(pickingScriptRef->objectIndex, ownerObj);
                        message = "Assigned " + AudioObjectLabel(*current, *picked) +
                                  " to '" + pickingScriptRef->propertyName + "'.";
                        messageIsError = false;
                    }
                    pickingScriptRef.reset();
                } else {
                    message = AudioObjectLabel(*current, *picked) + " does not have a " +
                              pickingScriptRef->refType + " component.";
                    messageIsError = true;
                }
            }
        } else if (hoveredResizeHandle != ResizeHandle::None && selectedObject &&
                   *selectedObject < graph.objects.size() && graph.objects[*selectedObject]) {
            Rectangle selectedBounds{};
            const auto& objData = data[*selectedObject];
            if (ObjectBounds(*graph.objects[*selectedObject], objData, selectedBounds) &&
                selectedBounds.width > 0.0f && selectedBounds.height > 0.0f) {
                ResizeDragState state;
                state.objectIndex = *selectedObject;
                state.handle = hoveredResizeHandle;
                state.startBounds = selectedBounds;
                state.startMouseWorld = worldMouse;
                const auto& comps = objData["components"];
                if (comps.contains("TransformComp") && comps["TransformComp"].is_object()) {
                    const auto& t = comps["TransformComp"];
                    if (t.contains("position"))
                        state.startPosition = {t["position"].value("x", 0.0f), t["position"].value("y", 0.0f)};
                    if (t.contains("scale"))
                        state.startScale = {t["scale"].value("x", 1.0f), t["scale"].value("y", 1.0f)};
                }
                if (comps.contains("RectangleComp") && comps["RectangleComp"].is_object()) {
                    state.hasRectangle = true;
                    state.startRectSize = {
                        comps["RectangleComp"].value("width", 64.0f),
                        comps["RectangleComp"].value("height", 64.0f)
                    };
                } else if (!comps.contains("Sprite") && comps.contains("ColliderComp") &&
                           comps["ColliderComp"].is_object()) {
                    state.hasColliderOnly = true;
                    state.startColliderSize = {
                        comps["ColliderComp"].value("width", 64.0f),
                        comps["ColliderComp"].value("height", 64.0f)
                    };
                }
                resizingObject = state;
                draggingObject.reset();
            }
        } else {
        selectedObject.reset();
        draggingObject.reset();
        for (int layer = 1; layer >= 0 && !selectedObject; --layer) {
            for (std::size_t i = graph.objects.size(); i > 0; --i) {
                const std::size_t index = i - 1;
                auto* object = graph.objects[index];
                if (!object || object->IsUiLayer() != (layer == 1) ||
                    !ContainsPoint(*object, data[index], worldMouse))
                    continue;
                selectedObject = index;
                const auto dataObject = data[index];
                if (!dataObject.value("locked", false))
                    draggingObject = index;
                objectNameInput.fill('\0');
                std::copy(object->name.begin(), object->name.end(), objectNameInput.begin());
                const auto* transform = object->getComponent<TransformComp>();
                const Vector2 worldPosition = transform ? transform->getWorldPosition() : Vector2{0, 0};
                dragOffsetX = worldMouse.x - worldPosition.x;
                dragOffsetY = worldMouse.y - worldPosition.y;
                break;
            }
        }
        }
    }
    if (!mouseCaptured && insideScene && !panningScene &&
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        contextObject.reset();
        for (int layer = 1; layer >= 0 && !contextObject; --layer) {
            for (std::size_t i = graph.objects.size(); i > 0; --i) {
                const std::size_t index = i - 1;
                auto* object = graph.objects[index];
                if (!object || object->IsUiLayer() != (layer == 1) ||
                    !ContainsPoint(*object, data[index], worldMouse))
                    continue;
                contextObject = index;
                selectedObject = index;
                break;
            }
        }
        openObjectContextMenu = true;
    }

    if (resizingObject && current) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
            resizingObject->objectIndex >= current->ObjectCount()) {
            resizingObject.reset();
        } else if (!mouseCaptured) {
            const auto& st = *resizingObject;
            const float dx = worldMouse.x - st.startMouseWorld.x;
            const float dy = worldMouse.y - st.startMouseWorld.y;
            constexpr float MinSize = 4.0f;

            float newX = st.startBounds.x;
            float newY = st.startBounds.y;
            float newW = st.startBounds.width;
            float newH = st.startBounds.height;

            const bool moveLeft = st.handle == ResizeHandle::TopLeft ||
                                  st.handle == ResizeHandle::Left ||
                                  st.handle == ResizeHandle::BottomLeft;
            const bool moveRight = st.handle == ResizeHandle::TopRight ||
                                   st.handle == ResizeHandle::Right ||
                                   st.handle == ResizeHandle::BottomRight;
            const bool moveTop = st.handle == ResizeHandle::TopLeft ||
                                 st.handle == ResizeHandle::Top ||
                                 st.handle == ResizeHandle::TopRight;
            const bool moveBottom = st.handle == ResizeHandle::BottomLeft ||
                                    st.handle == ResizeHandle::Bottom ||
                                    st.handle == ResizeHandle::BottomRight;

            if (moveLeft) {
                newW = std::max(MinSize, st.startBounds.width - dx);
                newX = st.startBounds.x + (st.startBounds.width - newW);
            } else if (moveRight) {
                newW = std::max(MinSize, st.startBounds.width + dx);
            }
            if (moveTop) {
                newH = std::max(MinSize, st.startBounds.height - dy);
                newY = st.startBounds.y + (st.startBounds.height - newH);
            } else if (moveBottom) {
                newH = std::max(MinSize, st.startBounds.height + dy);
            }

            // Hold Shift while dragging a corner handle to preserve aspect ratio.
            const bool isCorner = (moveLeft || moveRight) && (moveTop || moveBottom);
            if (isCorner && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) {
                const float ratioX = newW / st.startBounds.width;
                const float ratioY = newH / st.startBounds.height;
                const float uniform = std::max(0.05f, std::abs(ratioX - 1.0f) >= std::abs(ratioY - 1.0f)
                                                          ? ratioX : ratioY);
                newW = std::max(MinSize, st.startBounds.width * uniform);
                newH = std::max(MinSize, st.startBounds.height * uniform);
                if (moveLeft)
                    newX = st.startBounds.x + (st.startBounds.width - newW);
                if (moveTop)
                    newY = st.startBounds.y + (st.startBounds.height - newH);
            }

            const float ratioX = newW / st.startBounds.width;
            const float ratioY = newH / st.startBounds.height;
            const float deltaPosX = newX - st.startBounds.x;
            const float deltaPosY = newY - st.startBounds.y;

            auto objectData = current->Object(st.objectIndex);
            auto& comps = objectData["components"];
            if (!comps.contains("TransformComp")) {
                comps["TransformComp"] = {
                    {"position", {{"x", st.startPosition.x}, {"y", st.startPosition.y}}},
                    {"scale", {{"x", st.startScale.x}, {"y", st.startScale.y}}}
                };
            }
            auto& trans = comps["TransformComp"];
            trans["position"]["x"] = st.startPosition.x + deltaPosX;
            trans["position"]["y"] = st.startPosition.y + deltaPosY;

            if (st.hasRectangle && comps.contains("RectangleComp")) {
                comps["RectangleComp"]["width"] = std::max(1.0f, st.startRectSize.x * ratioX);
                comps["RectangleComp"]["height"] = std::max(1.0f, st.startRectSize.y * ratioY);
            } else if (st.hasColliderOnly && comps.contains("ColliderComp")) {
                comps["ColliderComp"]["width"] = std::max(1.0f, st.startColliderSize.x * ratioX);
                comps["ColliderComp"]["height"] = std::max(1.0f, st.startColliderSize.y * ratioY);
            } else {
                trans["scale"]["x"] = std::max(0.05f, st.startScale.x * ratioX);
                trans["scale"]["y"] = std::max(0.05f, st.startScale.y * ratioY);
            }

            try {
                current->SetObject(st.objectIndex, objectData, true);
            } catch (const std::exception& e) {
                message = e.what();
                messageIsError = true;
            }
        }
    }

    if (draggingObject && current) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            draggingObject.reset();
        } else if (!mouseCaptured) {
            auto objectData = current->Object(*draggingObject);
            if (!objectData["components"].contains("TransformComp")) {
                objectData["components"]["TransformComp"] = {
                    {"position", {{"x", 0}, {"y", 0}}},
                    {"scale", {{"x", 1.0}, {"y", 1.0}}}
                };
            }
            const Vector2 worldPosition{worldMouse.x - dragOffsetX, worldMouse.y - dragOffsetY};
            const auto parent = current->Parent(*draggingObject);
            const TransformComp* parentTransform = nullptr;
            if (parent && graph.objects[*parent])
                parentTransform = graph.objects[*parent]->getComponent<TransformComp>();
            Vector2 localPosition{};
            if (!WorldToLocal(parentTransform, worldPosition, localPosition)) {
                message = "Cannot move this object through a singular parent transform.";
                messageIsError = true;
                return;
            }
            auto& position = objectData["components"]["TransformComp"]["position"];
            if (position["x"].get<float>() != localPosition.x ||
                position["y"].get<float>() != localPosition.y) {
                position["x"] = localPosition.x;
                position["y"] = localPosition.y;
                try {
                    current->SetObject(*draggingObject, objectData, true);
                } catch (const std::exception& e) {
                    message = e.what();
                    messageIsError = true;
                }
            }
        }
    }
}

void SceneEditor::Draw(bool& exitRequested, bool sceneMode, bool& showPanel, bool& showObjectList,
                       bool& showVariableManager, bool showSettings, bool showManager)
{
    if (sceneMode && current) {
        const auto& io = ImGui::GetIO();
        if (io.KeyCtrl && !io.WantCaptureKeyboard && !io.WantTextInput) {
            const bool undoPressed = ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift;
            const bool redoPressed = ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
                                     (ImGui::IsKeyPressed(ImGuiKey_Z, false) && io.KeyShift);
            const bool changed = undoPressed ? current->Undo()
                               : redoPressed ? current->Redo()
                               : false;
            if (changed) {
                sceneVariableDraft = current->Variables();
                sceneVariableDraftPath = current->Path();
                sceneVariablesDirty = false;
                nameInput.fill('\0');
                std::copy_n(current->Name().begin(),
                            std::min(current->Name().size(), nameInput.size() - 1),
                            nameInput.begin());
                draggingObject.reset();
                contextObject.reset();
                openObjectContextMenu = false;
                if (selectedObject && *selectedObject >= current->ObjectCount())
                    selectedObject.reset();
                if (selectedObject) {
                    const auto object = current->Object(*selectedObject);
                    objectNameInput.fill('\0');
                    if (object.contains("name") && object["name"].is_string()) {
                        const auto restoredName = object["name"].get<std::string>();
                        std::copy_n(restoredName.begin(),
                                    std::min(restoredName.size(), objectNameInput.size() - 1),
                                    objectNameInput.begin());
                    }
                }
            }
        }
    }

    if (sceneMode)
        HandleSceneInput();

    if (showPanel) {
    ImGui::SetNextWindowPos(ImVec2(1040, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(230, 550), ImGuiCond_FirstUseEver);
    if (focusCurrentSceneTab)
        ImGui::SetNextWindowFocus();
    ImGui::Begin("Scenes");
    if (ImGui::BeginTabBar("SceneWindowTabs")) {
        const ImGuiTabItemFlags currentSceneTabFlags =
            focusCurrentSceneTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        focusCurrentSceneTab = false;
        if (ImGui::BeginTabItem("Current Scene", nullptr, currentSceneTabFlags)) {
            if (current) {
                ImGui::SeparatorText("Current scene");
                ImGui::TextWrapped("%s%s", current->Path().filename().string().c_str(),
                                   current->IsDirty() || sceneVariablesDirty ? " *" : "");
                if (ImGui::InputText("Name", nameInput.data(), nameInput.size()))
                    current->SetName(nameInput.data(), ImGui::IsAnyItemActive());
                ImGui::Text("Objects: %zu", current->ObjectCount());
                ImGui::Checkbox("Preview lighting", &lightPreview);
                ImGui::SeparatorText("Objects");
                if (ImGui::Button("Add object")) {
                    current->AddObject();
                    selectedObject = current->ObjectCount() - 1;
                    objectNameInput.fill('\0');
                    const std::string objectName = "Object";
                    std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
                }
                ImGui::SameLine();
                const auto sceneLightCount = SceneLightCount(*current);
                ImGui::BeginDisabled(sceneLightCount >= RenderSettings::MaxSceneLights);
                if (ImGui::Button("Add light")) {
                    const std::string lightName = "Light " + std::to_string(sceneLightCount + 1);
                    current->AddObject({
                        {"name", lightName},
                        {"components", {
                            {"TransformComp", {
                                {"position", {
                                    {"x", RenderSettings::AuthoredWidth / 2.0f},
                                    {"y", RenderSettings::AuthoredHeight / 2.0f}
                                }},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            }},
                            {"LightComp", {
                                {"intensity", 1.25},
                                {"radius", 64.0},
                                {"color", {{"r", 255}, {"g", 220}, {"b", 150}, {"a", 255}}}
                            }}
                        }}
                    });
                    const auto index = current->ObjectCount() - 1;
                    selectedObject = index;
                    objectNameInput.fill('\0');
                    std::copy(lightName.begin(), lightName.end(), objectNameInput.begin());
                }
                ImGui::EndDisabled();
                if (ImGui::Button("Add rectangle")) {
                    const std::string rectangleName = "Rectangle";
                    current->AddObject({
                        {"name", rectangleName},
                        {"components", {
                            {"TransformComp", {
                                {"position", {
                                    {"x", (RenderSettings::AuthoredWidth - 128.0f) / 2.0f},
                                    {"y", (RenderSettings::AuthoredHeight - 64.0f) / 2.0f}
                                }},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            }},
                            {"RectangleComp", DefaultRectangleComponent()}
                        }}
                    });
                    selectedObject = current->ObjectCount() - 1;
                    objectNameInput.fill('\0');
                    std::copy(rectangleName.begin(), rectangleName.end(),
                              objectNameInput.begin());
                }
                if (ImGui::Button("Add UI")) {
                    current->AddObject({
                        {"name", "UI text"}, {"ui", true},
                        {"components", {
                            {"TransformComp", {
                                {"position", {{"x", 10}, {"y", 10}}},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            }},
                            {"TextComp", {
                                {"text", "Text"}, {"size", 24},
                                {"color", {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
                                {"margin", {{"x", 0.0}, {"y", 0.0}}}
                            }}
                        }}
                    });
                    const auto index = current->ObjectCount() - 1;
                    selectedObject = index;
                    objectNameInput.fill('\0');
                    const std::string objectName = "UI text";
                    std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
                }
        if (ImGui::Button("Add hotspot")) {
                    current->AddObject({
                {"name", "Hotspot"},
                {"components", {
                    {"TransformComp", {
                        {"position", {{"x", 0}, {"y", 0}}},
                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                    }},
                    {"ColliderComp", {{"width", 64.0}, {"height", 64.0}}},
                    {"HotspotComp", {{"interaction", nullptr}}}
                }}
            });
            const auto index = current->ObjectCount() - 1;
            selectedObject = index;
            objectNameInput.fill('\0');
            const std::string objectName = "Hotspot";
            std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
        }
        ImGui::SameLine();
        if (ImGui::Button("Add interaction")) {
            current->AddObject({
                {"name", "Interaction"},
                {"components", {{"InteractionComp", {{"actions", nlohmann::json::array()}}}}}
            });
            const auto index = current->ObjectCount() - 1;
            selectedObject = index;
            objectNameInput.fill('\0');
            const std::string objectName = "Interaction";
            std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
        }
        if (ImGui::Button("Add inventory item")) {
            const auto inventoryItems = uiEditor.InventoryTargets();
            const int itemId = inventoryItems.empty() ? 0 : inventoryItems.front().first;
            nlohmann::json spriteData = {
                {"atlas", "inv_items.png"},
                {"source", {{"x", 0}, {"y", 0}, {"width", 26}, {"height", 26}}}
            };
            if (itemId != 0) {
                if (auto iconSprite = LoadInventoryItemSprite(directory, itemId))
                    spriteData = std::move(*iconSprite);
            }
            const float spriteWidth = spriteData["source"]["width"].get<float>();
            const float spriteHeight = spriteData["source"]["height"].get<float>();
            const std::string itemName = "Inventory item";
            current->AddObject({
                {"name", itemName},
                {"components", {
                    {"TransformComp", {
                        {"position", {
                            {"x", (RenderSettings::AuthoredWidth - spriteWidth) / 2.0f},
                            {"y", (RenderSettings::AuthoredHeight - spriteHeight) / 2.0f}
                        }},
                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                    }},
                    {"Sprite", std::move(spriteData)},
                    {"InventoryItemComp", {{"item", itemId}}}
                }}
            });
            selectedObject = current->ObjectCount() - 1;
            objectNameInput.fill('\0');
            std::copy_n(itemName.begin(), std::min(itemName.size(), objectNameInput.size() - 1),
                        objectNameInput.begin());
        }
        if (sceneLightCount >= RenderSettings::MaxSceneLights)
            ImGui::TextDisabled("Maximum of %d scene lights.", RenderSettings::MaxSceneLights);
        if (selectedObject) {
            auto object = current->Object(*selectedObject);
            bool inspectorDisableScopeOpen = false;
            try {
                auto edited = LoadObject(object);
                ImGui::SeparatorText("Object");
                bool changed = false;
                const bool locked = object.value("locked", false);
                if (locked)
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1), "Object is locked.");
                ImGui::BeginDisabled(locked);
                inspectorDisableScopeOpen = true;
                bool uiLayer = edited.entity->uiLayer;
                if (ImGui::Checkbox("UI layer", &uiLayer)) {
                    object["ui"] = uiLayer;
                    changed = true;
                }
                if (object.contains("parent")) {
                    const auto parentIndex = current->Parent(*selectedObject);
                    if (parentIndex) {
                        auto parent = parentIndex;
                        while (parent) {
                            const auto parentData = current->Object(*parent);
                            if (parentData.value("ui", false)) {
                                ImGui::TextWrapped("UI layer inherited from parent.");
                                break;
                            }
                            parent = current->Parent(*parent);
                        }
                    }
                }
                if (ImGui::InputText("Object name", objectNameInput.data(), objectNameInput.size())) {
                    if (objectNameInput[0] != '\0' &&
                        std::string(objectNameInput.data()).find_first_not_of(" \t\r\n") != std::string::npos) {
                        edited.entity->name = objectNameInput.data();
                        object["name"] = edited.entity->name;
                        changed = true;
                    }
                }
                if (ImGui::IsItemDeactivatedAfterEdit() &&
                    std::string(objectNameInput.data()).find_first_not_of(" \t\r\n") == std::string::npos) {
                    message = "Object name cannot be blank.";
                    messageIsError = true;
                    objectNameInput.fill('\0');
                    std::copy(edited.entity->name.begin(), edited.entity->name.end(), objectNameInput.begin());
                }
                const auto objectParent = current->Parent(*selectedObject);
                std::string parentLabel = "None";
                if (objectParent) {
                    try {
                        parentLabel = LoadObject(current->Object(*objectParent)).entity->name;
                    } catch (const std::runtime_error&) {
                        parentLabel = "Invalid object";
                    }
                }
                if (ImGui::BeginCombo("Parent", parentLabel.c_str())) {
                    if (ImGui::Selectable("None", !objectParent) && objectParent) {
                        try {
                            ReparentPreservingWorld(*current, *selectedObject, std::nullopt);
                            object = current->Object(*selectedObject);
                            edited = LoadObject(object);
                        } catch (const std::exception& e) {
                            message = e.what();
                            messageIsError = true;
                        }
                    }
                    for (std::size_t i = 0; i < current->ObjectCount(); ++i) {
                        if (i == *selectedObject)
                            continue;
                        std::string candidateName = "Invalid object";
                        try {
                            candidateName = LoadObject(current->Object(i)).entity->name;
                        } catch (const std::runtime_error&) {
                        }
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(candidateName.c_str(),
                                              objectParent && *objectParent == i) &&
                            (!objectParent || *objectParent != i)) {
                            try {
                                ReparentPreservingWorld(*current, *selectedObject, i);
                                object = current->Object(*selectedObject);
                                edited = LoadObject(object);
                            } catch (const std::exception& e) {
                                message = e.what();
                                messageIsError = true;
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                auto& components = object["components"];
                if (components.contains("HotspotComp")) {
                    ImGui::SeparatorText("Hotspot");
                    auto& hotspotData = components["HotspotComp"];
                    bool checkVariable = hotspotData.value("check_variable", false);
                    if (ImGui::Checkbox("Check variable", &checkVariable)) {
                        hotspotData["check_variable"] = checkVariable;
                        if (checkVariable) {
                            if (!hotspotData.contains("variable") ||
                                !hotspotData["variable"].is_object()) {
                                hotspotData["variable"] = {{"scope", "global"}, {"id", ""}};
                            }
                            if (!hotspotData.contains("fail_message") ||
                                !hotspotData["fail_message"].is_string()) {
                                hotspotData["fail_message"] = "";
                            }
                        }
                        changed = true;
                    }
                    if (checkVariable) {
                        if (!hotspotData.contains("variable") ||
                            !hotspotData["variable"].is_object())
                            hotspotData["variable"] = {{"scope", "global"}, {"id", ""}};
                        ImGui::PushID("HotspotVariable");
                        DrawVariableReference(hotspotData["variable"], &globalVariables,
                                              &sceneVariableDraft, changed);
                        ImGui::PopID();
                    }
                    bool requireItem = hotspotData.value("require_item", false);
                    if (ImGui::Checkbox("Require selected item", &requireItem)) {
                        hotspotData["require_item"] = requireItem;
                        if (requireItem) {
                            if (!hotspotData.contains("item") ||
                                !hotspotData["item"].is_number_integer()) {
                                const auto items = uiEditor.InventoryTargets();
                                hotspotData["item"] = items.empty() ? 0 : items.front().first;
                            }
                            if (!hotspotData.contains("consume_item") ||
                                !hotspotData["consume_item"].is_boolean())
                                hotspotData["consume_item"] = false;
                            if (!hotspotData.contains("fail_message") ||
                                !hotspotData["fail_message"].is_string())
                                hotspotData["fail_message"] = "";
                        }
                        changed = true;
                    }
                    if (requireItem) {
                        const int selectedItemId = hotspotData.value("item", 0);
                        const auto inventoryItems = uiEditor.InventoryTargets();
                        const auto found = std::find_if(
                            inventoryItems.begin(), inventoryItems.end(),
                            [selectedItemId](const auto& entry) { return entry.first == selectedItemId; });
                        const std::string itemPreview = found != inventoryItems.end()
                            ? found->second + " (" + std::to_string(found->first) + ")"
                            : (selectedItemId == 0 ? std::string("Choose item")
                                                   : "Missing item (" + std::to_string(selectedItemId) + ")");
                        if (ImGui::BeginCombo("Required item", itemPreview.c_str())) {
                            for (const auto& entry : inventoryItems) {
                                const std::string option =
                                    entry.second + " (" + std::to_string(entry.first) + ")";
                                ImGui::PushID(entry.first);
                                if (ImGui::Selectable(option.c_str(), entry.first == selectedItemId)) {
                                    hotspotData["item"] = entry.first;
                                    changed = true;
                                }
                                ImGui::PopID();
                            }
                            if (inventoryItems.empty())
                                ImGui::TextWrapped("Add items in Window > Inventory Item Editor.");
                            ImGui::EndCombo();
                        }
                        bool consumeItem = hotspotData.value("consume_item", false);
                        if (ImGui::Checkbox("Consume item on success", &consumeItem)) {
                            hotspotData["consume_item"] = consumeItem;
                            changed = true;
                        }
                    }
                    auto& target = hotspotData["interaction"];
                    std::optional<std::size_t> interactionIndex;
                    if (target.is_number_integer())
                        interactionIndex = target.get<std::size_t>();
                    std::string label = "None";
                    if (interactionIndex && *interactionIndex < current->ObjectCount()) {
                        const auto candidate = current->Object(*interactionIndex);
                        label = candidate["name"].get<std::string>() + " (" +
                                std::to_string(*interactionIndex) + ")";
                    } else if (interactionIndex) {
                        label = "Invalid interaction";
                    }
                    const char* interactionLabel =
                        (checkVariable || requireItem) ? "Success interaction" : "Interaction";
                    if (ImGui::BeginCombo(interactionLabel, label.c_str())) {
                        if (ImGui::Selectable("None", !interactionIndex)) {
                            target = nullptr;
                            changed = true;
                        }
                        for (std::size_t i = 0; i < current->ObjectCount(); ++i) {
                            const auto candidate = current->Object(i);
                            if (!candidate.is_object() || !candidate.contains("components") ||
                                !candidate["components"].is_object() ||
                                !candidate["components"].contains("InteractionComp"))
                                continue;
                            const auto candidateLabel = candidate["name"].get<std::string>() +
                                " (" + std::to_string(i) + ")";
                            ImGui::PushID(static_cast<int>(i));
                            if (ImGui::Selectable(candidateLabel.c_str(),
                                                  interactionIndex && *interactionIndex == i)) {
                                target = i;
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if (checkVariable || requireItem) {
                        const auto failMessage =
                            hotspotData.value("fail_message", std::string{});
                        textInput.fill('\0');
                        std::copy_n(failMessage.begin(),
                                    std::min(failMessage.size(), textInput.size() - 1),
                                    textInput.begin());
                        if (ImGui::InputText("Fail message", textInput.data(),
                                             textInput.size())) {
                            hotspotData["fail_message"] = textInput.data();
                            changed = true;
                        }
                        ImGui::TextDisabled("Shown as a subtitle when the condition is not met.");
                    }
                    bool disableOnSuccess = hotspotData.value("disable_on_success", false);
                    if (ImGui::Checkbox("Disable hotspot on success", &disableOnSuccess)) {
                        hotspotData["disable_on_success"] = disableOnSuccess;
                        changed = true;
                    }
                    ImGui::SeparatorText("Item interactions");
                    if (!hotspotData.contains("item_interactions") ||
                        !hotspotData["item_interactions"].is_array())
                        hotspotData["item_interactions"] = nlohmann::json::array();
                    auto& itemInteractions = hotspotData["item_interactions"];
                    const auto inventoryItems = uiEditor.InventoryTargets();
                    std::optional<std::size_t> removeItemInteraction;
                    for (std::size_t entryIdx = 0; entryIdx < itemInteractions.size(); ++entryIdx) {
                        auto& entry = itemInteractions[entryIdx];
                        if (!entry.is_object())
                            continue;
                        ImGui::PushID(static_cast<int>(entryIdx));
                        ImGui::BeginGroup();
                        const int entryItemId = entry.value("item", 0);
                        const auto foundItem = std::find_if(
                            inventoryItems.begin(), inventoryItems.end(),
                            [entryItemId](const auto& item) { return item.first == entryItemId; });
                        const std::string itemLabel = foundItem != inventoryItems.end()
                            ? foundItem->second + " (" + std::to_string(foundItem->first) + ")"
                            : (entryItemId == 0 ? std::string("Choose item")
                                                : "Missing item (" + std::to_string(entryItemId) + ")");
                        if (ImGui::BeginCombo("Item", itemLabel.c_str())) {
                            for (const auto& item : inventoryItems) {
                                const std::string option =
                                    item.second + " (" + std::to_string(item.first) + ")";
                                ImGui::PushID(item.first);
                                if (ImGui::Selectable(option.c_str(), item.first == entryItemId)) {
                                    entry["item"] = item.first;
                                    changed = true;
                                }
                                ImGui::PopID();
                            }
                            if (inventoryItems.empty())
                                ImGui::TextWrapped("Add items in Window > Inventory Item Editor.");
                            ImGui::EndCombo();
                        }
                        auto& entryTarget = entry["interaction"];
                        std::optional<std::size_t> entryInteractionIndex;
                        if (entryTarget.is_number_integer() && entryTarget.get<std::int64_t>() >= 0)
                            entryInteractionIndex = entryTarget.get<std::size_t>();
                        std::string entryInteractionLabel = "None";
                        if (entryInteractionIndex && *entryInteractionIndex < current->ObjectCount()) {
                            const auto candidate = current->Object(*entryInteractionIndex);
                            entryInteractionLabel = candidate["name"].get<std::string>() + " (" +
                                                    std::to_string(*entryInteractionIndex) + ")";
                        } else if (entryInteractionIndex) {
                            entryInteractionLabel = "Invalid interaction";
                        }
                        if (ImGui::BeginCombo("Interaction##ItemInteraction",
                                              entryInteractionLabel.c_str())) {
                            if (ImGui::Selectable("None", !entryInteractionIndex)) {
                                entryTarget = nullptr;
                                changed = true;
                            }
                            for (std::size_t i = 0; i < current->ObjectCount(); ++i) {
                                const auto candidate = current->Object(i);
                                if (!candidate.is_object() || !candidate.contains("components") ||
                                    !candidate["components"].is_object() ||
                                    !candidate["components"].contains("InteractionComp"))
                                    continue;
                                const auto candidateLabel = candidate["name"].get<std::string>() +
                                    " (" + std::to_string(i) + ")";
                                ImGui::PushID(static_cast<int>(i));
                                if (ImGui::Selectable(candidateLabel.c_str(),
                                                      entryInteractionIndex &&
                                                          *entryInteractionIndex == i)) {
                                    entryTarget = i;
                                    changed = true;
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndCombo();
                        }
                        bool entryConsume = entry.value("consume_item", false);
                        if (ImGui::Checkbox("Consume item", &entryConsume)) {
                            entry["consume_item"] = entryConsume;
                            changed = true;
                        }
                        bool entryDisable = entry.value("disable_on_success", false);
                        if (ImGui::Checkbox("Disable hotspot", &entryDisable)) {
                            entry["disable_on_success"] = entryDisable;
                            changed = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove"))
                            removeItemInteraction = entryIdx;
                        ImGui::EndGroup();
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    if (removeItemInteraction) {
                        itemInteractions.erase(itemInteractions.begin() +
                                               static_cast<std::ptrdiff_t>(*removeItemInteraction));
                        changed = true;
                    }
                    if (ImGui::Button("Add item interaction")) {
                        const int defaultItemId =
                            inventoryItems.empty() ? 0 : inventoryItems.front().first;
                        itemInteractions.push_back({
                            {"item", defaultItemId},
                            {"interaction", nullptr},
                            {"consume_item", false},
                            {"disable_on_success", false}
                        });
                        changed = true;
                    }
                    ImGui::TextWrapped("The rectangle is visible only in the editor.");
                }
                if (components.contains("InteractionComp")) {
                    bool runOnSceneStart =
                        components["InteractionComp"].value("run_on_scene_start", false);
                    if (ImGui::Checkbox("Run when scene starts", &runOnSceneStart)) {
                        components["InteractionComp"]["run_on_scene_start"] = runOnSceneStart;
                        changed = true;
                    }
                    ImGui::TextDisabled("Runs once after the scene is loaded.");
                    if (ImGui::Button("Open node editor"))
                        showInteractionGraph = true;
                    auto& actions = components["InteractionComp"]["actions"];
                    auto& game = Game::get_Instance();
                    const auto scenes = game.scene_manager
                        ? game.scene_manager->RegisteredScenes() : std::vector<std::filesystem::path>{};
                    const auto actionHeader = "Actions (" + std::to_string(actions.size()) +
                                              ")##SceneInteractionActions";
                    if (ImGui::CollapsingHeader(actionHeader.c_str(),
                                                ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (std::size_t i = 0; i < actions.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::BeginGroup();
                            ImGui::Dummy(ImVec2(0, 4));
                            ImGui::Indent(6);
                            ImGui::Text("Action %zu", i + 1);
                            if (actions[i]["type"] == "ChangeScene") {
                                if (!game.scene_manager)
                                    ImGui::TextWrapped("Scene manager is unavailable.");
                                else
                                    changed |= DrawActionTarget(actions[i]["scene"], scenes);
                            } else {
                                changed |= DrawUIAction(actions[i], uiEditor, &*current,
                                                        &globalVariables,
                                                        &sceneVariableDraft);
                            }
                            const bool removeAction = ImGui::Button("Remove action");
                            ImGui::Unindent(6);
                            ImGui::Dummy(ImVec2(0, 4));
                            ImGui::EndGroup();
                            const auto min = ImGui::GetItemRectMin();
                            const auto max = ImGui::GetItemRectMax();
                            ImGui::GetWindowDrawList()->AddRect(
                                ImVec2(min.x - 4, min.y), ImVec2(max.x + 4, max.y),
                                IM_COL32(105, 115, 135, 220), 4.0f);
                            ImGui::Dummy(ImVec2(0, 6));
                            ImGui::PopID();
                            if (removeAction) {
                                actions.erase(i);
                                changed = true;
                                break;
                            }
                        }
                        if (DrawAddInteractionAction(actions, uiEditor, &*current,
                                                     game.scene_manager != nullptr,
                                                     CanAppendAction(actions)))
                            changed = true;
                        if (!CanAppendAction(actions))
                            ImGui::TextDisabled("Change Scene ends the action list.");
                        ImGui::TextWrapped("Changing scene ends the current interaction.");
                    }
                }
                if (ImGui::BeginCombo("Add component", "Choose component")) {
                    if (!components.contains("TransformComp") &&
                        ImGui::Selectable("Transform")) {
                        components["TransformComp"] = {
                            {"position", {{"x", 0}, {"y", 0}}},
                            {"scale", {{"x", 1.0}, {"y", 1.0}}}
                        };
                        changed = true;
                    }
                    if (!components.contains("Sprite") && ImGui::Selectable("Sprite")) {
                        components["Sprite"] = {
                            {"atlas", "atlas_1.png"},
                            {"source", {{"x", 0}, {"y", 0}, {"width", 1}, {"height", 1}}}
                        };
                        changed = true;
                    }
                    if (!components.contains("Panel_Sprite") && ImGui::Selectable("Panel sprite")) {
                        if (!components.contains("TransformComp")) {
                            components["TransformComp"] = {
                                {"position", {{"x", 0}, {"y", 0}}},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            };
                        }
                        if (!components.contains("Sprite")) {
                            components["Sprite"] = {
                                {"atlas", "ui_page.png"},
                                {"source", {{"x", 0}, {"y", 0}, {"width", 32}, {"height", 32}}}
                            };
                        }
                        components["Panel_Sprite"] = {
                            {"margin", 8.0}, {"width", 128.0}, {"height", 64.0}
                        };
                        changed = true;
                    }
                    if (!components.contains("RectangleComp") &&
                        ImGui::Selectable("Colored rectangle")) {
                        if (!components.contains("TransformComp")) {
                            components["TransformComp"] = {
                                {"position", {{"x", 0}, {"y", 0}}},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            };
                        }
                        components["RectangleComp"] = DefaultRectangleComponent();
                        changed = true;
                    }
                    if (!components.contains("ColliderComp") && ImGui::Selectable("Box collider")) {
                        components["ColliderComp"] = {{"width", 64.0}, {"height", 64.0}};
                        changed = true;
                    }
                    if (!components.contains("CircleCol") && ImGui::Selectable("Circle collider")) {
                        components["CircleCol"] = {{"x", 0.0}, {"y", 0.0}, {"radius", 32.0}};
                        changed = true;
                    }
                    if (!components.contains("TextComp") && ImGui::Selectable("Text")) {
                        if (!components.contains("TransformComp")) {
                            components["TransformComp"] = {
                                {"position", {{"x", 0}, {"y", 0}}},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            };
                        }
                        components["TextComp"] = {
                            {"text", "Text"}, {"size", 24},
                            {"color", {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
                            {"margin", {{"x", 0.0}, {"y", 0.0}}}
                        };
                        changed = true;
                    }
                    if (!components.contains("LightComp") &&
                        sceneLightCount < RenderSettings::MaxSceneLights &&
                        ImGui::Selectable("Point light")) {
                        if (!components.contains("TransformComp")) {
                            components["TransformComp"] = {
                                {"position", {{"x", 0}, {"y", 0}}},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            };
                        }
                        components["LightComp"] = {
                            {"intensity", 1.25},
                            {"radius", 64.0},
                            {"color", {{"r", 255}, {"g", 220}, {"b", 150}, {"a", 255}}}
                        };
                        changed = true;
                    }
                    if (!components.contains("AudioComp") && ImGui::Selectable("Audio")) {
                            components["AudioComp"] = {
                                {"asset", "bg_music.mp3"}, {"volume", 1.0},
                                {"play_on_start", false}
                            };
                            changed = true;
                    }
                    if (!components.contains("InventoryItemComp") &&
                        ImGui::Selectable("Inventory item")) {
                        const auto inventoryItems = uiEditor.InventoryTargets();
                        const int itemId = inventoryItems.empty() ? 0 : inventoryItems.front().first;
                        if (!components.contains("TransformComp")) {
                            components["TransformComp"] = {
                                {"position", {{"x", 0}, {"y", 0}}},
                                {"scale", {{"x", 1.0}, {"y", 1.0}}}
                            };
                        }
                        if (!components.contains("Sprite")) {
                            if (auto iconSprite = itemId != 0
                                    ? LoadInventoryItemSprite(directory, itemId)
                                    : std::nullopt) {
                                components["Sprite"] = std::move(*iconSprite);
                            } else {
                                components["Sprite"] = {
                                    {"atlas", "inv_items.png"},
                                    {"source", {{"x", 0}, {"y", 0}, {"width", 26}, {"height", 26}}}
                                };
                            }
                        }
                        components["InventoryItemComp"] = {{"item", itemId}};
                        changed = true;
                    }
                    if (!components.contains("ScriptComp") && ImGui::Selectable("Lua script")) {
                        const auto scripts = ScriptEngine::AvailableScripts();
                        components["ScriptComp"] = {
                            {"script", scripts.empty() ? std::string{} : scripts.front()}
                        };
                        changed = true;
                    }
                    ImGui::EndCombo();
                }
                if (auto* transform = edited.entity->getComponent<TransformComp>()) {
                    ImGui::SeparatorText("Transform");
                    float position[] = {transform->position.x, transform->position.y};
                    const char* positionLabel = objectParent ? "Local position" : "Position";
                    if (DragFloat2Field(positionLabel, position, 0.1f, "%.2f")) {
                        transform->position = {position[0], position[1]};
                        components["TransformComp"]["position"]["x"] = transform->position.x;
                        components["TransformComp"]["position"]["y"] = transform->position.y;
                        changed = true;
                    }
                    float scale[] = {transform->scale.x, transform->scale.y};
                    if (DragFloat2Field("Scale", scale, 0.01f, "%.2f")) {
                        transform->scale = {scale[0], scale[1]};
                        components["TransformComp"]["scale"]["x"] = transform->scale.x;
                        components["TransformComp"]["scale"]["y"] = transform->scale.y;
                        changed = true;
                    }
                    if (ImGui::DragFloat("Rotation", &transform->rotation,
                                         0.1f, 0.0f, 0.0f, "%.1f")) {
                        components["TransformComp"]["rotation"] = transform->rotation;
                        changed = true;
                    }
                    ImGui::BeginDisabled(components.contains("HotspotComp"));
                    if (components.contains("TransformComp") && ImGui::Button("Remove Transform")) {
                        components.erase("TransformComp");
                        changed = true;
                    }
                    ImGui::EndDisabled();
                }
                if (auto* sprite = edited.entity->getComponent<Sprite>()) {
                    ImGui::SeparatorText("Sprite");
                    if (ImGui::BeginCombo("Atlas", edited.atlas.c_str())) {
                        const auto atlases = ResourcesLoader::AvailableTextures();
                        for (const auto& atlas : atlases) {
                            if (ImGui::Selectable(atlas.c_str(), edited.atlas == atlas)) {
                                std::string textureError;
                                const auto* texture = ResourcesLoader::GetTexture(atlas, &textureError);
                                if (texture) {
                                    edited.atlas = atlas;
                                    sprite->texture = *texture;
                                    components["Sprite"]["atlas"] = edited.atlas;
                                    components["Sprite"].erase("frame");
                                    changed = true;
                                } else {
                                    ReportError(textureError);
                                }
                            }
                        }
                        if (atlases.empty())
                            ImGui::TextWrapped("Add PNG textures to the project resource directory.");
                        ImGui::EndCombo();
                    }
                    auto& source = components["Sprite"]["source"];
                    std::string frameError;
                    const auto spriteFrames = SpriteAtlasFrames(edited.atlas, &frameError);
                    const auto currentFrame = components["Sprite"].value(
                        "frame", std::string{});
                    if (!spriteFrames.empty()) {
                        std::string frameLabel = currentFrame.empty()
                            ? "Choose sprite frame" : currentFrame;
                        if (currentFrame.empty()) {
                            for (const auto& frame : spriteFrames)
                                if (frame.source.x == sprite->src_rec.x &&
                                    frame.source.y == sprite->src_rec.y &&
                                    frame.source.width == sprite->src_rec.width &&
                                    frame.source.height == sprite->src_rec.height) {
                                    frameLabel = frame.name;
                                    break;
                                }
                        }
                        if (ImGui::BeginCombo("Sprite frame", frameLabel.c_str())) {
                            for (const auto& frame : spriteFrames) {
                                if (ImGui::Selectable(frame.name.c_str(),
                                                      frame.name == currentFrame)) {
                                    sprite->src_rec = frame.source;
                                    components["Sprite"]["frame"] = frame.name;
                                    source = {
                                        {"x", static_cast<int>(frame.source.x)},
                                        {"y", static_cast<int>(frame.source.y)},
                                        {"width", static_cast<int>(frame.source.width)},
                                        {"height", static_cast<int>(frame.source.height)}
                                    };
                                    changed = true;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    } else if (!frameError.empty()) {
                        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", frameError.c_str());
                    } else {
                        ImGui::TextDisabled("This atlas has no JSON frame list; edit the source manually.");
                    }
                    int origin[] = {source["x"].get<int>(), source["y"].get<int>()};
                    if (DragInt2Field("Atlas XY", origin, 1.0f)) {
                        origin[0] = std::max(0, origin[0]);
                        origin[1] = std::max(0, origin[1]);
                        sprite->src_rec.x = static_cast<float>(origin[0]);
                        sprite->src_rec.y = static_cast<float>(origin[1]);
                        source["x"] = origin[0];
                        source["y"] = origin[1];
                        components["Sprite"].erase("frame");
                        changed = true;
                    }
                    int size[] = {source["width"].get<int>(), source["height"].get<int>()};
                    if (DragInt2Field("Atlas size", size, 1.0f)) {
                        size[0] = std::max(1, size[0]);
                        size[1] = std::max(1, size[1]);
                        sprite->src_rec.width = static_cast<float>(size[0]);
                        sprite->src_rec.height = static_cast<float>(size[1]);
                        source["width"] = size[0];
                        source["height"] = size[1];
                        components["Sprite"].erase("frame");
                        changed = true;
                    }
                    float opacity = components["Sprite"].value("opacity", sprite->alpha);
                    if (ImGui::DragFloat("Opacity", &opacity, 0.01f, 0.0f, 1.0f,
                                         "%.2f")) {
                        opacity = std::clamp(opacity, 0.0f, 1.0f);
                        components["Sprite"]["opacity"] = opacity;
                        sprite->alpha = opacity;
                        changed = true;
                    }
                    bool lensOnly = components["Sprite"].value("lens_only", sprite->lensOnly);
                    if (ImGui::Checkbox("Visible only under lens", &lensOnly)) {
                        sprite->lensOnly = lensOnly;
                        if (lensOnly)
                            components["Sprite"]["lens_only"] = true;
                        else
                            components["Sprite"].erase("lens_only");
                        changed = true;
                    }
                    const auto offset = components["Sprite"].value(
                        "offset", nlohmann::json::object());
                    float spriteOffset[] = {
                        offset.value("x", 0.0f), offset.value("y", 0.0f)
                    };
                    if (DragFloat2Field("Local offset", spriteOffset, 0.1f, "%.2f")) {
                        sprite->offset = {spriteOffset[0], spriteOffset[1]};
                        components["Sprite"]["offset"] = {
                            {"x", spriteOffset[0]}, {"y", spriteOffset[1]}
                        };
                        changed = true;
                    }
                    int renderOrder = components["Sprite"].value(
                        "render_order", sprite->drawOrder);
                    if (ImGui::DragInt("Render order", &renderOrder)) {
                        components["Sprite"]["render_order"] = renderOrder;
                        edited.entity->setDrawOrder(sprite, renderOrder);
                        changed = true;
                    }
                    if (!SourceFits(*sprite))
                        ImGui::TextWrapped("Atlas unavailable or rectangle outside atlas.");
                    ImGui::BeginDisabled(components.contains("InventoryItemComp"));
                    if (ImGui::Button("Remove Sprite")) {
                        if (components.contains("LightComp") &&
                            components["LightComp"].value("use_sprite_shape", false)) {
                            components["LightComp"]["use_sprite_shape"] = false;
                            components["LightComp"]["also_use_point_light"] = false;
                            if (auto* light = edited.entity->getComponent<LightComp>()) {
                                light->useSpriteShape = false;
                                light->alsoUsePointLight = false;
                            }
                        }
                        components.erase("Sprite");
                        changed = true;
                    }
                    ImGui::EndDisabled();
                }
                if (auto* panel = edited.entity->getComponent<Panel_Sprite>()) {
                    ImGui::SeparatorText("Panel sprite");
                    float margin = panel->info.left;
                    float size[] = {panel->targetRect.width, panel->targetRect.height};
                    if (ImGui::DragFloat("Nine-patch margin", &margin,
                                         0.1f, 0.0f, 0.0f, "%.1f")) {
                        margin = std::max(0.0f, margin);
                        components["Panel_Sprite"]["margin"] = margin;
                        changed = true;
                    }
                    if (DragFloat2Field("Panel size", size, 1.0f, "%.1f")) {
                        size[0] = std::max(1.0f, size[0]);
                        size[1] = std::max(1.0f, size[1]);
                        components["Panel_Sprite"]["width"] = size[0];
                        components["Panel_Sprite"]["height"] = size[1];
                        changed = true;
                    }
                    if (ImGui::Button("Remove Panel sprite")) {
                        components.erase("Panel_Sprite");
                        changed = true;
                    }
                }
                if (auto* rectangle = edited.entity->getComponent<RectangleComp>()) {
                    ImGui::SeparatorText("Colored rectangle");
                    float size[] = {rectangle->width, rectangle->height};
                    if (DragFloat2Field("Size", size, 1.0f, "%.1f")) {
                        rectangle->width = std::max(1.0f, size[0]);
                        rectangle->height = std::max(1.0f, size[1]);
                        components["RectangleComp"]["width"] = rectangle->width;
                        components["RectangleComp"]["height"] = rectangle->height;
                        changed = true;
                    }
                    auto& color = components["RectangleComp"]["color"];
                    if (!color.is_object())
                        color = {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}};
                    float colorValue[] = {
                        color["r"].get<float>() / 255.0f,
                        color["g"].get<float>() / 255.0f,
                        color["b"].get<float>() / 255.0f,
                        color["a"].get<float>() / 255.0f
                    };
                    if (ImGui::ColorEdit4("Rectangle color", colorValue)) {
                        color["r"] = static_cast<int>(colorValue[0] * 255.0f + 0.5f);
                        color["g"] = static_cast<int>(colorValue[1] * 255.0f + 0.5f);
                        color["b"] = static_cast<int>(colorValue[2] * 255.0f + 0.5f);
                        color["a"] = static_cast<int>(colorValue[3] * 255.0f + 0.5f);
                        rectangle->color = {
                            static_cast<unsigned char>(color["r"].get<int>()),
                            static_cast<unsigned char>(color["g"].get<int>()),
                            static_cast<unsigned char>(color["b"].get<int>()),
                            static_cast<unsigned char>(color["a"].get<int>())
                        };
                        changed = true;
                    }
                    float opacity = rectangle->opacity;
                    if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 1.0f, "%.2f")) {
                        rectangle->opacity = std::clamp(opacity, 0.0f, 1.0f);
                        components["RectangleComp"]["opacity"] = rectangle->opacity;
                        changed = true;
                    }
                    int renderOrder = components["RectangleComp"].value(
                        "render_order", rectangle->drawOrder);
                    if (ImGui::DragInt("Render order", &renderOrder)) {
                        components["RectangleComp"]["render_order"] = renderOrder;
                        edited.entity->setDrawOrder(rectangle, renderOrder);
                        changed = true;
                    }
                    if (ImGui::Button("Remove rectangle")) {
                        components.erase("RectangleComp");
                        changed = true;
                    }
                }
                if (auto* collider = edited.entity->getComponent<ColliderComp>()) {
                    ImGui::SeparatorText("Box collider");
                    if (edited.entity->getComponent<Sprite>()) {
                        ImGui::TextWrapped("Bounds follow the Sprite/Panel size.");
                    } else {
                        float size[] = {
                            components["ColliderComp"]["width"].get<float>(),
                            components["ColliderComp"]["height"].get<float>()
                        };
                        if (DragFloat2Field("Collider size", size, 1.0f, "%.1f")) {
                            size[0] = std::max(1.0f, size[0]);
                            size[1] = std::max(1.0f, size[1]);
                            components["ColliderComp"]["width"] = size[0];
                            components["ColliderComp"]["height"] = size[1];
                            changed = true;
                        }
                    }
                    ImGui::BeginDisabled(components.contains("HotspotComp"));
                    if (ImGui::Button("Remove Box collider")) {
                        components.erase("ColliderComp");
                        changed = true;
                    }
                    ImGui::EndDisabled();
                }
                if (auto* circle = edited.entity->getComponent<CircleCol>()) {
                    ImGui::SeparatorText("Circle collider");
                    float radius = components["CircleCol"]["radius"].get<float>();
                    if (ImGui::DragFloat("Radius", &radius, 0.5f, 0.0f, 0.0f, "%.1f")) {
                        components["CircleCol"]["radius"] = std::max(1.0f, radius);
                        changed = true;
                    }
                    if (ImGui::Button("Remove Circle collider")) {
                        components.erase("CircleCol");
                        changed = true;
                    }
                }
                if (auto* textComponent = edited.entity->getComponent<TextComp>()) {
                    ImGui::SeparatorText("Text");
                    textInput.fill('\0');
                    const auto textValue = components["TextComp"]["text"].get<std::string>();
                    std::copy(textValue.begin(), textValue.end(), textInput.begin());
                    if (ImGui::InputText("Content", textInput.data(), textInput.size())) {
                        components["TextComp"]["text"] = textInput.data();
                        changed = true;
                    }
                    const auto& fontName = textComponent->FontName();
                    if (ImGui::BeginCombo("Font", fontName.empty() ? "Default" : fontName.c_str())) {
                        if (ImGui::Selectable("Default", fontName.empty())) {
                            components["TextComp"].erase("font");
                            changed = true;
                        }
                        for (const auto& font : ResourcesLoader::AvailableFonts()) {
                            if (ImGui::Selectable(font.c_str(), font == fontName)) {
                                std::string error;
                                if (ResourcesLoader::GetCustomFont(font, &error)) {
                                    components["TextComp"]["font"] = font;
                                    changed = true;
                                } else {
                                    ReportError(error);
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Refresh fonts"))
                        ResourcesLoader::ClearFontErrors();
                    ImGui::TextDisabled("Fonts: resource/fonts");
                    if (!fontName.empty()) {
                        std::string error;
                        if (!ResourcesLoader::GetCustomFont(fontName, &error))
                            ImGui::TextWrapped("%s", error.c_str());
                    }
                    int textSize = components["TextComp"]["size"].get<int>();
                    if (ImGui::DragInt("Font size", &textSize, 1.0f)) {
                        components["TextComp"]["size"] = std::clamp(textSize, 1, 512);
                        changed = true;
                    }
                    auto& color = components["TextComp"]["color"];
                    if (!color.is_object())
                        color = {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}};
                    float colorValue[] = {
                        color["r"].get<float>() / 255.0f,
                        color["g"].get<float>() / 255.0f,
                        color["b"].get<float>() / 255.0f,
                        color["a"].get<float>() / 255.0f
                    };
                    if (ImGui::ColorEdit4("Color", colorValue)) {
                        color["r"] = static_cast<int>(colorValue[0] * 255.0f + 0.5f);
                        color["g"] = static_cast<int>(colorValue[1] * 255.0f + 0.5f);
                        color["b"] = static_cast<int>(colorValue[2] * 255.0f + 0.5f);
                        color["a"] = static_cast<int>(colorValue[3] * 255.0f + 0.5f);
                        changed = true;
                    }
                    auto& margin = components["TextComp"]["margin"];
                    if (!margin.is_object())
                        margin = {{"x", 0.0}, {"y", 0.0}};
                    float textMargin[] = {margin["x"].get<float>(), margin["y"].get<float>()};
                    if (DragFloat2Field("Text offset", textMargin, 0.1f, "%.2f")) {
                        margin["x"] = textMargin[0];
                        margin["y"] = textMargin[1];
                        changed = true;
                    }
                    if (ImGui::Button("Remove Text")) {
                        components.erase("TextComp");
                        changed = true;
                    }
                }
                if (auto* light = edited.entity->getComponent<LightComp>()) {
                    ImGui::SeparatorText("Point light");
                    auto& lightData = components["LightComp"];
                    bool useSpriteShape = lightData.value(
                        "use_sprite_shape", light->useSpriteShape);
                    ImGui::BeginDisabled(!edited.entity->getComponent<Sprite>());
                    if (ImGui::Checkbox("Use Sprite as shape", &useSpriteShape)) {
                        lightData["use_sprite_shape"] = useSpriteShape;
                        light->useSpriteShape = useSpriteShape;
                        if (!useSpriteShape) {
                            lightData["also_use_point_light"] = false;
                            light->alsoUsePointLight = false;
                        }
                        changed = true;
                    }
                    ImGui::EndDisabled();
                    if (light->useSpriteShape) {
                        ImGui::TextWrapped("The Sprite alpha is additively composited over the scene.");
                        bool alsoUsePointLight = lightData.value(
                            "also_use_point_light", light->alsoUsePointLight);
                        if (ImGui::Checkbox("Also use point light", &alsoUsePointLight)) {
                            lightData["also_use_point_light"] = alsoUsePointLight;
                            light->alsoUsePointLight = alsoUsePointLight;
                            changed = true;
                        }
                    } else {
                        ImGui::TextWrapped("Radius is in scene units.");
                    }
                    float intensity = components["LightComp"]["intensity"].get<float>();
                    if (ImGui::DragFloat("Point light intensity", &intensity, 0.01f, 0.0f, 10.0f,
                                         "%.2f")) {
                        intensity = std::clamp(intensity, 0.0f, 10.0f);
                        components["LightComp"]["intensity"] = intensity;
                        light->intensity = intensity;
                        changed = true;
                    }
                    if (light->useSpriteShape) {
                        float spriteIntensity = lightData.value(
                            "sprite_intensity", light->spriteIntensity);
                        if (ImGui::DragFloat("Sprite light intensity", &spriteIntensity,
                                             0.01f, 0.0f, 10.0f, "%.2f")) {
                            spriteIntensity = std::clamp(spriteIntensity, 0.0f, 10.0f);
                            lightData["sprite_intensity"] = spriteIntensity;
                            light->spriteIntensity = spriteIntensity;
                            changed = true;
                        }
                    }
                    if (!light->useSpriteShape || light->alsoUsePointLight) {
                        float radius = components["LightComp"]["radius"].get<float>();
                        if (ImGui::DragFloat("Radius", &radius, 1.0f, 1.0f, 4096.0f,
                                             "%.1f")) {
                            radius = std::clamp(radius, 1.0f, 4096.0f);
                            components["LightComp"]["radius"] = radius;
                            light->radius = radius;
                            changed = true;
                        }
                    }
                    auto& color = components["LightComp"]["color"];
                    if (!color.is_object())
                        color = {{"r", 255}, {"g", 220}, {"b", 150}, {"a", 255}};
                    float colorValue[] = {
                        color["r"].get<float>() / 255.0f,
                        color["g"].get<float>() / 255.0f,
                        color["b"].get<float>() / 255.0f,
                        color["a"].get<float>() / 255.0f
                    };
                    if (ImGui::ColorEdit4("Light color", colorValue)) {
                        color["r"] = static_cast<int>(colorValue[0] * 255.0f + 0.5f);
                        color["g"] = static_cast<int>(colorValue[1] * 255.0f + 0.5f);
                        color["b"] = static_cast<int>(colorValue[2] * 255.0f + 0.5f);
                        color["a"] = static_cast<int>(colorValue[3] * 255.0f + 0.5f);
                        changed = true;
                    }
                    if (ImGui::Button("Remove Point light")) {
                        components.erase("LightComp");
                        changed = true;
                    }
                }
                if (components.contains("AudioComp")) {
                    ImGui::SeparatorText("Audio");
                    auto& audio = components["AudioComp"];
                    const auto asset = audio.value("asset", std::string{});
                    textInput.fill('\0');
                    std::copy_n(asset.begin(), std::min(asset.size(), textInput.size() - 1),
                                textInput.begin());
                    if (ImGui::InputText("Asset in resource", textInput.data(), textInput.size())) {
                        audio["asset"] = textInput.data();
                        changed = true;
                    }
                    ImGui::TextDisabled("Use a supported audio file path relative to resource/.");
                    float volume = audio.value("volume", 1.0f);
                    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                        audio["volume"] = std::clamp(volume, 0.0f, 1.0f);
                        changed = true;
                    }
                    bool playOnStart = audio.value("play_on_start", false);
                    if (ImGui::Checkbox("Play on start", &playOnStart)) {
                        audio["play_on_start"] = playOnStart;
                        changed = true;
                    }
                    if (ImGui::Button("Remove Audio")) {
                        components.erase("AudioComp");
                        changed = true;
                    }
                }
                if (components.contains("InventoryItemComp")) {
                    ImGui::SeparatorText("Inventory item");
                    auto& itemData = components["InventoryItemComp"];
                    const int selectedItemId = itemData.value("item", 0);
                    const auto inventoryItems = uiEditor.InventoryTargets();
                    const auto found = std::find_if(
                        inventoryItems.begin(), inventoryItems.end(),
                        [selectedItemId](const auto& entry) { return entry.first == selectedItemId; });
                    const std::string preview = found != inventoryItems.end()
                        ? found->second + " (" + std::to_string(found->first) + ")"
                        : (selectedItemId == 0 ? std::string("Choose item")
                                               : "Missing item (" + std::to_string(selectedItemId) + ")");
                    if (ImGui::BeginCombo("Inventory item", preview.c_str())) {
                        for (const auto& entry : inventoryItems) {
                            const std::string option =
                                entry.second + " (" + std::to_string(entry.first) + ")";
                            ImGui::PushID(entry.first);
                            if (ImGui::Selectable(option.c_str(), entry.first == selectedItemId)) {
                                itemData["item"] = entry.first;
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        if (inventoryItems.empty())
                            ImGui::TextWrapped("Add items in Window > Inventory Item Editor.");
                        ImGui::EndCombo();
                    }
                    if (selectedItemId != 0 && ImGui::Button("Use item icon as Sprite")) {
                        if (auto iconSprite = LoadInventoryItemSprite(directory, selectedItemId)) {
                            if (components.contains("Sprite") && components["Sprite"].is_object()) {
                                auto& spriteJson = components["Sprite"];
                                spriteJson["atlas"] = (*iconSprite)["atlas"];
                                spriteJson["source"] = (*iconSprite)["source"];
                                if (iconSprite->contains("frame"))
                                    spriteJson["frame"] = (*iconSprite)["frame"];
                                else
                                    spriteJson.erase("frame");
                            } else {
                                components["Sprite"] = std::move(*iconSprite);
                            }
                            changed = true;
                        }
                    }
                    ImGui::TextWrapped("Clicking this sprite in Play adds the selected item to the runtime inventory.");
                    if (ImGui::Button("Remove Inventory item")) {
                        components.erase("InventoryItemComp");
                        changed = true;
                    }
                }
                if (components.contains("ScriptComp")) {
                    ImGui::SeparatorText("Lua script");
                    auto& scriptData = components["ScriptComp"];
                    const auto selectedScript = scriptData.value("script", std::string{});
                    const auto scripts = ScriptEngine::AvailableScripts();
                    const std::string preview = selectedScript.empty()
                        ? "Choose script" : selectedScript;
                    if (ImGui::BeginCombo("Script file", preview.c_str())) {
                        for (const auto& script : scripts) {
                            if (ImGui::Selectable(script.c_str(), script == selectedScript)) {
                                scriptData["script"] = script;
                                changed = true;
                            }
                        }
                        if (scripts.empty())
                            ImGui::TextWrapped("No .lua files in project scripts/ folder yet.");
                        ImGui::EndCombo();
                    }
                    ImGui::BeginDisabled(selectedScript.empty());
                    if (ImGui::Button("Open script")) {
                        std::string openError;
                        if (!OpenScriptInExternalEditor(selectedScript, openError)) {
                            message = openError;
                            messageIsError = true;
                        }
                    }
                    ImGui::EndDisabled();
                    static std::array<char, 64> newScriptName{};
                    ImGui::SetNextItemWidth(140.0f);
                    ImGui::InputTextWithHint("##NewScriptName", "script_name",
                                             newScriptName.data(), newScriptName.size());
                    ImGui::SameLine();
                    if (ImGui::Button("New script")) {
                        try {
                            const auto created = ScriptEngine::CreateTemplateScript(newScriptName.data());
                            scriptData["script"] = created.filename().string();
                            newScriptName.fill('\0');
                            message = "Created script: " + created.filename().string();
                            messageIsError = false;
                            changed = true;
                        } catch (const std::exception& e) {
                            message = e.what();
                            messageIsError = true;
                        }
                    }
                    ImGui::TextDisabled("Callbacks: on_start(self), on_update(self, dt), on_click(self)");
                    if (!selectedScript.empty()) {
                        std::string inspectError;
                        const auto declaredProperties =
                            ScriptEngine::InspectProperties(selectedScript, &inspectError);
                        if (!inspectError.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f),
                                               "Script parse error: %s", inspectError.c_str());
                        } else if (!declaredProperties.empty()) {
                            ImGui::SeparatorText("Script properties");
                            if (!scriptData.contains("properties") ||
                                !scriptData["properties"].is_object())
                                scriptData["properties"] = nlohmann::json::object();
                            auto& savedProps = scriptData["properties"];
                            for (const auto& prop : declaredProperties) {
                                ImGui::PushID(prop.name.c_str());
                                const bool hasOverride = savedProps.contains(prop.name);
                                if (!prop.refType.empty()) {
                                    const auto requiredKey = ScriptEngine::RefTypeComponentKey(prop.refType);
                                    auto hasRequiredComponent = [&](std::size_t idx) {
                                        if (requiredKey.empty())
                                            return true;
                                        const auto candidate = current->Object(idx);
                                        return candidate.is_object() &&
                                               candidate.contains("components") &&
                                               candidate["components"].is_object() &&
                                               candidate["components"].contains(requiredKey);
                                    };
                                    std::optional<std::size_t> selectedTarget;
                                    if (hasOverride && savedProps[prop.name].is_object() &&
                                        savedProps[prop.name].contains("object")) {
                                        const auto& refVal = savedProps[prop.name]["object"];
                                        if (refVal.is_number_unsigned())
                                            selectedTarget = refVal.get<std::size_t>();
                                        else if (refVal.is_number_integer() && refVal.get<std::int64_t>() >= 0)
                                            selectedTarget = static_cast<std::size_t>(refVal.get<std::int64_t>());
                                    }
                                    const std::string refLabel =
                                        selectedTarget && *selectedTarget < current->ObjectCount()
                                            ? AudioObjectLabel(*current, *selectedTarget) + " (" + prop.refType + ")"
                                            : (selectedTarget ? ("Missing (" + prop.refType + ")")
                                                              : ("None (" + prop.refType + ")"));
                                    const std::string fieldLabel = prop.name + "##RefCombo";
                                    if (ImGui::BeginCombo(fieldLabel.c_str(), refLabel.c_str())) {
                                        if (ImGui::Selectable(("None (" + prop.refType + ")").c_str(),
                                                              !selectedTarget)) {
                                            savedProps.erase(prop.name);
                                            changed = true;
                                        }
                                        bool anyMatching = false;
                                        for (std::size_t i = 0; i < current->ObjectCount(); ++i) {
                                            if (!hasRequiredComponent(i))
                                                continue;
                                            anyMatching = true;
                                            const auto option = AudioObjectLabel(*current, i);
                                            if (ImGui::Selectable(option.c_str(),
                                                                  selectedTarget && *selectedTarget == i)) {
                                                savedProps[prop.name] = {{"object", i}};
                                                changed = true;
                                            }
                                        }
                                        if (!anyMatching)
                                            ImGui::TextDisabled("No objects with %s in scene.", prop.refType.c_str());
                                        ImGui::EndCombo();
                                    }
                                    if (ImGui::BeginDragDropTarget()) {
                                        if (const ImGuiPayload* payload =
                                                ImGui::AcceptDragDropPayload("DEVILWOOD_SCENE_OBJECT")) {
                                            const std::size_t droppedIndex =
                                                *static_cast<const std::size_t*>(payload->Data);
                                            if (droppedIndex < current->ObjectCount() &&
                                                hasRequiredComponent(droppedIndex)) {
                                                savedProps[prop.name] = {{"object", droppedIndex}};
                                                changed = true;
                                                message = "Assigned " + AudioObjectLabel(*current, droppedIndex) +
                                                          " to '" + prop.name + "'.";
                                                messageIsError = false;
                                            } else if (droppedIndex < current->ObjectCount()) {
                                                message = AudioObjectLabel(*current, droppedIndex) +
                                                          " does not have a " + prop.refType + " component.";
                                                messageIsError = true;
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }
                                    ImGui::SameLine();
                                    const bool isPickingThis =
                                        pickingScriptRef &&
                                        pickingScriptRef->objectIndex == *selectedObject &&
                                        pickingScriptRef->propertyName == prop.name;
                                    if (ImGui::SmallButton(isPickingThis ? "Cancel" : "Pick")) {
                                        if (isPickingThis) {
                                            pickingScriptRef.reset();
                                        } else {
                                            pickingScriptRef = ScriptRefPickTarget{
                                                *selectedObject, prop.name, prop.refType};
                                            message = "Click an object in Scene or Object List for '" +
                                                      prop.name + "' (" + prop.refType + "). Press Esc to cancel.";
                                            messageIsError = false;
                                        }
                                    }
                                    if (hasOverride) {
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton("Clear")) {
                                            savedProps.erase(prop.name);
                                            changed = true;
                                        }
                                    }
                                } else if (const auto* defaultBool = std::get_if<bool>(&prop.defaultValue)) {
                                    bool val = hasOverride && savedProps[prop.name].is_boolean()
                                        ? savedProps[prop.name].get<bool>() : *defaultBool;
                                    if (ImGui::Checkbox(prop.name.c_str(), &val)) {
                                        savedProps[prop.name] = val;
                                        changed = true;
                                    }
                                } else if (const auto* defaultNum = std::get_if<double>(&prop.defaultValue)) {
                                    float val = static_cast<float>(
                                        hasOverride && savedProps[prop.name].is_number()
                                            ? savedProps[prop.name].get<double>() : *defaultNum);
                                    if (ImGui::DragFloat(prop.name.c_str(), &val, 0.1f, 0.0f, 0.0f, "%.3g")) {
                                        savedProps[prop.name] = static_cast<double>(val);
                                        changed = true;
                                    }
                                } else {
                                    const auto& defaultStr = std::get<std::string>(prop.defaultValue);
                                    const std::string currentStr =
                                        hasOverride && savedProps[prop.name].is_string()
                                            ? savedProps[prop.name].get<std::string>() : defaultStr;
                                    std::array<char, 512> propBuf{};
                                    std::copy_n(currentStr.begin(),
                                                std::min(currentStr.size(), propBuf.size() - 1),
                                                propBuf.begin());
                                    if (ImGui::InputText(prop.name.c_str(), propBuf.data(), propBuf.size())) {
                                        savedProps[prop.name] = std::string(propBuf.data());
                                        changed = true;
                                    }
                                }
                                if (prop.refType.empty() && hasOverride) {
                                    ImGui::SameLine();
                                    if (ImGui::SmallButton("Reset")) {
                                        savedProps.erase(prop.name);
                                        changed = true;
                                    }
                                }
                                ImGui::PopID();
                            }
                        }
                    }
                    if (ImGui::Button("Remove Script")) {
                        components.erase("ScriptComp");
                        changed = true;
                    }
                }
                for (auto it = components.begin(); it != components.end(); ++it) {
                    if (it.key() != "TransformComp" && it.key() != "Sprite" &&
                        it.key() != "Panel_Sprite" && it.key() != "RectangleComp" &&
                        it.key() != "ColliderComp" &&
                        it.key() != "CircleCol" && it.key() != "TextComp" &&
                        it.key() != "LightComp" && it.key() != "AudioComp" &&
                        it.key() != "HotspotComp" &&
                        it.key() != "InteractionComp" &&
                        it.key() != "InventoryItemComp" &&
                        it.key() != "ScriptComp")
                        ImGui::TextWrapped("Unsupported component: %s (preserved)", it.key().c_str());
                }
                if (changed && !locked) {
                    try {
                        LoadObject(object);
                        current->SetObject(*selectedObject, object, ImGui::IsAnyItemActive());
                    } catch (const std::exception& e) {
                        message = e.what();
                        messageIsError = true;
                    }
                }
                ImGui::EndDisabled();
                inspectorDisableScopeOpen = false;
            } catch (const std::exception& e) {
                if (inspectorDisableScopeOpen)
                    ImGui::EndDisabled();
                ImGui::TextWrapped("Cannot edit object: %s", e.what());
            }
        }
        if (ImGui::Button("Save"))
            Save();
        ImGui::TextWrapped("Register this scene in the Scene Manager to load it in Play.");
    } else {
        ImGui::TextWrapped("No scene is open. Open or create one in the Scene List tab.");
    }
    ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Scene List")) {
        ImGui::TextWrapped("Directory: %s", directory.string().c_str());
        ImGui::InputText("Scene ID", idInput.data(), idInput.size());
        if (ImGui::Button("New scene"))
            Request(Action::New);
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            Refresh();

        ImGui::SeparatorText("Open scene");
        ImGui::BeginChild("SceneFiles", ImVec2(0, 160), ImGuiChildFlags_Borders);
        std::vector<std::filesystem::path> mainScenes;
        std::map<std::string, std::vector<std::filesystem::path>> popupScenes;
        std::vector<std::filesystem::path> orphanPopups;
        std::vector<std::pair<std::filesystem::path, std::string>> invalidScenes;
        for (const auto& info : cachedSceneFiles) {
            if (!info.error.empty()) {
                invalidScenes.emplace_back(info.path, info.error);
            } else if (info.type == SceneType::Main) {
                mainScenes.push_back(info.path);
            } else if (info.parentScene) {
                popupScenes[*info.parentScene].push_back(info.path);
            } else {
                orphanPopups.push_back(info.path);
            }
        }
        for (const auto& file : mainScenes) {
            const auto label = file.filename().string();
            ImGui::PushID(label.c_str());
            const bool expanded = ImGui::TreeNodeEx(
                "Scene", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth,
                "%s", label.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                Request(Action::Open, file);
            if (expanded) {
                const auto children = popupScenes.find(label);
                if (children != popupScenes.end()) {
                    for (const auto& popup : children->second) {
                        const auto popupLabel = popup.filename().string();
                        if (ImGui::Selectable(popupLabel.c_str(),
                                              current && current->Path() == popup))
                            Request(Action::Open, popup);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (!orphanPopups.empty() || !popupScenes.empty()) {
            for (const auto& entry : popupScenes) {
                const bool parentListed = std::any_of(mainScenes.begin(), mainScenes.end(),
                    [&entry](const auto& file) {
                        return file.filename().string() == entry.first;
                    });
                if (parentListed)
                    continue;
                if (ImGui::TreeNodeEx(entry.first.c_str(),
                                      ImGuiTreeNodeFlags_OpenOnArrow |
                                      ImGuiTreeNodeFlags_SpanAvailWidth,
                                      "Missing parent: %s", entry.first.c_str())) {
                    for (const auto& popup : entry.second) {
                        const auto popupLabel = popup.filename().string();
                        if (ImGui::Selectable(popupLabel.c_str(),
                                              current && current->Path() == popup))
                            Request(Action::Open, popup);
                    }
                    ImGui::TreePop();
                }
            }
        }
        for (const auto& file : orphanPopups) {
            const auto label = "Popup without parent: " + file.filename().string();
            if (ImGui::Selectable(label.c_str(), current && current->Path() == file))
                Request(Action::Open, file);
        }
        for (const auto& invalid : invalidScenes) {
            const auto label = "Invalid scene: " + invalid.first.filename().string();
            if (ImGui::Selectable(label.c_str(),
                                  current && current->Path() == invalid.first))
                Request(Action::Open, invalid.first);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", invalid.second.c_str());
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    }

    if (!message.empty()) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text,
                              messageIsError ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.5f, 1, 0.5f, 1));
        ImGui::TextWrapped("%s", message.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
    }

    if (sceneMode && showObjectList) {
        ImGui::SetNextWindowPos(ImVec2(20, 120), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(240, 460), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Object List", &showObjectList)) {
            if (!current) {
                ImGui::TextWrapped("Open a scene to view its objects.");
            } else {
                ImGui::Text("Objects: %zu", current->ObjectCount());
                ImGui::BeginChild("SceneObjects", ImVec2(0, 0), ImGuiChildFlags_Borders);
                for (std::size_t i = 0; i < current->ObjectCount(); ++i) {
                    std::string label = "Unsupported object";
                    std::string objectName;
                    const auto rawObj = current->Object(i);
                    if (rawObj.is_object() && rawObj.contains("name") && rawObj["name"].is_string()) {
                        objectName = rawObj["name"].get<std::string>();
                        label = objectName;
                    } else {
                        label = "Invalid or unsupported object";
                    }
                    std::size_t depth = 0;
                    auto parent = current->Parent(i);
                    while (parent) {
                        ++depth;
                        parent = current->Parent(*parent);
                    }
                    label.insert(0, depth * 2, ' ');
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::Selectable(label.c_str(), selectedObject && *selectedObject == i)) {
                        if (pickingScriptRef && pickingScriptRef->objectIndex < current->ObjectCount()) {
                            const auto requiredKey =
                                ScriptEngine::RefTypeComponentKey(pickingScriptRef->refType);
                            const auto pickedObj = current->Object(i);
                            const bool matches = requiredKey.empty() ||
                                (pickedObj.is_object() && pickedObj.contains("components") &&
                                 pickedObj["components"].is_object() &&
                                 pickedObj["components"].contains(requiredKey));
                            if (matches) {
                                auto ownerObj = current->Object(pickingScriptRef->objectIndex);
                                if (ownerObj.is_object() && ownerObj.contains("components") &&
                                    ownerObj["components"].is_object() &&
                                    ownerObj["components"].contains("ScriptComp")) {
                                    auto& scriptData = ownerObj["components"]["ScriptComp"];
                                    if (!scriptData.contains("properties") ||
                                        !scriptData["properties"].is_object())
                                        scriptData["properties"] = nlohmann::json::object();
                                    scriptData["properties"][pickingScriptRef->propertyName] = {{"object", i}};
                                    current->SetObject(pickingScriptRef->objectIndex, ownerObj);
                                    message = "Assigned " + AudioObjectLabel(*current, i) +
                                              " to '" + pickingScriptRef->propertyName + "'.";
                                    messageIsError = false;
                                }
                                pickingScriptRef.reset();
                            } else {
                                message = AudioObjectLabel(*current, i) + " does not have a " +
                                          pickingScriptRef->refType + " component.";
                                messageIsError = true;
                            }
                        } else {
                            selectedObject = i;
                            objectNameInput.fill('\0');
                            std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
                        }
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                        !pickingScriptRef) {
                        selectedObject = i;
                        objectNameInput.fill('\0');
                        std::copy_n(objectName.begin(),
                                    std::min(objectName.size(), objectNameInput.size() - 1),
                                    objectNameInput.begin());
                        showPanel = true;
                        focusCurrentSceneTab = true;
                        try {
                            if (EnsurePreviewGraph() && i < cachedPreviewGraph.objects.size() &&
                                cachedPreviewGraph.objects[i]) {
                                auto* targetObj = cachedPreviewGraph.objects[i];
                                Vector2 center{renderWidth * 0.5f, renderHeight * 0.5f};
                                Rectangle bounds{};
                                if (ObjectBounds(*targetObj, cachedPreviewData[i], bounds)) {
                                    center = {bounds.x + bounds.width * 0.5f,
                                              bounds.y + bounds.height * 0.5f};
                                    if (bounds.width > 1.0f && bounds.height > 1.0f) {
                                        const float fitX = (GetScreenWidth() * 0.45f) / bounds.width;
                                        const float fitY = (GetScreenHeight() * 0.45f) / bounds.height;
                                        sceneZoom = std::clamp(std::min(fitX, fitY), 0.75f, 3.0f);
                                    }
                                } else if (const auto* trans = targetObj->getComponent<TransformComp>()) {
                                    center = trans->getWorldPosition();
                                    sceneZoom = std::clamp(std::max(sceneZoom, 1.5f), 0.75f, 3.0f);
                                }
                                sceneOffset = {
                                    GetScreenWidth() * 0.5f - center.x * sceneZoom,
                                    GetScreenHeight() * 0.5f - center.y * sceneZoom
                                };
                            }
                        } catch (const std::exception&) {
                        }
                    }
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        const std::size_t payloadIndex = i;
                        ImGui::SetDragDropPayload("DEVILWOOD_SCENE_OBJECT", &payloadIndex, sizeof(payloadIndex));
                        ImGui::Text("%s", objectName.empty() ? label.c_str() : objectName.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        selectedObject = i;
                        contextObject = i;
                        openObjectContextMenu = true;
                        objectNameInput.fill('\0');
                        std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
                    }
                    ImGui::PopID();
                }
                if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    contextObject.reset();
                    openObjectContextMenu = true;
                }
                ImGui::EndChild();
                if (openObjectContextMenu) {
                    ImGui::OpenPopup("Scene object menu");
                    openObjectContextMenu = false;
                }
                if (ImGui::BeginPopup("Scene object menu")) {
                    auto selectCreatedObject = [&](const std::string& createdName) {
                        selectedObject = current->ObjectCount() - 1;
                        objectNameInput.fill('\0');
                        std::copy_n(createdName.begin(),
                                    std::min(createdName.size(), objectNameInput.size() - 1),
                                    objectNameInput.begin());
                    };
                    auto drawCreateItems = [&]() {
                        if (ImGui::MenuItem("Empty Object")) {
                            current->AddObject();
                            selectCreatedObject("Object");
                        }
                        const auto sceneLightCount = SceneLightCount(*current);
                        if (ImGui::MenuItem("Light", nullptr, false,
                                            sceneLightCount < RenderSettings::MaxSceneLights)) {
                            const std::string lightName =
                                "Light " + std::to_string(sceneLightCount + 1);
                            current->AddObject({
                                {"name", lightName},
                                {"components", {
                                    {"TransformComp", {
                                        {"position", {
                                            {"x", RenderSettings::AuthoredWidth / 2.0f},
                                            {"y", RenderSettings::AuthoredHeight / 2.0f}
                                        }},
                                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                                    }},
                                    {"LightComp", {
                                        {"intensity", 1.25},
                                        {"radius", 64.0},
                                        {"color", {{"r", 255}, {"g", 220}, {"b", 150}, {"a", 255}}}
                                    }}
                                }}
                            });
                            selectCreatedObject(lightName);
                        }
                        if (ImGui::MenuItem("Rectangle")) {
                            current->AddObject({
                                {"name", "Rectangle"},
                                {"components", {
                                    {"TransformComp", {
                                        {"position", {
                                            {"x", (RenderSettings::AuthoredWidth - 128.0f) / 2.0f},
                                            {"y", (RenderSettings::AuthoredHeight - 64.0f) / 2.0f}
                                        }},
                                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                                    }},
                                    {"RectangleComp", DefaultRectangleComponent()}
                                }}
                            });
                            selectCreatedObject("Rectangle");
                        }
                        if (ImGui::MenuItem("UI Text")) {
                            current->AddObject({
                                {"name", "UI text"}, {"ui", true},
                                {"components", {
                                    {"TransformComp", {
                                        {"position", {{"x", 10}, {"y", 10}}},
                                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                                    }},
                                    {"TextComp", {
                                        {"text", "Text"}, {"size", 24},
                                        {"color", {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
                                        {"margin", {{"x", 0.0}, {"y", 0.0}}}
                                    }}
                                }}
                            });
                            selectCreatedObject("UI text");
                        }
                        if (ImGui::MenuItem("Hotspot")) {
                            current->AddObject({
                                {"name", "Hotspot"},
                                {"components", {
                                    {"TransformComp", {
                                        {"position", {{"x", 0}, {"y", 0}}},
                                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                                    }},
                                    {"ColliderComp", {{"width", 64.0}, {"height", 64.0}}},
                                    {"HotspotComp", {{"interaction", nullptr}}}
                                }}
                            });
                            selectCreatedObject("Hotspot");
                        }
                        if (ImGui::MenuItem("Interaction")) {
                            current->AddObject({
                                {"name", "Interaction"},
                                {"components", {{"InteractionComp", {{"actions", nlohmann::json::array()}}}}}
                            });
                            selectCreatedObject("Interaction");
                        }
                        if (ImGui::MenuItem("Inventory Item")) {
                            const auto inventoryItems = uiEditor.InventoryTargets();
                            const int itemId = inventoryItems.empty() ? 0 : inventoryItems.front().first;
                            nlohmann::json spriteData = {
                                {"atlas", "inv_items.png"},
                                {"source", {{"x", 0}, {"y", 0}, {"width", 26}, {"height", 26}}}
                            };
                            if (itemId != 0) {
                                if (auto iconSprite = LoadInventoryItemSprite(directory, itemId))
                                    spriteData = std::move(*iconSprite);
                            }
                            const float spriteWidth = spriteData["source"]["width"].get<float>();
                            const float spriteHeight = spriteData["source"]["height"].get<float>();
                            current->AddObject({
                                {"name", "Inventory item"},
                                {"components", {
                                    {"TransformComp", {
                                        {"position", {
                                            {"x", (RenderSettings::AuthoredWidth - spriteWidth) / 2.0f},
                                            {"y", (RenderSettings::AuthoredHeight - spriteHeight) / 2.0f}
                                        }},
                                        {"scale", {{"x", 1.0}, {"y", 1.0}}}
                                    }},
                                    {"Sprite", std::move(spriteData)},
                                    {"InventoryItemComp", {{"item", itemId}}}
                                }}
                            });
                            selectCreatedObject("Inventory item");
                        }
                    };
                    const bool hasTarget = contextObject &&
                        *contextObject < current->ObjectCount();
                    if (hasTarget) {
                        auto object = current->Object(*contextObject);
                        const bool locked = object.value("locked", false);
                        if (ImGui::MenuItem("Delete", nullptr, false, !locked)) {
                            current->RemoveObject(*contextObject);
                            if (selectedObject == contextObject)
                                selectedObject.reset();
                            else if (selectedObject && *selectedObject > *contextObject)
                                --*selectedObject;
                            draggingObject.reset();
                            contextObject.reset();
                        }
                        if (ImGui::MenuItem("Duplicate")) {
                            auto duplicate = object;
                            duplicate.erase("parent");
                            if (duplicate.contains("name") && duplicate["name"].is_string()) {
                                const auto original = duplicate["name"].get<std::string>();
                                duplicate["name"] = original.substr(0, 122) + " copy";
                            }
                            current->AddObject(duplicate);
                            selectedObject = current->ObjectCount() - 1;
                        }
                        if (ImGui::MenuItem("Copy")) {
                            copiedObject = object;
                            copiedObject->erase("parent");
                            message = "Scene object copied.";
                            messageIsError = false;
                        }
                        if (ImGui::MenuItem(locked ? "Unlock" : "Lock")) {
                            object["locked"] = !locked;
                            current->SetObject(*contextObject, object);
                            draggingObject.reset();
                        }
                        ImGui::Separator();
                        if (ImGui::BeginMenu("Create")) {
                            drawCreateItems();
                            ImGui::EndMenu();
                        }
                        ImGui::Separator();
                    } else {
                        ImGui::TextDisabled("Create");
                        drawCreateItems();
                        ImGui::Separator();
                    }
                    if (ImGui::MenuItem("Paste", nullptr, false, copiedObject.has_value())) {
                        auto pasted = *copiedObject;
                        pasted.erase("parent");
                        pasted.erase("locked");
                        if (pasted.contains("name") && pasted["name"].is_string()) {
                            const auto original = pasted["name"].get<std::string>();
                            pasted["name"] = original.substr(0, 122) + " copy";
                        }
                        current->AddObject(pasted);
                        selectedObject = current->ObjectCount() - 1;
                    }
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();
    }

    if (sceneMode && showInteractionGraph)
        DrawInteractionGraph();
    if (!showInteractionGraph && nodeEditor)
        ShutdownNodeEditor();

    DrawVariableManager(showVariableManager, sceneMode);

    if (showSettings) {
        ImGui::SetNextWindowPos(ImVec2(1040, 360), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 260), ImGuiCond_FirstUseEver);
        ImGui::Begin("Project Settings");
        if (ImGui::BeginTabBar("ProjectSettingsTabs")) {
            if (ImGui::BeginTabItem("Project")) {
                ImGui::TextWrapped("Project-wide game render area. The scene workspace fills the editor window.");
                settingsDirty |= ImGui::DragInt("Render width", &renderWidth, 1.0f,
                                                MinRenderSize, MaxRenderSize);
                settingsDirty |= ImGui::DragInt("Render height", &renderHeight, 1.0f,
                                                MinRenderSize, MaxRenderSize);
                settingsDirty |= ImGui::Checkbox("Graphics filtering (Bilinear)", &textureFiltering);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Off = crisp pixel art (Point). On = smooth texture filtering (Bilinear).");
                ImGui::SeparatorText("Lens");
                settingsDirty |= ImGui::DragFloat("Lens offset X", &lensOffset.x, 0.5f, -512.0f, 512.0f, "%.1f");
                settingsDirty |= ImGui::DragFloat("Lens offset Y", &lensOffset.y, 0.5f, -256.0f, 256.0f, "%.1f");
                settingsDirty |= ImGui::Checkbox("Click-drag to move lens", &lensDragToMove);
                if (ImGui::SmallButton("Reset lens offset")) {
                    lensOffset = {0.0f, 0.0f};
                    settingsDirty = true;
                }
                if (ImGui::Button("Apply and save"))
                    SaveSettings();
                ImGui::Text("Game View: %d x %d", appliedRenderWidth, appliedRenderHeight);
                ImGui::Text("Lens Offset: (%.1f, %.1f) | %s",
                            appliedLensOffset.x, appliedLensOffset.y,
                            appliedLensDragToMove ? "Click-drag" : "Follow cursor");
                if (settingsDirty)
                    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Unsaved settings");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scene Settings")) {
                if (current) {
                    ImGui::TextWrapped("These settings are saved with the current scene.");
                    std::vector<std::filesystem::path> mainScenes;
                    std::vector<std::pair<std::string, std::string>> sceneReadErrors;
                    bool hasPopupChildren = false;
                    for (const auto& info : cachedSceneFiles) {
                        if (info.path == current->Path())
                            continue;
                        if (!info.error.empty()) {
                            sceneReadErrors.emplace_back(info.path.filename().string(), info.error);
                        } else if (info.type == SceneType::Main) {
                            mainScenes.push_back(info.path);
                        } else if (info.parentScene ==
                                   std::optional<std::string>(
                                       current->Path().filename().string())) {
                            hasPopupChildren = true;
                        }
                    }
                    int sceneType = current->Type() == SceneType::Popup ? 1 : 0;
                    const char* sceneTypeNames[] = {"Main", "Popup"};
                    if (current->Type() == SceneType::Main && hasPopupChildren)
                        ImGui::BeginDisabled();
                    if (ImGui::Combo("Scene type", &sceneType, sceneTypeNames, 2)) {
                        if (sceneType == 0) {
                            current->SetType(SceneType::Main);
                        } else {
                            std::string parent = current->ParentScene().value_or("");
                            const bool parentAvailable = std::any_of(
                                mainScenes.begin(), mainScenes.end(),
                                [&parent](const auto& file) {
                                    return file.filename().string() == parent;
                                });
                            if (!parentAvailable && !mainScenes.empty())
                                parent = mainScenes.front().filename().string();
                            if (parent.empty()) {
                                message = "Create a main scene before making this a popup.";
                                messageIsError = true;
                            } else {
                                current->SetType(SceneType::Popup, parent);
                            }
                        }
                    }
                    if (current->Type() == SceneType::Main && hasPopupChildren) {
                        ImGui::EndDisabled();
                        ImGui::TextDisabled(
                            "Reassign this scene's popup children before changing its type.");
                    }
                    if (current->Type() == SceneType::Popup) {
                        const auto selectedParent = current->ParentScene().value_or("");
                        const auto parentLabel = selectedParent.empty()
                            ? std::string("Choose parent scene")
                            : selectedParent;
                        if (ImGui::BeginCombo("Parent scene", parentLabel.c_str())) {
                            for (const auto& parent : mainScenes) {
                                const auto label = parent.filename().string();
                                if (ImGui::Selectable(label.c_str(), label == selectedParent))
                                    current->SetType(SceneType::Popup, label);
                            }
                            ImGui::EndCombo();
                        }
                        if (mainScenes.empty())
                            ImGui::TextDisabled("Create a main scene to choose as the popup parent.");
                    }
                    for (const auto& error : sceneReadErrors)
                        ImGui::TextWrapped("Cannot inspect %s: %s",
                                           error.first.c_str(), error.second.c_str());
                    float darkness = current->BaseDarkness();
                    if (ImGui::SliderFloat("Base darkness", &darkness, 0.0f, 1.0f,
                                           "%.2f"))
                        current->SetBaseDarkness(darkness, ImGui::IsAnyItemActive());
                    ImGui::TextDisabled("Higher values make the scene darker.");
                } else {
                    ImGui::TextWrapped("Open a scene to edit its settings.");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!message.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  messageIsError ? ImVec4(1, 0.4f, 0.4f, 1)
                                                 : ImVec4(0.5f, 1, 0.5f, 1));
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }

    if (showManager) {
        ImGui::SetNextWindowPos(ImVec2(640, 80), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(390, 350), ImGuiCond_FirstUseEver);
        ImGui::Begin("Scene Manager");
        if (current) {
            ImGui::TextWrapped("Current scene: %s", current->Path().filename().string().c_str());
            if (ImGui::Button("Add current scene to game")) {
                if (Save()) {
                    try {
                        auto& game = Game::get_Instance();
                        if (!game.scene_manager)
                            throw std::runtime_error("Game scene manager is not initialized.");
                        const auto path = current->Path();
                        game.scene_manager->RegisterScene(path);
                        game.LoadManagedScene(path);
                        message = "Added scene to game and loaded it for Play in Editor.";
                        messageIsError = false;
                    } catch (const std::exception& e) {
                        ReportError(e.what());
                    }
                }
            }
        } else {
            ImGui::TextUnformatted("Open or create a scene to add it to the game.");
        }

        ImGui::SeparatorText("Game scenes");
        auto& game = Game::get_Instance();
        if (!game.scene_manager) {
            ImGui::TextUnformatted("Game scene manager is unavailable.");
        } else {
            try {
                const auto scenes = game.scene_manager->RegisteredScenes();
                const auto startup = game.scene_manager->StartupScene();
                if (scenes.empty())
                    ImGui::TextUnformatted("No scenes added to the game.");
                for (std::size_t i = 0; i < scenes.size(); ++i) {
                    const auto& path = scenes[i];
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TextUnformatted(path.filename().string().c_str());
                    if (startup && *startup == path) {
                        ImGui::SameLine();
                        ImGui::TextUnformatted("(startup)");
                    }
                    if (ImGui::Button("Load for Play"))
                        game.LoadManagedScene(path);
                    ImGui::SameLine();
                    if (ImGui::Button("Set startup")) {
                        game.scene_manager->SetStartupScene(path);
                        message = "Startup scene set to " + path.filename().string() + ".";
                        messageIsError = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Remove")) {
                        game.scene_manager->UnregisterScene(path);
                        message = "Removed scene from the game catalog.";
                        messageIsError = false;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (startup) {
                    bool fadeInOnStartup = game.scene_manager->StartupFadeIn();
                    if (ImGui::Checkbox("Fade in at startup", &fadeInOnStartup)) {
                        try {
                            game.scene_manager->SetStartupFadeIn(fadeInOnStartup);
                            message = fadeInOnStartup
                                ? "Startup fade-in enabled."
                                : "Startup fade-in disabled.";
                            messageIsError = false;
                        } catch (const std::exception& e) {
                            ReportError(e.what());
                        }
                    }
                } else {
                    ImGui::TextDisabled("Set a startup scene to enable startup fade-in.");
                }
            } catch (const std::exception& e) {
                ImGui::TextWrapped("Cannot read game scenes: %s", e.what());
            }
        }
        if (!message.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  messageIsError ? ImVec4(1, 0.4f, 0.4f, 1)
                                                 : ImVec4(0.5f, 1, 0.5f, 1));
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }

    if (openConfirmation) {
        ImGui::OpenPopup("Unsaved scene");
        openConfirmation = false;
    }
    if (ImGui::BeginPopupModal("Unsaved scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save changes before continuing?");
        if (ImGui::Button("Save") && Save()) {
            ImGui::CloseCurrentPopup();
            Execute(exitRequested);
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            ImGui::CloseCurrentPopup();
            Execute(exitRequested);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending = Action::None;
            pendingPath.clear();
            ImGui::CloseCurrentPopup();
        }
        if (messageIsError)
            ImGui::TextWrapped("%s", message.c_str());
        ImGui::EndPopup();
    } else if (pending != Action::None &&
               (!current || !current->IsDirty()) &&
               !sceneVariablesDirty &&
               (pending != Action::Exit || !globalVariablesDirty)) {
        Execute(exitRequested);
    } else if (pending != Action::None && !ImGui::IsPopupOpen("Unsaved scene")) {
        openConfirmation = true;
    }

    if (current && !ImGui::IsAnyItemActive() && !draggingObject)
        current->EndUndoGroup();
}

void SceneEditor::DrawVariableManager(bool& showWindow, bool sceneMode)
{
    UpdateSceneVariableDraft();
    if (!ImGui::Begin("Variable Manager", &showWindow)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("VariableManagerTabs")) {
        if (ImGui::BeginTabItem("Global")) {
            ImGui::TextWrapped("Global variables are shared by all scenes and reset to their defaults when Play starts.");
            ImGui::TextDisabled("Names must be nonblank and unique within this scope.");
            if (DrawVariableDefinitions(globalVariables, newGlobalVariableName,
                                        "GlobalVariables", variableMessage,
                                        variableMessageIsError))
                globalVariablesDirty = true;
            if (!sceneMode)
                ImGui::TextDisabled("Definition changes apply the next time Play starts.");
            if (globalVariablesDirty)
                ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Unsaved global variables");
            if (ImGui::Button("Save global variables") && SaveGlobalVariables())
                globalVariablesDirty = false;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Scene")) {
            if (!current) {
                ImGui::TextWrapped("Open a scene to manage its scene variables.");
            } else {
                ImGui::TextWrapped("Scene variables belong to this scene and reset to their defaults when the scene loads.");
                ImGui::TextDisabled("Names must be nonblank and unique within this scope.");
                if (DrawVariableDefinitions(sceneVariableDraft, newSceneVariableName,
                                            "SceneVariables", variableMessage,
                                            variableMessageIsError)) {
                    sceneVariablesDirty = true;
                    if (CommitSceneVariables(ImGui::IsAnyItemActive()) &&
                        variableMessageIsError) {
                        variableMessage.clear();
                        variableMessageIsError = false;
                    }
                }
                if (!sceneMode)
                    ImGui::TextDisabled("Definition changes apply when this scene loads again.");
                if (sceneVariablesDirty)
                    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Unsaved scene variables");
                if (ImGui::Button("Save scene variables"))
                    Save();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (!variableMessage.empty()) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text,
                              variableMessageIsError ? ImVec4(1, 0.4f, 0.4f, 1)
                                                     : ImVec4(0.5f, 1, 0.5f, 1));
        ImGui::TextWrapped("%s", variableMessage.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

void SceneEditor::DrawInteractionGraph()
{
    ImGui::SetNextWindowSize(ImVec2(760, 600), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Interaction Node Editor", &showInteractionGraph);
    if (!visible) {
        ImGui::End();
        return;
    }

    if (!current || !selectedObject || *selectedObject >= current->ObjectCount()) {
        ShutdownNodeEditor();
        ImGui::TextUnformatted("Select an interaction in the Scenes panel.");
        ImGui::End();
        return;
    }

    auto object = current->Object(*selectedObject);
    try {
        LoadObject(object);
        if (!object["components"].contains("InteractionComp")) {
            ShutdownNodeEditor();
            ImGui::TextUnformatted("Select an interaction in the Scenes panel.");
            ImGui::End();
            return;
        }

        if (!nodeEditor || nodeScene != current->Path() || nodeObject != selectedObject) {
            ShutdownNodeEditor();
            const auto settingsDirectory = directory / ".devilwood" / "node_editor";
            std::filesystem::create_directories(settingsDirectory);
            nodeSettingsFile = (settingsDirectory /
                (current->Path().stem().string() + "_" +
                 std::to_string(*selectedObject) + ".json")).u8string();
            ed::Config config;
            config.SettingsFile = nodeSettingsFile.c_str();
            nodeEditor = ed::CreateEditor(&config);
            nodeScene = current->Path();
            nodeObject = selectedObject;
            initializeNodeView = true;
        }
        ed::SetCurrentEditor(nodeEditor);
        ed::EnableShortcuts(false);
        auto& actions = object["components"]["InteractionComp"]["actions"];
        bool changed = false;
        std::optional<std::size_t> selectAddedAction;
        std::optional<std::size_t> contextAddedAction;
        std::optional<std::size_t> contextDeletedAction;
        std::optional<std::string> contextAddType;

        ImGui::Text("Interaction: %s", object["name"].get<std::string>().c_str());
        ImGui::TextWrapped("Right-click empty graph space to add an action; right-click an action node to delete it. Change Scene ends the interaction.");
        auto& game = Game::get_Instance();
        if (!CanAppendAction(actions))
            ImGui::TextDisabled("Change Scene ends the action list.");
        if (ImGui::Button("Fit graph"))
            ed::NavigateToContent();

        ed::NodeId selectedNode;
        std::optional<std::size_t> selectedIndex;
        std::optional<bool> selectedBranch;
        bool selectIfElseTab = false;
        if (ed::GetSelectedNodes(&selectedNode, 1) == 1) {
            const auto selectedId = selectedNode.Get();
            selectIfElseTab = selectedNodeForDetails != selectedId;
            selectedNodeForDetails = selectedId;
            if (selectedId >= ActionNodeId(0) &&
                selectedId < ActionNodeId(actions.size()) &&
                (selectedId - ActionNodeId(0)) % 4 == 0) {
                const auto index = static_cast<std::size_t>(
                    (selectedId - ActionNodeId(0)) / 4);
                if (index < actions.size())
                    selectedIndex = index;
            } else {
                for (std::size_t i = 0; i < actions.size(); ++i) {
                    if (actions[i]["type"] != "IfElse")
                        continue;
                    if (selectedId == BranchNodeId(i, true)) {
                        selectedIndex = i;
                        selectedBranch = true;
                        break;
                    }
                    if (selectedId == BranchNodeId(i, false)) {
                        selectedIndex = i;
                        selectedBranch = false;
                        break;
                    }
                }
            }
        } else {
            selectedNodeForDetails.reset();
        }
        const bool actionSelected = selectedIndex.has_value();
        std::vector<std::filesystem::path> scenes;
        const std::size_t selectedActionIndex = selectedIndex.value_or(0);
        if (actionSelected && actions[selectedActionIndex]["type"] == "ChangeScene" &&
            game.scene_manager)
            scenes = game.scene_manager->MainScenes();
        ImGui::BeginChild("Action details", ImVec2(0, 230), ImGuiChildFlags_Borders);
        if (actionSelected) {
            if (actions[selectedActionIndex]["type"] == "ChangeScene" && scenes.empty())
                ImGui::TextWrapped("Register a main scene in Scene Manager first.");
            changed |= DrawUIAction(actions[selectedActionIndex], uiEditor, &*current,
                                    &globalVariables, &sceneVariableDraft,
                                    selectedBranch
                                        ? (*selectedBranch ? 0 : 1)
                                        : -1,
                                    selectIfElseTab);
            if (ImGui::Button("Remove selected action")) {
                actions.erase(selectedActionIndex);
                ed::ClearSelection();
                changed = true;
            }
        } else {
            ImGui::TextUnformatted("Select an action node to edit its target.");
        }
        ImGui::EndChild();

        ed::Begin("Interaction graph");
        ed::BeginNode(ed::NodeId(1));
        ImGui::TextUnformatted("Start");
        ed::BeginPin(ed::PinId(2), ed::PinKind::Output);
        ImGui::TextUnformatted("Out  >");
        ed::EndPin();
        ed::EndNode();

        std::vector<std::pair<std::uintptr_t, ImVec2>> branchNodePositions;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const auto id = ActionNodeId(i);
            ed::BeginNode(ed::NodeId(id));
            ed::BeginPin(ed::PinId(id + 1), ed::PinKind::Input);
            ImGui::TextUnformatted(">  In");
            ed::EndPin();
            const auto type = actions[i]["type"].get<std::string>();
            ImGui::TextUnformatted(ActionLabel(type));
            std::string target;
            if (type == "ChangeScene") {
                target = actions[i].value("scene", std::string{});
            } else if (type == "OpenPopupScene") {
                target = actions[i].value("scene", std::string{});
            } else if (type == "ShowObject" || type == "HideObject") {
                if (actions[i].contains("object") && actions[i]["object"].is_number_integer() &&
                    actions[i]["object"].get<std::int64_t>() >= 0)
                    target = std::to_string(actions[i]["object"].get<std::size_t>());
            } else if (type == "DisableHotspot" || type == "ChangeHotspotSetting") {
                if (actions[i].contains("hotspot") && actions[i]["hotspot"].is_number_integer() &&
                    actions[i]["hotspot"].get<std::int64_t>() >= 0)
                    target = std::to_string(actions[i]["hotspot"].get<std::size_t>());
            } else if (type == "SetVariable") {
                target = actions[i].value("scope", std::string("global")) + ":" +
                         actions[i].value("id", std::string{});
            } else if (type == "IfElse") {
                const auto& condition = actions[i]["condition"];
                target = condition.value("scope", std::string("global")) + ":" +
                         condition.value("id", std::string{});
            } else if (type == "AddItem" || type == "RemoveItem") {
                target = std::to_string(actions[i].value("item", 0));
            } else if (IsManagedSoundAction(type)) {
                target = std::to_string(actions[i].value("sound", 0));
            } else if (type == "SaveGame" || type == "LoadGame" || type == "DeleteSave") {
                target = "Slot " + std::to_string(actions[i].value("slot", 1));
            } else if (type == "EnableLens" || type == "DisableLens" || type == "ToggleLens") {
                target = "Scene lens";
            } else if (type == "RunScript") {
                target = actions[i].value("script", std::string{});
            } else if (IsAudioAction(type)) {
                if (actions[i].contains("audio") && actions[i]["audio"].is_number_integer())
                    target = std::to_string(actions[i]["audio"].get<std::size_t>());
            } else if (type == "ShowSubtitle") {
                target = "text input";
            } else {
                target = actions[i].value("ui", std::string{});
            }
            std::string targetLabel = target.empty() ? "(choose target)" : target;
            if ((type == "SetVariable" || type == "IfElse") && !target.empty()) {
                const auto separator = target.find(':');
                const std::string scope = target.substr(0, separator);
                const std::string id = separator == std::string::npos
                    ? std::string{} : target.substr(separator + 1);
                const auto& definitions = scope == "scene" ? sceneVariableDraft : globalVariables;
                const auto variable = std::find_if(definitions.begin(), definitions.end(),
                    [&id](const auto& definition) { return definition.id == id; });
                if (variable != definitions.end())
                    targetLabel = (scope == "scene" ? "Scene." : "Global.") + variable->name;
                else
                    targetLabel = (scope == "scene" ? "Scene." : "Global.") + id;
            } else if ((IsAudioAction(type) || type == "ShowObject" || type == "HideObject" ||
                        type == "DisableHotspot" || type == "ChangeHotspotSetting") &&
                       !target.empty()) {
                const auto objectIndex = static_cast<std::size_t>(std::stoull(target));
                if (objectIndex < current->ObjectCount())
                    targetLabel = AudioObjectLabel(*current, objectIndex);
            } else if (IsManagedSoundAction(type) && !target.empty()) {
                const auto* sound = game.soundManager.Find(std::stoi(target));
                if (sound)
                    targetLabel = sound->name + " (" + std::to_string(sound->id) + ")";
            } else if (type != "ChangeScene" && type != "OpenPopupScene" &&
                       type != "ClosePopupScene" && !target.empty()) {
                const auto uiTargets = uiEditor.Targets();
                const auto match = std::find_if(uiTargets.begin(), uiTargets.end(),
                    [&target](const auto& entry) { return entry.first == target; });
                if (match != uiTargets.end())
                    targetLabel = match->second;
            }
            ImGui::TextUnformatted(targetLabel.c_str());
            if (type != "IfElse" && i + 1 < actions.size()) {
                ed::BeginPin(ed::PinId(id + 2), ed::PinKind::Output);
                ImGui::TextUnformatted("Out  >");
                ed::EndPin();
            }
            if (type == "IfElse") {
                ed::BeginPin(ed::PinId(IfElseBranchPinId(i, true)), ed::PinKind::Output);
                ImGui::TextUnformatted("True  >");
                ed::EndPin();
                ed::BeginPin(ed::PinId(IfElseBranchPinId(i, false)), ed::PinKind::Output);
                ImGui::TextUnformatted("False  >");
                ed::EndPin();
            }
            ed::EndNode();

            if (type == "IfElse") {
                std::string variableName = targetLabel;
                const auto prefix = variableName.find('.');
                if (prefix != std::string::npos)
                    variableName.erase(0, prefix + 1);
                if (variableName.empty() || variableName == "(choose target)")
                    variableName = "variable";

                const auto drawBranchNode = [&](bool trueBranch,
                                                const nlohmann::json& branchActions) {
                    const auto nodeId = BranchNodeId(i, trueBranch);
                    ed::BeginNode(ed::NodeId(nodeId));
                    ed::BeginPin(ed::PinId(BranchInputPinId(i, trueBranch)),
                                 ed::PinKind::Input);
                    ImGui::TextUnformatted(">  In");
                    ed::EndPin();
                    const std::string label = std::string(trueBranch ? "True " : "False ") +
                                              variableName + " branch";
                    ImGui::TextUnformatted(label.c_str());
                    ImGui::TextDisabled("%zu action(s)", branchActions.size());
                    ed::BeginPin(ed::PinId(BranchOutputPinId(i, trueBranch)),
                                 ed::PinKind::Output);
                    ImGui::TextUnformatted("Continue  >");
                    ed::EndPin();
                    ed::EndNode();
                    if (positionedBranchNodes.insert(nodeId).second) {
                        branchNodePositions.emplace_back(
                            nodeId, ImVec2(500.0f + i * 220.0f,
                                           trueBranch ? 180.0f : 320.0f));
                    }
                    ed::Link(ed::LinkId(IfElseBranchLinkId(i, trueBranch ? 0 : 1)),
                             ed::PinId(IfElseBranchPinId(i, trueBranch)),
                             ed::PinId(BranchInputPinId(i, trueBranch)));
                };
                drawBranchNode(true, actions[i]["then"]);
                drawBranchNode(false, actions[i]["else"]);
            }

            if (i == 0) {
                ed::Link(ed::LinkId(id + 3), ed::PinId(2), ed::PinId(id + 1));
            } else if (actions[i - 1]["type"] == "IfElse") {
                ed::Link(ed::LinkId(IfElseBranchLinkId(i - 1, 2)),
                         ed::PinId(BranchOutputPinId(i - 1, true)), ed::PinId(id + 1));
                ed::Link(ed::LinkId(IfElseBranchLinkId(i - 1, 3)),
                         ed::PinId(BranchOutputPinId(i - 1, false)), ed::PinId(id + 1));
            } else {
                ed::Link(ed::LinkId(id + 3),
                         ed::PinId(ActionNodeId(i - 1) + 2), ed::PinId(id + 1));
            }
        }

        for (const auto& branchNode : branchNodePositions)
            ed::SetNodePosition(ed::NodeId(branchNode.first), branchNode.second);

        ed::Suspend();
        ed::NodeId contextNodeId(contextActionNode);
        if (ed::ShowNodeContextMenu(&contextNodeId)) {
            contextActionNode = contextNodeId.Get();
            const auto mouse = ImGui::GetMousePos();
            actionContextPosition = {mouse.x, mouse.y};
            ImGui::OpenPopup("Action Node Context");
        } else if (ed::ShowBackgroundContextMenu()) {
            contextActionNode = 0;
            const auto mouse = ImGui::GetMousePos();
            actionContextPosition = {mouse.x, mouse.y};
            ImGui::OpenPopup("Action Graph Context");
        }
        ed::Resume();

        ed::Suspend();
        if (ImGui::BeginPopup("Action Node Context")) {
            const auto nodeId = contextActionNode;
            if (nodeId >= ActionNodeId(0) &&
                nodeId < ActionNodeId(actions.size()) &&
                (nodeId - ActionNodeId(0)) % 4 == 0) {
                const auto index = static_cast<std::size_t>(
                    (nodeId - ActionNodeId(0)) / 4);
                if (index < actions.size()) {
                    if (ImGui::MenuItem("Delete action"))
                        contextDeletedAction = index;
                }
            } else {
                bool branchNode = false;
                for (std::size_t i = 0; i < actions.size(); ++i)
                    if (actions[i]["type"] == "IfElse" &&
                        (nodeId == BranchNodeId(i, true) ||
                         nodeId == BranchNodeId(i, false))) {
                        branchNode = true;
                        break;
                    }
                ImGui::TextDisabled(branchNode
                    ? "Select this branch to edit it in Action details."
                    : "The start node cannot be deleted.");
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("Action Graph Context")) {
            if (!CanAppendAction(actions)) {
                ImGui::TextDisabled("Change Scene ends the action list.");
            } else {
                std::vector<std::string> categories;
                for (const auto& choice : ActionChoices) {
                    if (!IsActionChoiceAvailable(choice, true,
                                                 game.scene_manager != nullptr))
                        continue;
                    if (std::find(categories.begin(), categories.end(), choice.category) ==
                        categories.end())
                        categories.emplace_back(choice.category);
                }
                for (const auto& category : categories) {
                    if (!ImGui::BeginMenu(category.c_str()))
                        continue;
                    for (const auto& choice : ActionChoices) {
                        if (choice.category == category &&
                            IsActionChoiceAvailable(choice, true,
                                                    game.scene_manager != nullptr) &&
                            ImGui::MenuItem(choice.label))
                            contextAddType = choice.type;
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndPopup();
        }
        ed::Resume();
        ed::End();

        if (contextDeletedAction && *contextDeletedAction < actions.size()) {
            actions.erase(*contextDeletedAction);
            ed::ClearSelection();
            contextActionNode = 0;
            changed = true;
        }
        if (contextAddType && CanAppendAction(actions)) {
            actions.push_back(CreateInteractionAction(*contextAddType, uiEditor));
            contextAddedAction = actions.size() - 1;
            selectAddedAction = contextAddedAction;
            changed = true;
        }

        if (selectAddedAction) {
            ed::ClearSelection();
            ed::SelectNode(ed::NodeId(ActionNodeId(*selectAddedAction)));
        }
        if (initializeNodeView) {
            ed::SetNodePosition(ed::NodeId(1), ImVec2(40, 50));
            if (!actions.empty())
                ed::SelectNode(ed::NodeId(ActionNodeId(0)));
        }
        for (std::size_t i = positionedActionNodes; i < actions.size(); ++i) {
            const auto position = contextAddedAction && i == *contextAddedAction
                ? ed::ScreenToCanvas(ImVec2(actionContextPosition.x, actionContextPosition.y))
                : ImVec2(260.0f + i * 220.0f, 50);
            ed::SetNodePosition(ed::NodeId(ActionNodeId(i)), position);
        }
        positionedActionNodes = actions.size();
        if (initializeNodeView) {
            ed::NavigateToContent(0.0f);
            initializeNodeView = false;
        }
        ed::SetCurrentEditor(nullptr);

        if (changed) {
            LoadObject(object);
            current->SetObject(*selectedObject, object, ImGui::IsAnyItemActive());
        }
    } catch (const std::exception& e) {
        ed::SetCurrentEditor(nullptr);
        ImGui::TextWrapped("Cannot edit interaction: %s", e.what());
    }
    ImGui::End();
}
