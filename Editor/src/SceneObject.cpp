#include "SceneObject.h"
#include "SceneDocument.h"
#include "ResourcesLoader.h"
#include "VariableStore.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>

namespace
{
const nlohmann::json& Field(const nlohmann::json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key))
        throw std::runtime_error(std::string("Missing object field: ") + key);
    return object[key];
}

float Number(const nlohmann::json& object, const char* key)
{
    const auto& value = Field(object, key);
    if (!value.is_number())
        throw std::runtime_error(std::string("Expected a number for ") + key);
    const double number = value.get<double>();
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max())
        throw std::runtime_error(std::string("Number out of range: ") + key);
    return static_cast<float>(number);
}

int Integer(const nlohmann::json& object, const char* key)
{
    const auto& value = Field(object, key);
    if (!value.is_number_integer())
        throw std::runtime_error(std::string("Expected an integer for ") + key);
    if (value.is_number_unsigned()) {
        if (value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error(std::string("Integer out of range: ") + key);
    } else {
        const auto number = value.get<std::int64_t>();
        if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
            throw std::runtime_error(std::string("Integer out of range: ") + key);
    }
    return value.get<int>();
}

std::string String(const nlohmann::json& object, const char* key)
{
    const auto& value = Field(object, key);
    if (!value.is_string())
        throw std::runtime_error(std::string("Expected text for ") + key);
    return value.get<std::string>();
}

Color ColorValue(const nlohmann::json& value)
{
    if (!value.is_object())
        throw std::runtime_error("Component color must be an object.");
    auto channel = [&value](const char* key) {
        const int number = Integer(value, key);
        if (number < 0 || number > 255)
            throw std::runtime_error(std::string("Color channel out of range: ") + key);
        return static_cast<unsigned char>(number);
    };
    return {channel("r"), channel("g"), channel("b"), channel("a")};
}

bool IsSafeAudioAssetPath(const std::string& asset)
{
    const std::filesystem::path path(asset);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto& part : path)
        if (part == "." || part == "..")
            return false;
    return true;
}

bool IsPngAssetName(const std::string& name)
{
    const std::filesystem::path path(name);
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return path.filename() == path && extension == ".png";
}

bool IsSupportedAudioAsset(const std::string& asset)
{
    if (asset.empty() || !IsSafeAudioAssetPath(asset))
        return false;
    const std::filesystem::path path(asset);
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".wav" || extension == ".ogg" || extension == ".mp3" ||
           extension == ".flac" || extension == ".qoa" || extension == ".xm" ||
           extension == ".mod";
}

VariableScope ParseVariableScope(const nlohmann::json& object)
{
    const auto scope = String(object, "scope");
    if (scope == "global")
        return VariableScope::Global;
    if (scope == "scene")
        return VariableScope::Scene;
    throw std::runtime_error("Variable scope must be global or scene.");
}

