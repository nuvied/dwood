#include "SceneObject.h"
#include "SceneDocument.h"
#include "ResourcesLoader.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
}

const Texture2D* AtlasTexture(const std::string& name)
{
    if (name == "atlas_1.png") return &ResourcesLoader::boat_tex_page;
    if (name == "inv_items.png") return &ResourcesLoader::inv_items;
    if (name == "light_page.png") return &ResourcesLoader::light_page;
    if (name == "ui_page.png") return &ResourcesLoader::ui_page;
    return nullptr;
}

std::vector<SpriteAtlasFrame> SpriteAtlasFrames(const std::string& atlas, std::string* error)
{
    std::vector<SpriteAtlasFrame> frames;
    const std::filesystem::path image(atlas);
    if (image.filename() != image || image.extension() != ".png") {
        if (error) *error = "Sprite atlas must be a PNG filename.";
        return frames;
    }
    const auto metadataPath = std::filesystem::path("resource") /
                              std::filesystem::path(atlas).replace_extension(".json");
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
    if (components.contains("Sprite")) {
        const auto& data = components["Sprite"];
        result.atlas = String(data, "atlas");
        const std::filesystem::path atlas(result.atlas);
        if (result.atlas.empty() || atlas.filename().string() != result.atlas || atlas.extension() != ".png")
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
        const float radius = Number(data, "radius");
        if (intensity < 0 || intensity > 10 || radius <= 0 || radius > 4096)
            throw std::runtime_error(
                "Light intensity must be 0-10 and radius must be positive and at most 4096.");
        const auto color = data.contains("color") ? ColorValue(data["color"]) : WHITE;
        auto* light = result.entity->addComponent<LightComp>();
        light->intensity = intensity;
        light->radius = radius;
        light->color = color;
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
        for (const auto& action : actions) {
            const auto type = String(action, "type");
            using Type = InteractionComp::ActionData::Type;
            if (type == "AddItem" || type == "RemoveItem") {
                interaction->actions.push_back(
                    {type == "AddItem" ? Type::AddItem : Type::RemoveItem,
                     {}, 0.0f, Integer(action, "item")});
                continue;
            }
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
                interaction->actions.push_back({operation, String(action, "ui"), duration});
                continue;
            }
            if (type != "ChangeScene")
                throw std::runtime_error("Unsupported interaction action.");
            const auto scene = String(action, "scene");
            const std::filesystem::path path(scene);
            if (!scene.empty() && (path.filename() != path || path.extension() != ".json" ||
                                   scene.find_first_of("/\\:") != std::string::npos))
                throw std::runtime_error("Action scene must be a JSON scene filename.");
            interaction->actions.push_back({InteractionComp::ActionData::Type::ChangeScene, scene});
        }
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
        result.entity->addComponent<HotspotComp>();
    }
    return result;
}

EditorSceneGraph LoadSceneGraph(const std::vector<nlohmann::json>& data, bool rejectInvalidObjects)
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

    if (rejectInvalidObjects) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            auto* object = graph.objects[i];
            if (!object)
                continue;
            if (auto* interaction = object->getComponent<InteractionComp>())
                for (const auto& action : interaction->actions)
                    if (action.type != InteractionComp::ActionData::Type::AddItem &&
                        action.type != InteractionComp::ActionData::Type::RemoveItem &&
                        action.target.empty())
                        throw std::runtime_error(
                            action.type == InteractionComp::ActionData::Type::ChangeScene
                                ? "Choose a scene for every Change Scene action."
                                : "Choose a UI element for every UI action.");
            auto* hotspot = object->getComponent<HotspotComp>();
            if (!hotspot)
                continue;
            const auto& hotspotData = data[i]["components"]["HotspotComp"];
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