InteractionComp::ActionData ParseInteractionAction(const nlohmann::json& action)
{
    using Type = InteractionComp::ActionData::Type;
    const auto type = String(action, "type");
    if (type == "SaveGame" || type == "LoadGame" || type == "DeleteSave") {
        const int slot = action.contains("slot") ? Integer(action, "slot") : 1;
        if (slot < 1 || slot > 99)
            throw std::runtime_error("Save slot must be between 1 and 99.");
        const auto op = type == "SaveGame" ? Type::SaveGame
                      : type == "LoadGame" ? Type::LoadGame : Type::DeleteSave;
        InteractionComp::ActionData parsed{op};
        parsed.itemId = slot;
        return parsed;
    }
    if (type == "EnableLens" || type == "DisableLens" || type == "ToggleLens") {
        const auto op = type == "EnableLens" ? Type::EnableLens
                      : type == "DisableLens" ? Type::DisableLens : Type::ToggleLens;
        return InteractionComp::ActionData{op};
    }
    if (type == "RunScript") {
        InteractionComp::ActionData parsed{Type::RunScript};
        parsed.target = action.contains("script") ? String(action, "script") : std::string{};
        if (action.contains("function"))
            parsed.text = String(action, "function");
        return parsed;
    }
    if (type == "ShowObject" || type == "HideObject") {
        InteractionComp::ActionData parsed{
            type == "ShowObject" ? Type::ShowObject : Type::HideObject
        };
        if (action.contains("object") && !action["object"].is_null()) {
            const int index = Integer(action, "object");
            if (index < 0)
                throw std::runtime_error("Object action index cannot be negative.");
            parsed.objectIndex = static_cast<std::size_t>(index);
        }
        return parsed;
    }
    if (type == "DisableHotspot") {
        InteractionComp::ActionData parsed{Type::DisableHotspot};
        if (action.contains("hotspot") && !action["hotspot"].is_null()) {
            const int index = Integer(action, "hotspot");
            if (index < 0)
                throw std::runtime_error("Hotspot action index cannot be negative.");
            parsed.objectIndex = static_cast<std::size_t>(index);
        }
        return parsed;
    }
    if (type == "ChangeHotspotSetting") {
        InteractionComp::ActionData parsed{Type::ChangeHotspotSetting};
        if (action.contains("hotspot") && !action["hotspot"].is_null()) {
            const int index = Integer(action, "hotspot");
            if (index < 0)
                throw std::runtime_error("Hotspot action index cannot be negative.");
            parsed.objectIndex = static_cast<std::size_t>(index);
        }
        if (action.contains("interaction") && !action["interaction"].is_null()) {
            const int index = Integer(action, "interaction");
            if (index < 0)
                throw std::runtime_error("Hotspot setting interaction index cannot be negative.");
            parsed.hotspotInteractionIndex = static_cast<std::size_t>(index);
        }
        if (action.contains("check_variable") && !action["check_variable"].is_boolean())
            throw std::runtime_error("Hotspot setting check_variable must be a boolean.");
        if (action.contains("require_item") && !action["require_item"].is_boolean())
            throw std::runtime_error("Hotspot setting require_item must be a boolean.");
        if (action.contains("consume_item") && !action["consume_item"].is_boolean())
            throw std::runtime_error("Hotspot setting consume_item must be a boolean.");
        if (action.contains("disable_on_success") && !action["disable_on_success"].is_boolean())
            throw std::runtime_error("Hotspot setting disable_on_success must be a boolean.");
        if (action.contains("fail_message") && !action["fail_message"].is_string())
            throw std::runtime_error("Hotspot setting fail_message must be text.");
        parsed.hotspotCheckVariable = action.value("check_variable", false);
        if (action.contains("variable") && !action["variable"].is_null()) {
            const auto& variable = action["variable"];
            if (!variable.is_object())
                throw std::runtime_error("Hotspot setting variable must be an object.");
            parsed.variableScope = ParseVariableScope(variable);
            parsed.variableId = String(variable, "id");
        }
        parsed.hotspotRequireItem = action.value("require_item", false);
        if (action.contains("item") && !action["item"].is_null())
            parsed.itemId = Integer(action, "item");
        parsed.hotspotConsumeItem = action.value("consume_item", false);
        parsed.hotspotDisableOnSuccess = action.value("disable_on_success", false);
        if (action.contains("fail_message"))
            parsed.text = action["fail_message"].get<std::string>();
        return parsed;
    }
    if (type == "PlayAudio" || type == "PauseAudio" || type == "StopAudio") {
        std::optional<std::size_t> audioObject;
        if (action.contains("audio") && !action["audio"].is_null()) {
            const int index = Integer(action, "audio");
            audioObject = static_cast<std::size_t>(index);
        }
        const auto operation = type == "PlayAudio" ? Type::PlayAudio :
            type == "PauseAudio" ? Type::PauseAudio : Type::StopAudio;
        return {operation, {}, 0.0f, 0, audioObject};
    }
    if (type == "SoundOn" || type == "SoundOff" ||
        type == "SoundFadeIn" || type == "SoundFadeOut") {
        const int soundId = Integer(action, "sound");
        const bool fade = type == "SoundFadeIn" || type == "SoundFadeOut";
        const float duration = fade ? Number(action, "duration") : 0.0f;
        if (duration < 0)
            throw std::runtime_error("Sound fade duration must be non-negative.");
        const auto operation = type == "SoundOn" ? Type::SoundOn :
            type == "SoundOff" ? Type::SoundOff :
            type == "SoundFadeIn" ? Type::SoundFadeIn : Type::SoundFadeOut;
        return {operation, {}, duration, 0, std::nullopt, nullptr, soundId};
    }
    if (type == "AddItem" || type == "RemoveItem")
        return {type == "AddItem" ? Type::AddItem : Type::RemoveItem,
                {}, 0.0f, Integer(action, "item")};
    if (type == "ShowUI" || type == "HideUI" || type == "ToggleUI" ||
        type == "EnableUI" || type == "DisableUI" || type == "FadeInUI" ||
        type == "FadeOutUI" || type == "ToggleFadeUI") {
        const bool fade = type == "FadeInUI" || type == "FadeOutUI" ||
                          type == "ToggleFadeUI";
        const float duration = fade ? Number(action, "duration") : 0.0f;
        if (duration < 0)
            throw std::runtime_error("UI fade duration must be non-negative.");
        const auto operation = type == "ShowUI" ? Type::ShowUI :
            type == "HideUI" ? Type::HideUI :
            type == "ToggleUI" ? Type::ToggleUI :
            type == "EnableUI" ? Type::EnableUI :
            type == "DisableUI" ? Type::DisableUI :
            type == "FadeInUI" ? Type::FadeInUI :
            type == "FadeOutUI" ? Type::FadeOutUI : Type::ToggleFadeUI;
        return {operation, String(action, "ui"), duration};
    }
    if (type == "SetVariable") {
        InteractionComp::ActionData parsed{Type::SetVariable};
        parsed.variableScope = ParseVariableScope(action);
        parsed.variableId = String(action, "id");
        parsed.variableValue = VariableStore::ParseValue(Field(action, "value"));
        return parsed;
    }
    if (type == "ShowSubtitle") {
        InteractionComp::ActionData parsed{Type::ShowSubtitle};
        parsed.text = String(action, "text");
        return parsed;
    }
    if (type == "IfElse") {
        const auto& condition = Field(action, "condition");
        InteractionComp::ActionData parsed{Type::IfElse};
        parsed.variableScope = ParseVariableScope(condition);
        parsed.variableId = String(condition, "id");
        parsed.comparison = String(condition, "operator");
        parsed.variableValue = VariableStore::ParseValue(Field(condition, "value"));
        VariableStore::Compare(parsed.variableValue, parsed.comparison, parsed.variableValue);
        const auto& thenActions = Field(action, "then");
        const auto& elseActions = Field(action, "else");
        if (!thenActions.is_array() || !elseActions.is_array())
            throw std::runtime_error("If/Else branches must be action arrays.");
        for (const auto& nested : thenActions)
            parsed.thenActions.push_back(ParseInteractionAction(nested));
        for (const auto& nested : elseActions)
            parsed.elseActions.push_back(ParseInteractionAction(nested));
        return parsed;
    }
    if (type == "ClosePopupScene") {
        const float duration = action.contains("duration")
            ? Number(action, "duration") : 0.5f;
        if (duration < 0)
            throw std::runtime_error("Popup transition duration must be non-negative.");
        InteractionComp::ActionData parsed{Type::ClosePopupScene};
        parsed.duration = duration;
        return parsed;
    }
    if (type != "ChangeScene" && type != "OpenPopupScene")
        throw std::runtime_error("Unsupported interaction action.");
    const auto scene = String(action, "scene");
    const std::filesystem::path path(scene);
    if (!scene.empty() && (path.filename() != path || path.extension() != ".json" ||
                           scene.find_first_of("/\\:") != std::string::npos))
        throw std::runtime_error("Action scene must be a JSON scene filename.");
    InteractionComp::ActionData parsed{
        type == "ChangeScene" ? Type::ChangeScene : Type::OpenPopupScene, scene
    };
    if (type == "OpenPopupScene") {
        parsed.duration = action.contains("duration")
            ? Number(action, "duration") : 0.5f;
        if (parsed.duration < 0)
            throw std::runtime_error("Popup transition duration must be non-negative.");
    }
    return parsed;
}
}

const Texture2D* AtlasTexture(const std::string& name)
{
    return ResourcesLoader::GetTexture(name);
}

std::vector<SpriteAtlasFrame> SpriteAtlasFrames(const std::string& atlas, std::string* error)
{
    std::vector<SpriteAtlasFrame> frames;
    if (!IsPngAssetName(atlas)) {
        if (error) *error = "Sprite atlas must be a PNG filename.";
        return frames;
    }
        const auto metadataPath = AssetPaths::Resource(
        std::filesystem::path(atlas).replace_extension(".json"));

    if (!std::filesystem::exists(metadataPath))
        return frames;
    try {
        std::ifstream input(metadataPath, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open " + metadataPath.string());
        const auto metadata = nlohmann::json::parse(input);
        if (!metadata.contains("frames") || !metadata["frames"].is_object())
            throw std::runtime_error("Atlas metadata has no frames object: " + metadataPath.string());
        for (auto entry = metadata["frames"].begin(); entry != metadata["frames"].end(); ++entry) {
            if (!entry.value().contains("frame") || !entry.value()["frame"].is_object())
                throw std::runtime_error("Atlas frame is invalid: " + entry.key());
            const auto& frame = entry.value()["frame"];
            const int x = Integer(frame, "x"), y = Integer(frame, "y");
            const int width = Integer(frame, "w"), height = Integer(frame, "h");
            if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
                entry.value().value("rotated", false))
                throw std::runtime_error("Atlas frame is invalid or rotated: " + entry.key());
            frames.push_back({
                entry.key(),
                {
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(width), static_cast<float>(height)
                }
            });
        }
        if (error) error->clear();
    } catch (const std::exception& e) {
        frames.clear();
        if (error) *error = e.what();
    }
    return frames;
}

EditorObject LoadObject(const nlohmann::json& object)
{
    const auto name = String(object, "name");
    if (name.empty() || name.size() > 127 || name.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("Object name must contain 1-127 non-blank characters.");
    const auto& components = Field(object, "components");
    if (!components.is_object())
        throw std::runtime_error("Object components must be an object.");
    EditorObject result{std::make_unique<Entity>(name), {}};
    if (object.contains("ui")) {
        if (!object["ui"].is_boolean())
            throw std::runtime_error("Object UI layer must be a boolean.");
        result.entity->uiLayer = object["ui"].get<bool>();
    }
    if ((object.contains("parent") || components.contains("LightComp")) &&
        !components.contains("TransformComp"))
        result.entity->addComponent<TransformComp>();
    if (components.contains("TransformComp")) {
        const auto& data = components["TransformComp"];
        const auto& position = Field(data, "position");
        const auto& scale = Field(data, "scale");
        const float x = Number(position, "x"), y = Number(position, "y");
        const float sx = Number(scale, "x"), sy = Number(scale, "y");
        if (sx <= 0 || sy <= 0)
            throw std::runtime_error("Transform scale must be positive.");
        auto* transform = result.entity->addComponent<TransformComp>();
        transform->position = {x, y};
        transform->scale = {sx, sy};
        if (data.contains("rotation"))
            transform->rotation = Number(data, "rotation");
    }
    if (components.contains("Panel_Sprite")) {
        const auto& data = components["Panel_Sprite"];
        const float margin = Number(data, "margin");
        const float width = Number(data, "width");
        const float height = Number(data, "height");
        if (margin < 0 || width <= 0 || height <= 0)
            throw std::runtime_error("Panel sprite margin must be non-negative and its size positive.");
        if (!components.contains("Sprite"))
            throw std::runtime_error("Panel sprite requires a Sprite component.");
        result.entity->addComponent<Panel_Sprite>(
            margin, Rectangle{0, 0, width, height});
    }
    if (components.contains("RectangleComp")) {
        const auto& data = components["RectangleComp"];
        const float width = Number(data, "width");
        const float height = Number(data, "height");
        const float opacity = data.contains("opacity") ? Number(data, "opacity") : 1.0f;
        if (width <= 0 || height <= 0)
            throw std::runtime_error("Rectangle size must be positive.");
        if (opacity < 0 || opacity > 1)
            throw std::runtime_error("Rectangle opacity must be between 0 and 1.");
        auto* rectangle = result.entity->addComponent<RectangleComp>();
        rectangle->width = width;
        rectangle->height = height;
        rectangle->opacity = opacity;
        rectangle->color = data.contains("color") ? ColorValue(data["color"]) : WHITE;
        if (data.contains("render_order"))
            result.entity->setDrawOrder(rectangle, Integer(data, "render_order"));
    }
    if (components.contains("Sprite")) {
        const auto& data = components["Sprite"];
        result.atlas = String(data, "atlas");
        if (result.atlas.empty() || !IsPngAssetName(result.atlas))
            throw std::runtime_error("Sprite atlas must be a PNG filename in the resource directory.");
        const auto& source = Field(data, "source");
        const int x = Integer(source, "x"), y = Integer(source, "y");
        const int width = Integer(source, "width"), height = Integer(source, "height");
        if (x < 0 || y < 0 || width <= 0 || height <= 0)
            throw std::runtime_error("Sprite source coordinates must be non-negative and its size positive.");
        const auto* texture = AtlasTexture(result.atlas);
        auto* sprite = result.entity->addComponent<Sprite>(
            texture ? *texture : Texture2D{},
            Rectangle{static_cast<float>(x), static_cast<float>(y),
                      static_cast<float>(width), static_cast<float>(height)});
        if (data.contains("offset"))
            sprite->offset = {Number(data["offset"], "x"), Number(data["offset"], "y")};
        if (data.contains("opacity")) {
            sprite->alpha = Number(data, "opacity");
            if (sprite->alpha < 0 || sprite->alpha > 1)
                throw std::runtime_error("Sprite opacity must be between 0 and 1.");
        }
        if (data.contains("lens_only")) {
            if (!data["lens_only"].is_boolean())
                throw std::runtime_error("Sprite lens_only must be a boolean.");
            sprite->lensOnly = data["lens_only"].get<bool>();
        }
        if (data.contains("render_order"))
            result.entity->setDrawOrder(sprite, Integer(data, "render_order"));
        const auto* transform = result.entity->getComponent<TransformComp>();
        if (transform && (!std::isfinite(sprite->src_rec.width * transform->scale.x) ||
                          !std::isfinite(sprite->src_rec.height * transform->scale.y)))
            throw std::runtime_error("Transform scale produces a non-finite sprite size.");
    }
    if (components.contains("ColliderComp")) {
        const auto& data = components["ColliderComp"];
        const float width = Number(data, "width");
        const float height = Number(data, "height");
        if (width <= 0 || height <= 0)
            throw std::runtime_error("Collider size must be positive.");
        Rectangle colliderRect{0, 0, width, height};
        result.entity->addComponent<ColliderComp>(colliderRect);
    }
    if (components.contains("CircleCol")) {
        const auto& data = components["CircleCol"];
        const float radius = Number(data, "radius");
        if (radius <= 0)
            throw std::runtime_error("Circle collider radius must be positive.");
        const float x = data.contains("x") ? Number(data, "x") : 0;
        const float y = data.contains("y") ? Number(data, "y") : 0;
        auto* circle = result.entity->addComponent<CircleCol>(x, y, radius);
        if (const auto* transform = result.entity->getComponent<TransformComp>()) {
            circle->c.x = transform->position.x;
            circle->c.y = transform->position.y;
        }
    }
    if (components.contains("TextComp")) {
        const auto& data = components["TextComp"];
        const auto text = String(data, "text");
        const int size = Integer(data, "size");
        if (text.size() > 1023 || size <= 0 || size > 512)
            throw std::runtime_error("Text must be at most 1023 characters and its size must be 1-512.");
        const auto color = data.contains("color") ? ColorValue(data["color"]) : WHITE;
        Vector2 margin{0, 0};
        if (data.contains("margin")) {
            margin.x = Number(data["margin"], "x");
            margin.y = Number(data["margin"], "y");
        }
        const auto font = data.contains("font") ? String(data, "font") : std::string{};
        if (!font.empty() && !ResourcesLoader::IsFontAssetName(font))
            throw std::runtime_error("Text font must be a .ttf or .otf filename in resource/fonts.");
        result.entity->addComponent<TextComp>(text.c_str(), size, margin, color, font);
    }
    if (components.contains("LightComp")) {
        const auto& data = components["LightComp"];
        const float intensity = Number(data, "intensity");
        const float spriteIntensity = data.contains("sprite_intensity")
            ? Number(data, "sprite_intensity") : intensity;
        const float radius = Number(data, "radius");
        if (intensity < 0 || intensity > 10 || spriteIntensity < 0 ||
            spriteIntensity > 10 || radius <= 0 || radius > 4096)
            throw std::runtime_error(
                "Light intensities must be 0-10 and radius must be positive and at most 4096.");
        const auto color = data.contains("color") ? ColorValue(data["color"]) : WHITE;
        auto* light = result.entity->addComponent<LightComp>();
        light->intensity = intensity;
        light->spriteIntensity = spriteIntensity;
        light->radius = radius;
        light->color = color;
        if (data.contains("use_sprite_shape")) {
            if (!data["use_sprite_shape"].is_boolean())
                throw std::runtime_error("Light use_sprite_shape must be a boolean.");
            light->useSpriteShape = data["use_sprite_shape"].get<bool>();
            if (light->useSpriteShape && !result.entity->getComponent<Sprite>())
                throw std::runtime_error("A sprite-shaped light requires a Sprite component.");
        }
        if (data.contains("also_use_point_light")) {
            if (!data["also_use_point_light"].is_boolean())
                throw std::runtime_error("Light also_use_point_light must be a boolean.");
            light->alsoUsePointLight = data["also_use_point_light"].get<bool>();
            if (light->alsoUsePointLight && !light->useSpriteShape)
                throw std::runtime_error(
                    "Light also_use_point_light requires use_sprite_shape.");
        }
    }
    if (components.contains("AudioComp")) {
        const auto& data = components["AudioComp"];
        const auto asset = String(data, "asset");
        if (!IsSafeAudioAssetPath(asset))
            throw std::runtime_error("Audio asset must be inside the resource directory.");
        const float volume = Number(data, "volume");
        if (volume < 0 || volume > 1)
            throw std::runtime_error("Audio volume must be between 0 and 1.");
        if (data.contains("play_on_start") && !data["play_on_start"].is_boolean())
            throw std::runtime_error("Audio play_on_start must be a boolean.");
        result.entity->addComponent<AudioComp>(
            asset, volume, data.value("play_on_start", false));
    }
    if (components.contains("InteractionComp")) {
        const auto& data = components["InteractionComp"];
        const auto& actions = Field(data, "actions");
        if (!actions.is_array())
            throw std::runtime_error("Interaction actions must be an array.");
        if (data.contains("run_on_scene_start") && !data["run_on_scene_start"].is_boolean())
            throw std::runtime_error("Interaction run_on_scene_start must be a boolean.");
        auto* interaction = result.entity->addComponent<InteractionComp>();
        interaction->runOnSceneStart = data.value("run_on_scene_start", false);
        for (const auto& action : actions)
            interaction->actions.push_back(ParseInteractionAction(action));
    }
    if (components.contains("CraftingComp")) {
        if (!components["CraftingComp"].is_object() || !components.contains("TransformComp"))
            throw std::runtime_error("Crafting UI requires a Transform and a crafting component object.");
        auto* crafting = result.entity->addComponent<CraftingUIComp>();
        result.entity->setDrawOrder(crafting, 1);
        if (auto* text = result.entity->getComponent<TextComp>())
            result.entity->setDrawOrder(text, 2);
    }
    if (components.contains("InventoryComp")) {
        const auto& data = components["InventoryComp"];
        const int slotCount = Integer(data, "slot_count");
        if (slotCount < 1 || slotCount > 100)
            throw std::runtime_error("Inventory UI slot count must be between 1 and 100.");
        const float spacing = Number(data, "spacing");
        if (spacing < 0 || spacing > 4096)
            throw std::runtime_error("Inventory UI spacing must be non-negative and at most 4096.");
        const auto layout = String(data, "layout");
        if (layout != "horizontal" && layout != "vertical")
            throw std::runtime_error("Inventory UI layout must be horizontal or vertical.");
        const auto atlas = String(data, "slot_atlas");
        const auto* texture = AtlasTexture(atlas);
        if (!texture)
            throw std::runtime_error("Inventory slot atlas is unavailable: " + atlas);
        const auto& source = Field(data, "slot_source");
        const int x = Integer(source, "x"), y = Integer(source, "y");
        const int width = Integer(source, "width"), height = Integer(source, "height");
        if (x < 0 || y < 0 || width <= 0 || height <= 0)
            throw std::runtime_error("Inventory slot source must have positive size.");
        Vector2 iconSize{static_cast<float>(std::min(width, height)),
                         static_cast<float>(std::min(width, height))};
        if (data.contains("icon_size")) {
            const auto& authoredSize = Field(data, "icon_size");
            iconSize = {Number(authoredSize, "width"), Number(authoredSize, "height")};
            if (iconSize.x <= 0 || iconSize.y <= 0 ||
                !std::isfinite(iconSize.x) || !std::isfinite(iconSize.y))
                throw std::runtime_error("Inventory icon size must be positive and finite.");
        }
        result.entity->addComponent<InventoryUIComp>(
            *texture, Rectangle{static_cast<float>(x), static_cast<float>(y),
                                static_cast<float>(width), static_cast<float>(height)},
            slotCount, layout == "horizontal" ? InventoryUIComp::Layout::Horizontal
                                              : InventoryUIComp::Layout::Vertical,
            spacing, iconSize);
    }
    if (components.contains("HotspotComp")) {
        const auto& data = components["HotspotComp"];
        if (!data.is_object() || !components.contains("TransformComp") ||
            !components.contains("ColliderComp"))
            throw std::runtime_error("A hotspot requires a Transform and box collider.");
        if (data.contains("interaction") && !data["interaction"].is_null() &&
            Integer(data, "interaction") < 0)
            throw std::runtime_error("Hotspot interaction index cannot be negative.");
        if (data.contains("check_variable") && !data["check_variable"].is_boolean())
            throw std::runtime_error("Hotspot check_variable must be a boolean.");
        if (data.contains("require_item") && !data["require_item"].is_boolean())
            throw std::runtime_error("Hotspot require_item must be a boolean.");
        if (data.contains("consume_item") && !data["consume_item"].is_boolean())
            throw std::runtime_error("Hotspot consume_item must be a boolean.");
        if (data.contains("disable_on_success") && !data["disable_on_success"].is_boolean())
            throw std::runtime_error("Hotspot disable_on_success must be a boolean.");
        if (data.contains("fail_message") && !data["fail_message"].is_string())
            throw std::runtime_error("Hotspot fail_message must be text.");
        auto* hotspot = result.entity->addComponent<HotspotComp>();
        hotspot->checkVariable = data.value("check_variable", false);
        if (data.contains("variable") && !data["variable"].is_null()) {
            const auto& variable = data["variable"];
            if (!variable.is_object())
                throw std::runtime_error("Hotspot variable must be an object.");
            hotspot->variableScope = ParseVariableScope(variable);
            hotspot->variableId = String(variable, "id");
        }
        hotspot->requireItem = data.value("require_item", false);
        if (data.contains("item") && !data["item"].is_null())
            hotspot->requiredItemId = Integer(data, "item");
        hotspot->consumeItem = data.value("consume_item", false);
        hotspot->disableOnSuccess = data.value("disable_on_success", false);
        if (data.contains("fail_message"))
            hotspot->failMessage = data["fail_message"].get<std::string>();
        if (data.contains("item_interactions")) {
            if (!data["item_interactions"].is_array())
                throw std::runtime_error("Hotspot item_interactions must be an array.");
            for (const auto& entry : data["item_interactions"]) {
                if (!entry.is_object())
                    throw std::runtime_error("Hotspot item interaction entry must be an object.");
                if (entry.contains("interaction") && !entry["interaction"].is_null() &&
                    Integer(entry, "interaction") < 0)
                    throw std::runtime_error("Hotspot item interaction index cannot be negative.");
                if (entry.contains("consume_item") && !entry["consume_item"].is_boolean())
                    throw std::runtime_error("Hotspot item interaction consume_item must be a boolean.");
                if (entry.contains("disable_on_success") && !entry["disable_on_success"].is_boolean())
                    throw std::runtime_error("Hotspot item interaction disable_on_success must be a boolean.");
                HotspotComp::ItemInteraction parsed;
                parsed.itemId = Integer(entry, "item");
                parsed.consumeItem = entry.value("consume_item", false);
                parsed.disableOnSuccess = entry.value("disable_on_success", false);
                hotspot->itemInteractions.push_back(parsed);
            }
        }
    }
    if (components.contains("InventoryItemComp")) {
        const auto& data = components["InventoryItemComp"];
        if (!data.is_object() || !components.contains("Sprite"))
            throw std::runtime_error("An inventory item object requires a Sprite component.");
        result.entity->addComponent<InventoryItemComp>(Integer(data, "item"));
    }
    if (components.contains("ScriptComp")) {
        const auto& data = components["ScriptComp"];
        if (!data.is_object())
            throw std::runtime_error("ScriptComp must be an object.");
        const auto script = data.contains("script") ? String(data, "script") : std::string{};
        if (!script.empty() && !ScriptEngine::IsScriptAssetName(script))
            throw std::runtime_error("Script must be a .lua file inside the project scripts directory.");
        ScriptPropertyMap properties;
        std::map<std::string, std::optional<std::size_t>> objectRefIndices;
        if (data.contains("properties")) {
            if (!data["properties"].is_object())
                throw std::runtime_error("ScriptComp properties must be an object.");
            for (auto it = data["properties"].begin(); it != data["properties"].end(); ++it) {
                const auto& value = it.value();
                if (value.is_boolean()) {
                    properties.emplace(it.key(), value.get<bool>());
                } else if (value.is_number()) {
                    const double num = value.get<double>();
                    if (!std::isfinite(num))
                        throw std::runtime_error("Script property number must be finite: " + it.key());
                    properties.emplace(it.key(), num);
                } else if (value.is_string()) {
                    properties.emplace(it.key(), value.get<std::string>());
                } else if (value.is_object() && value.contains("object")) {
                    const auto& target = value["object"];
                    std::optional<std::size_t> index;
                    if (target.is_number_unsigned())
                        index = target.get<std::size_t>();
                    else if (target.is_number_integer() && target.get<std::int64_t>() >= 0)
                        index = static_cast<std::size_t>(target.get<std::int64_t>());
                    else if (!target.is_null())
                        throw std::runtime_error("Script object reference must be an object index: " + it.key());
                    objectRefIndices[it.key()] = index;
                } else {
                    throw std::runtime_error("Script property must be a boolean, number, string, or object reference: " + it.key());
                }
            }
        }
        auto* scriptComp = result.entity->addComponent<ScriptComp>(script, std::move(properties));
        scriptComp->objectRefIndices = std::move(objectRefIndices);
    }
    return result;
}

EditorSceneGraph LoadSceneGraph(
    const std::vector<nlohmann::json>& data, bool rejectInvalidObjects,
    const std::vector<VariableDefinition>* globalVariables,
    const std::vector<VariableDefinition>* sceneVariables)
{
    EditorSceneGraph graph;
    graph.objects.resize(data.size(), nullptr);
    std::vector<std::unique_ptr<Entity>> entities(data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        try {
            auto object = LoadObject(data[i]);
            entities[i] = std::move(object.entity);
            graph.objects[i] = entities[i].get();
        } catch (const std::runtime_error&) {
            if (rejectInvalidObjects)
                throw;
        }
    }

    for (std::size_t i = 0; i < data.size(); ++i) {
        if (!entities[i])
            continue;
        std::optional<std::size_t> parent;
        if (data[i].is_object() && data[i].contains("parent"))
            parent = data[i]["parent"].get<std::size_t>();
        if (parent) {
            if (*parent >= graph.objects.size() || !graph.objects[*parent]) {
                if (rejectInvalidObjects)
                    throw std::runtime_error("Scene parent refers to an invalid object.");
                graph.roots.push_back(std::move(entities[i]));
                continue;
            }
            graph.objects[*parent]->addChild(std::move(entities[i]));
        } else {
            graph.roots.push_back(std::move(entities[i]));
        }
    }

    for (auto* object : graph.objects) {
        if (!object)
            continue;
        auto* script = object->getComponent<ScriptComp>();
        if (!script)
            continue;
        script->objectRefs.clear();
        for (const auto& [name, index] : script->objectRefIndices) {
            Entity* target = nullptr;
            if (index && *index < graph.objects.size())
                target = graph.objects[*index];
            script->objectRefs[name] = target;
        }
    }

    if (rejectInvalidObjects) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            auto* object = graph.objects[i];
            if (!object)
                continue;
            if (const auto* audio = object->getComponent<AudioComp>();
                audio && !IsSupportedAudioAsset(audio->Asset()))
                throw std::runtime_error("Audio asset must be a supported file inside the resource directory.");
            if (const auto* inventoryItem = object->getComponent<InventoryItemComp>();
                inventoryItem && inventoryItem->itemId == 0)
                throw std::runtime_error("Choose an inventory item for every Inventory Item object.");
            if (const auto* script = object->getComponent<ScriptComp>();
                script && !ScriptEngine::IsScriptAssetName(script->Script()))
                throw std::runtime_error("Choose a valid .lua script for every Script component.");
            if (auto* interaction = object->getComponent<InteractionComp>()) {
                std::function<void(std::vector<InteractionComp::ActionData>&)> validateActions;
                validateActions = [&](std::vector<InteractionComp::ActionData>& actions) {
                    for (auto& action : actions) {
                        if (action.type == InteractionComp::ActionData::Type::SaveGame ||
                            action.type == InteractionComp::ActionData::Type::LoadGame ||
                            action.type == InteractionComp::ActionData::Type::DeleteSave) {
                            if (action.itemId < 1 || action.itemId > 99)
                                throw std::runtime_error("Save slot must be between 1 and 99.");
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::EnableLens ||
                            action.type == InteractionComp::ActionData::Type::DisableLens ||
                            action.type == InteractionComp::ActionData::Type::ToggleLens) {
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::RunScript) {
                            if (!ScriptEngine::IsScriptAssetName(action.target))
                                throw std::runtime_error("Choose a valid .lua script for every Run Script action.");
                            action.object = object;
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::ShowObject ||
                            action.type == InteractionComp::ActionData::Type::HideObject) {
                            if (!action.objectIndex ||
                                *action.objectIndex >= graph.objects.size() ||
                                !(action.object = graph.objects[*action.objectIndex]))
                                throw std::runtime_error(
                                    "Object action refers to an invalid scene object.");
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::DisableHotspot ||
                            action.type == InteractionComp::ActionData::Type::ChangeHotspotSetting) {
                            if (!action.objectIndex ||
                                *action.objectIndex >= graph.objects.size() ||
                                !(action.object = graph.objects[*action.objectIndex]) ||
                                !action.object->getComponent<HotspotComp>())
                                throw std::runtime_error(
                                    "Hotspot action refers to an invalid hotspot object.");
                            if (action.type == InteractionComp::ActionData::Type::ChangeHotspotSetting) {
                                if (action.hotspotInteractionIndex) {
                                    if (*action.hotspotInteractionIndex >= graph.objects.size() ||
                                        !graph.objects[*action.hotspotInteractionIndex] ||
                                        !(action.hotspotInteraction =
                                              graph.objects[*action.hotspotInteractionIndex]
                                                  ->getComponent<InteractionComp>()))
                                        throw std::runtime_error(
                                            "Change Hotspot Setting refers to an invalid interaction object.");
                                }
                                if (action.hotspotCheckVariable) {
                                    const auto* definitions =
                                        action.variableScope == VariableScope::Global
                                            ? globalVariables : sceneVariables;
                                    if (!definitions)
                                        throw std::runtime_error(
                                            "Change Hotspot Setting variable validation requires definitions.");
                                    const auto definition = std::find_if(
                                        definitions->begin(), definitions->end(),
                                        [&action](const auto& candidate) {
                                            return candidate.id == action.variableId;
                                        });
                                    if (definition == definitions->end())
                                        throw std::runtime_error(
                                            "Change Hotspot Setting refers to an undefined variable: " +
                                            action.variableId);
                                }
                                if (action.hotspotRequireItem && action.itemId == 0)
                                    throw std::runtime_error(
                                        "Choose an inventory item for Change Hotspot Setting.");
                            }
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::PlayAudio ||
                            action.type == InteractionComp::ActionData::Type::PauseAudio ||
                            action.type == InteractionComp::ActionData::Type::StopAudio) {
                            if (!action.audioObject ||
                                *action.audioObject >= graph.objects.size() ||
                                !graph.objects[*action.audioObject] ||
                                !(action.audio = graph.objects[*action.audioObject]
                                      ->getComponent<AudioComp>()))
                                throw std::runtime_error(
                                    "Audio action refers to an invalid audio object.");
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::SoundOn ||
                            action.type == InteractionComp::ActionData::Type::SoundOff ||
                            action.type == InteractionComp::ActionData::Type::SoundFadeIn ||
                            action.type == InteractionComp::ActionData::Type::SoundFadeOut) {
                            if (!Game::get_Instance().soundManager.HasSound(action.soundId))
                                throw std::runtime_error(
                                    "Sound action refers to an invalid sound manager entry.");
                            continue;
                        }
                        if (action.type == InteractionComp::ActionData::Type::SetVariable ||
                            action.type == InteractionComp::ActionData::Type::IfElse) {
                            const auto* definitions =
                                action.variableScope == VariableScope::Global
                                    ? globalVariables : sceneVariables;
                            if (!definitions)
                                throw std::runtime_error(
                                    "Variable validation requires definitions for the selected scope.");
                            const auto definition = std::find_if(
                                definitions->begin(), definitions->end(),
                                [&action](const auto& candidate) {
                                    return candidate.id == action.variableId;
                                });
                            if (definition == definitions->end())
                                throw std::runtime_error(
                                    "Interaction refers to an undefined variable: " +
                                    action.variableId);
                            if (definition->defaultValue.index() !=
                                action.variableValue.index())
                                throw std::runtime_error(
                                    "Interaction variable value does not match its definition: " +
                                    definition->name);
                            if (action.type == InteractionComp::ActionData::Type::IfElse) {
                                VariableStore::Compare(definition->defaultValue,
                                                       action.comparison,
                                                       action.variableValue);
                                validateActions(action.thenActions);
                                validateActions(action.elseActions);
                            }
                            continue;
                        }
                        if (action.type != InteractionComp::ActionData::Type::AddItem &&
                            action.type != InteractionComp::ActionData::Type::RemoveItem &&
                            action.type != InteractionComp::ActionData::Type::ClosePopupScene &&
                            action.type != InteractionComp::ActionData::Type::ShowSubtitle &&
                            action.target.empty())
                            throw std::runtime_error(
                                action.type == InteractionComp::ActionData::Type::ChangeScene
                                    ? "Choose a scene for every Change Scene action."
                                    : action.type == InteractionComp::ActionData::Type::OpenPopupScene
                                    ? "Choose a popup scene for every Open Popup Scene action."
                                    : "Choose a UI element for every UI action.");
                    }
                };
                validateActions(interaction->actions);
            }
            auto* hotspot = object->getComponent<HotspotComp>();
            if (!hotspot)
                continue;
            if (hotspot->checkVariable) {
                const auto* definitions = hotspot->variableScope == VariableScope::Global
                    ? globalVariables : sceneVariables;
                if (!definitions)
                    throw std::runtime_error(
                        "Hotspot variable validation requires definitions for the selected scope.");
                const auto definition = std::find_if(
                    definitions->begin(), definitions->end(),
                    [hotspot](const auto& candidate) {
                        return candidate.id == hotspot->variableId;
                    });
                if (definition == definitions->end())
                    throw std::runtime_error(
                        "Hotspot refers to an undefined variable: " + hotspot->variableId);
            }
            if (hotspot->requireItem && hotspot->requiredItemId == 0)
                throw std::runtime_error(
                    "Choose an inventory item for every item-checked hotspot.");
            const auto& hotspotData = data[i]["components"]["HotspotComp"];
            if (hotspotData.contains("item_interactions") &&
                hotspotData["item_interactions"].is_array()) {
                const auto& entries = hotspotData["item_interactions"];
                for (std::size_t e = 0; e < entries.size() && e < hotspot->itemInteractions.size(); ++e) {
                    if (hotspot->itemInteractions[e].itemId == 0)
                        throw std::runtime_error(
                            "Choose an inventory item for every hotspot item interaction.");
                    const auto& entry = entries[e];
                    if (!entry.contains("interaction") || entry["interaction"].is_null())
                        continue;
                    const auto targetIdx = static_cast<std::size_t>(Integer(entry, "interaction"));
                    if (targetIdx >= graph.objects.size() || !graph.objects[targetIdx] ||
                        !graph.objects[targetIdx]->getComponent<InteractionComp>())
                        throw std::runtime_error(
                            "Hotspot item interaction refers to an invalid interaction object.");
                    hotspot->itemInteractions[e].interaction =
                        graph.objects[targetIdx]->getComponent<InteractionComp>();
                }
            }
            if (!hotspotData.contains("interaction") || hotspotData["interaction"].is_null())
                continue;
            const auto index = static_cast<std::size_t>(Integer(hotspotData, "interaction"));
            if (index >= graph.objects.size() || !graph.objects[index] ||
                !graph.objects[index]->getComponent<InteractionComp>())
                throw std::runtime_error("Hotspot refers to an invalid interaction object.");
            hotspot->interaction = graph.objects[index]->getComponent<InteractionComp>();
        }
    }
    for (auto& root : graph.roots)
        root->Update(0);
    return graph;
}

bool SourceFits(const Sprite& sprite)
{
    return sprite.texture.id != 0 &&
        static_cast<double>(sprite.src_rec.x) + sprite.src_rec.width <= sprite.texture.width &&
        static_cast<double>(sprite.src_rec.y) + sprite.src_rec.height <= sprite.texture.height;
}

bool WorldToLocal(const TransformComp* parent, Vector2 world, Vector2& local)
{
    if (!parent) {
        local = world;
        return true;
    }
    const Matrix2D matrix = parent->getWorldMatrix();
    const float a = matrix.m[0][0], b = matrix.m[0][1], x = matrix.m[0][2];
    const float c = matrix.m[1][0], d = matrix.m[1][1], y = matrix.m[1][2];
    const float determinant = a * d - b * c;
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
        return false;
    const float worldX = world.x - x;
    const float worldY = world.y - y;
    local = {(d * worldX - b * worldY) / determinant,
             (-c * worldX + a * worldY) / determinant};
    return std::isfinite(local.x) && std::isfinite(local.y);
}

void ReparentPreservingWorld(SceneDocument& scene, std::size_t childIndex,
                             std::optional<std::size_t> parentIndex)
{
    if (scene.Parent(childIndex) == parentIndex)
        return;
    std::vector<nlohmann::json> data;
    data.reserve(scene.ObjectCount());
    for (std::size_t i = 0; i < scene.ObjectCount(); ++i)
        data.push_back(scene.Object(i));
    const auto graph = LoadSceneGraph(data);
    auto* child = graph.objects.at(childIndex);
    if (!child)
        throw std::runtime_error("Cannot reparent an invalid scene object.");
    const auto* childTransform = child->getComponent<TransformComp>();
    const Vector2 worldPosition = childTransform ? childTransform->getWorldPosition() : Vector2{0, 0};
    const Vector2 worldScale = childTransform ? childTransform->getWorldScale() : Vector2{1, 1};
    const float worldRotation = childTransform ? childTransform->getWorldRotation() : 0;
    const TransformComp* parentTransform = nullptr;
    if (parentIndex) {
        auto* parent = graph.objects.at(*parentIndex);
        if (!parent)
            throw std::runtime_error("Cannot parent to an invalid scene object.");
        parentTransform = parent->getComponent<TransformComp>();
    }
    Vector2 localPosition{};
    if (!WorldToLocal(parentTransform, worldPosition, localPosition))
        throw std::runtime_error("Cannot preserve position through a singular parent transform.");
    Vector2 localScale = worldScale;
    float localRotation = worldRotation;
    if (parentTransform) {
        const Vector2 parentScale = parentTransform->getWorldScale();
        if (parentScale.x <= 0 || parentScale.y <= 0 ||
            !std::isfinite(parentScale.x) || !std::isfinite(parentScale.y))
            throw std::runtime_error("Cannot preserve scale through an invalid parent transform.");
        localScale = {worldScale.x / parentScale.x, worldScale.y / parentScale.y};
        localRotation -= parentTransform->getWorldRotation();
    }
    if (localScale.x <= 0 || localScale.y <= 0 ||
        !std::isfinite(localScale.x) || !std::isfinite(localScale.y) ||
        !std::isfinite(localRotation))
        throw std::runtime_error("Reparenting produced an invalid local transform.");
    auto object = scene.Object(childIndex);
    if (parentIndex)
        object["parent"] = *parentIndex;
    else
        object.erase("parent");
    object["components"]["TransformComp"] = {
        {"position", {{"x", localPosition.x}, {"y", localPosition.y}}},
        {"scale", {{"x", localScale.x}, {"y", localScale.y}}},
        {"rotation", localRotation}
    };
    LoadObject(object);
    scene.SetObject(childIndex, object);
}
