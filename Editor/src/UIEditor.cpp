#include "UIEditor.h"

#include "Component.h"
#include "ProjectUI.h"
#include "ResourcesLoader.h"
#include "SceneObject.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <fstream>
#include <set>
#include <stdexcept>

namespace
{
Vector2 ParentLocalPoint(Entity& entity, Vector2 point)
{
    auto* parent = entity.parent;
    while (parent && !parent->getComponent<TransformComp>())
        parent = parent->parent;
    if (!parent)
        return point;
    Vector2 local{};
    if (!WorldToLocal(parent->getComponent<TransformComp>(), point, local))
        throw std::runtime_error("Cannot drag UI element under a singular parent transform.");
    return local;
}

bool ContainsUIElement(Entity& entity, const nlohmann::json& data, Vector2 point)
{
    const auto* transform = entity.getComponent<TransformComp>();
    if (!transform)
        return false;
    const auto matrix = transform->getWorldMatrix();
    const float determinant = matrix.m[0][0] * matrix.m[1][1] - matrix.m[0][1] * matrix.m[1][0];
    if (std::abs(determinant) < 1e-8f)
        return false;
    const float x = point.x - matrix.m[0][2], y = point.y - matrix.m[1][2];
    const Vector2 local{(matrix.m[1][1] * x - matrix.m[0][1] * y) / determinant,
                        (-matrix.m[1][0] * x + matrix.m[0][0] * y) / determinant};
    if (const auto* panel = entity.getComponent<Panel_Sprite>())
        return CheckCollisionPointRec(local, {0, 0, panel->targetRect.width, panel->targetRect.height});
    if (const auto* sprite = entity.getComponent<Sprite>())
        return CheckCollisionPointRec(local, {0, 0, sprite->src_rec.width, sprite->src_rec.height});
    if (const auto* text = entity.getComponent<TextComp>()) {
        const auto margin = data["components"]["TextComp"].value("margin", nlohmann::json::object());
        const auto size = text->Measure();
        const float angle = transform->getWorldRotation() * DEG2RAD;
        const auto world = transform->getWorldPosition();
        const float dx = point.x - world.x - margin.value("x", 0.0f);
        const float dy = point.y - world.y - margin.value("y", 0.0f);
        const auto scale = transform->getWorldScale();
        const Vector2 textPoint{(std::cos(angle) * dx + std::sin(angle) * dy) / scale.x,
                                (-std::sin(angle) * dx + std::cos(angle) * dy) / scale.y};
        return CheckCollisionPointRec(textPoint, {0, 0, size.x, size.y});
    }
    return false;
}
}

UIEditor::UIEditor(std::filesystem::path directory) : sceneDirectory(std::move(directory))
{
    try {
        const auto path = ProjectUI::Path(sceneDirectory);
        if (std::filesystem::exists(path))
            document = SceneDocument::Open(path);
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
}

UIEditor::~UIEditor()
{
    Shutdown();
}

void UIEditor::Shutdown()
{
    if (IsRenderTextureValid(preview))
        UnloadRenderTexture(preview);
    preview = {};
}

std::vector<std::pair<std::string, std::string>> UIEditor::Targets(bool groupsOnly) const
{
    std::vector<std::pair<std::string, std::string>> result;
    if (document) {
        for (std::size_t i = 0; i < document->ObjectCount(); ++i) {
            const auto object = document->Object(i);
            if (object.is_object() && (!groupsOnly || object.value("group", false)) &&
                object.contains("id") && object["id"].is_string() &&
                object.contains("name") && object["name"].is_string())
                result.emplace_back(object["id"].get<std::string>(), object["name"].get<std::string>());
        }
    }
    return result;
}

std::vector<std::pair<int, std::string>> UIEditor::InventoryTargets() const
{
    std::vector<std::pair<int, std::string>> result;
    const auto path = sceneDirectory / ".devilwood" / "inventory_items.json";
    if (!std::filesystem::exists(path))
        return result;
    try {
        std::ifstream input(path, std::ios::binary);
        const auto data = nlohmann::json::parse(input);
        if (!data.contains("items") || !data["items"].is_array())
            return result;
        for (const auto& item : data["items"]) {
            if (item.contains("id") && item["id"].is_number_integer() &&
                item.contains("name") && item["name"].is_string())
                result.emplace_back(item["id"].get<int>(), item["name"].get<std::string>());
        }
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Cannot list inventory action targets: %s", e.what());
    }
    return result;
}

bool UIEditor::Save()
{
    if (!document)
        return false;
    try {
        document->Save();
        message = "Project UI saved.";
        messageIsError = false;
        return true;
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
        return false;
    }
}

bool UIEditor::SaveForRun()
{
    if (!document)
        return !messageIsError;
    return !document->IsDirty() || Save();
}

void UIEditor::Create()
{
    try {
        auto next = SceneDocument::Create(sceneDirectory / ".devilwood", "project_ui");
        next.Save();
        document = std::move(next);
        message = "Project UI manager created.";
        messageIsError = false;
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
}

void UIEditor::AddElement(const char* type)
{
    std::set<std::string> ids;
    for (const auto& target : Targets())
        ids.insert(target.first);
    auto next = std::chrono::system_clock::now().time_since_epoch().count();
    while (ids.count("ui_" + std::to_string(next)))
        ++next;
    const auto id = "ui_" + std::to_string(next);
    const bool subtitle = std::string(type) == "Subtitle";
    ids.insert(id);
    const bool group = std::string(type) == "Group" || subtitle;
    std::string subtitleTextId;
    if (subtitle) {
        auto textId = std::chrono::system_clock::now().time_since_epoch().count();
        do {
            subtitleTextId = "ui_" + std::to_string(textId++);
        } while (ids.count(subtitleTextId));
        ids.insert(subtitleTextId);
    }
    nlohmann::json components = {
        {"TransformComp", {{"position", {{"x", group ? 0 : 10}, {"y", group ? 0 : 10}}},
                           {"scale", {{"x", 1.0}, {"y", 1.0}}}}}
    };
    if (std::string(type) == "Text")
        components["TextComp"] = {{"text", "Text"}, {"size", 24},
            {"color", {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
            {"margin", {{"x", 0.0}, {"y", 0.0}}}};
    else if (std::string(type) == "Inventory") {
        components["InventoryComp"] = {
            {"slot_count", 10}, {"layout", "horizontal"}, {"spacing", 4.0},
            {"icon_size", {{"width", 24.0}, {"height", 24.0}}},
            {"slot_atlas", "ui_page.png"},
            {"slot_source", {{"x", 0}, {"y", 0}, {"width", 32}, {"height", 32}}}
        };
    }
    else if (!group) {
        components["Sprite"] = {{"atlas", "ui_page.png"},
            {"source", {{"x", 0}, {"y", 0}, {"width", 32}, {"height", 32}}}};
        if (std::string(type) == "Panel")
            components["Panel_Sprite"] = {{"margin", 8.0}, {"width", 128.0}, {"height", 64.0}};
    }
    const auto parent = group ? std::optional<std::size_t>{} : selected;
    document->AddObject();
    selected = document->ObjectCount() - 1;
    nlohmann::json object = {{"id", id}, {"name", type}, {"visible", !subtitle},
                             {"enabled", true}, {"alpha", 1.0}, {"components", std::move(components)}};
    if (subtitle) {
        object["type"] = "subtitle";
        object["type_in_speed"] = 30.0f;
        object["subtitle_text"] = subtitleTextId;
    }
    if (group) object["group"] = true;
    if (parent) object["parent"] = *parent;
    document->SetObject(*selected, object);
    if (subtitle) {
        const auto groupIndex = *selected;
        nlohmann::json textObject = {
            {"id", subtitleTextId}, {"name", "Subtitle Text"}, {"parent", groupIndex},
            {"visible", true}, {"enabled", true}, {"alpha", 1.0},
            {"components", {
                {"TransformComp", {
                    {"position", {{"x", 10}, {"y", 10}}},
                    {"scale", {{"x", 1.0}, {"y", 1.0}}}
                }},
                {"TextComp", {
                    {"text", "Subtitle"}, {"size", 24},
                    {"color", {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}},
                    {"margin", {{"x", 0.0}, {"y", 0.0}}}
                }}
            }}
        };
        document->AddObject(textObject);
        selected = document->ObjectCount() - 1;
    }
}

void UIEditor::DrawPreview(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    if (!IsRenderTextureValid(preview) || preview.texture.width != width || preview.texture.height != height) {
        auto next = LoadRenderTexture(width, height);
        if (!IsRenderTextureValid(next)) {
            message = "Unable to create the project UI preview render target.";
            messageIsError = true;
            return;
        }
        SetTextureFilter(next.texture, TEXTURE_FILTER_POINT);
        if (IsRenderTextureValid(preview))
            UnloadRenderTexture(preview);
        preview = next;
    }
    const float scale = std::min(static_cast<float>(width) / RenderSettings::AuthoredWidth,
                                 static_cast<float>(height) / RenderSettings::AuthoredHeight);
    std::unique_ptr<ProjectUI> ui;
    try {
        ui = ProjectUI::FromDocument(*document);
    } catch (const std::exception& e) {
        message = std::string("Cannot preview project UI: ") + e.what();
        messageIsError = true;
        return;
    }
    auto subtitlePreview = selected;
    while (subtitlePreview) {
        const auto selectedData = document->Object(*subtitlePreview);
        if (selectedData.value("type", std::string{}) == "subtitle") {
            const auto id = selectedData.at("id").get<std::string>();
            if (!ui->IsVisible(id))
                ui->SetVisible(id, true);
            break;
        }
        subtitlePreview = document->Parent(*subtitlePreview);
    }
    BeginTextureMode(preview);
    ClearBackground(Color{30, 33, 40, 255});
    Camera2D camera{};
    camera.zoom = scale;
    BeginMode2D(camera);
    ui->Draw();
    if (selected) {
        const auto id = document->Object(*selected).at("id").get<std::string>();
        if (const auto* transform = ui->GetElement(id).getComponent<TransformComp>())
            DrawCircleV(transform->getWorldPosition(), 3.0f / scale, YELLOW);
    }
    EndMode2D();
    EndTextureMode();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float imageScale = std::min(avail.x / width, std::max(1.0f, avail.y) / height);
    if (imageScale <= 0)
        return;
    ImGui::InvisibleButton("UI canvas", ImVec2(width * imageScale, height * imageScale));
    const ImVec2 origin = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)preview.texture.id, origin,
                                         ImGui::GetItemRectMax(), ImVec2(0, 1), ImVec2(1, 0));
    const auto mouse = ImGui::GetIO().MousePos;
    const Vector2 point{(mouse.x - origin.x) / (imageScale * scale),
                        (mouse.y - origin.y) / (imageScale * scale)};
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        selected.reset();
        dragging.reset();
        for (auto it = ui->DrawOrder().rbegin(); it != ui->DrawOrder().rend(); ++it) {
            const auto data = document->Object(*it);
            try {
                const auto id = data.at("id").get<std::string>();
                auto& entity = ui->GetElement(id);
                if (!ui->IsEffectivelyVisible(id) || !ContainsUIElement(entity, data, point))
                    continue;
                selected = *it;
                dragging = *it;
                const auto* transform = entity.getComponent<TransformComp>();
                const auto local = ParentLocalPoint(entity, point);
                dragOffset = {local.x - transform->position.x, local.y - transform->position.y};
                break;
            } catch (const std::exception& e) {
                message = std::string("Cannot select UI element: ") + e.what();
                messageIsError = true;
            }
        }
    }
    if (dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto data = document->Object(*dragging);
        auto& position = data["components"]["TransformComp"]["position"];
        try {
            const auto local = ParentLocalPoint(ui->GetElement(data.at("id").get<std::string>()), point);
            position["x"] = local.x - dragOffset.x;
            position["y"] = local.y - dragOffset.y;
            document->SetObject(*dragging, data);
        } catch (const std::exception& e) {
            message = e.what();
            messageIsError = true;
            dragging.reset();
        }
    } else {
        dragging.reset();
    }
}

void UIEditor::Draw(bool& open, int width, int height)
{
    ImGui::SetNextWindowSize(ImVec2(940, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UI Manager", &open)) {
        ImGui::End();
        return;
    }
    if (!message.empty()) {
        ImGui::TextColored(messageIsError ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.5f, 1, 0.5f, 1),
                           "%s", message.c_str());
    }
    if (!document) {
        ImGui::TextWrapped("Create a project-wide UI manager shared by all scenes.");
        if (ImGui::Button("Create UI Manager"))
            Create();
    } else {
        ImGui::Text("Project UI%s", document->IsDirty() ? " *" : "");
        if (ImGui::Button("Save UI"))
            Save();
        ImGui::SameLine();
        if (ImGui::Button("Add Group")) AddElement("Group");
        ImGui::SameLine();
        if (ImGui::Button("Add Text")) AddElement("Text");
        ImGui::SameLine();
        if (ImGui::Button("Add Sprite")) AddElement("Sprite");
        ImGui::SameLine();
        if (ImGui::Button("Add Panel")) AddElement("Panel");
        ImGui::SameLine();
        if (ImGui::Button("Add Inventory")) AddElement("Inventory");
        ImGui::SameLine();
        if (ImGui::Button("Add Subtitle")) AddElement("Subtitle");
        ImGui::TextDisabled("Preview matches the game view; positions use 512 x 256 authored units.");

        ImGui::BeginChild("UI controls", ImVec2(280, 0), ImGuiChildFlags_Borders);
        std::function<void(std::size_t, int)> showTree = [&](std::size_t index, int depth) {
            const auto object = document->Object(index);
            const auto prefix = object.value("type", std::string{}) == "subtitle" ? "[Subtitle] " :
                object.value("group", false) ? "[Group] " : "";
            const auto label = prefix +
                object.value("name", std::string("Invalid element")) + "##" + std::to_string(index);
            if (depth > 0) ImGui::Indent(depth * 12.0f);
            if (ImGui::Selectable(label.c_str(), selected && *selected == index))
                selected = index;
            if (depth > 0) ImGui::Unindent(depth * 12.0f);
            for (std::size_t child = 0; child < document->ObjectCount(); ++child)
                if (document->Parent(child) && *document->Parent(child) == index)
                    showTree(child, depth + 1);
        };
        try {
            for (std::size_t i = 0; i < document->ObjectCount(); ++i)
                if (!document->Parent(i)) showTree(i, 0);
        } catch (const std::exception& e) {
            message = std::string("Cannot display UI hierarchy: ") + e.what();
            messageIsError = true;
        }
        if (selected) {
            auto object = document->Object(*selected);
            try {
                auto loaded = LoadObject(object);
                bool changed = false;
                std::optional<std::optional<std::size_t>> reparent;
                ImGui::Separator();
                ImGui::Text("ID: %s", object.at("id").get<std::string>().c_str());
                std::array<char, 128> name{};
                std::copy(loaded.entity->name.begin(), loaded.entity->name.end(), name.begin());
                if (ImGui::InputText("Name", name.data(), name.size()) &&
                    std::string(name.data()).find_first_not_of(" \t\r\n") != std::string::npos) {
                    object["name"] = name.data();
                    changed = true;
                }
                bool visible = object.value("visible", true), enabled = object.value("enabled", true);
                if (ImGui::Checkbox("Visible", &visible)) { object["visible"] = visible; changed = true; }
                if (ImGui::Checkbox("Enabled", &enabled)) { object["enabled"] = enabled; changed = true; }
                if (object.value("type", std::string{}) == "subtitle") {
                    const auto selectedTextId =
                        object.value("subtitle_text", std::string{});
                    std::vector<std::pair<std::string, std::string>> childTextObjects;
                    for (std::size_t child = 0; child < document->ObjectCount(); ++child) {
                        const auto childObject = document->Object(child);
                        if (document->Parent(child) != selected ||
                            !childObject.contains("components") ||
                            !childObject["components"].is_object() ||
                            !childObject["components"].contains("TextComp") ||
                            !childObject.contains("id") || !childObject["id"].is_string() ||
                            !childObject.contains("name") || !childObject["name"].is_string())
                            continue;
                        childTextObjects.emplace_back(
                            childObject["id"].get<std::string>(),
                            childObject["name"].get<std::string>());
                    }
                    auto selectedText = std::find_if(
                        childTextObjects.begin(), childTextObjects.end(),
                        [&selectedTextId](const auto& candidate) {
                            return candidate.first == selectedTextId;
                        });
                    const std::string selectedTextName =
                        selectedText == childTextObjects.end()
                            ? "Select a child text object"
                            : selectedText->second;
                    if (ImGui::BeginCombo("Subtitle text object",
                                          selectedTextName.c_str())) {
                        for (const auto& candidate : childTextObjects) {
                            const auto label = candidate.second + "##subtitle_" +
                                candidate.first;
                            if (ImGui::Selectable(
                                    label.c_str(), candidate.first == selectedTextId)) {
                                object["subtitle_text"] = candidate.first;
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (childTextObjects.empty())
                        ImGui::TextDisabled("Add a child UI Text object to this group.");
                    float typeInSpeed = object.value("type_in_speed", 30.0f);
                    if (ImGui::DragFloat("Type-in speed (characters/sec)", &typeInSpeed,
                                         0.5f, 0.1f, 1000.0f)) {
                        object["type_in_speed"] = std::clamp(typeInSpeed, 0.1f, 1000.0f);
                        changed = true;
                    }
                    ImGui::TextDisabled("Auto-hide delay is 0.15 seconds per character after typing.");
                }
                float alpha = object.value("alpha", 1.0f);
                if (ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f)) {
                    object["alpha"] = alpha;
                    changed = true;
                }
                if (!object.value("group", false)) {
                    const auto parent = document->Parent(*selected);
                    const auto label = parent ? document->Object(*parent).at("name").get<std::string>() : "None";
                    if (ImGui::BeginCombo("Parent", label.c_str())) {
                        if (ImGui::Selectable("None", !parent)) reparent.emplace();
                        for (std::size_t candidate = 0; candidate < document->ObjectCount(); ++candidate) {
                            auto ancestor = std::optional<std::size_t>(candidate);
                            while (ancestor && *ancestor != *selected)
                                ancestor = document->Parent(*ancestor);
                            if (ancestor) continue;
                            const auto option = document->Object(candidate).at("name").get<std::string>() +
                                "##parent" + std::to_string(candidate);
                            if (ImGui::Selectable(option.c_str(), parent && *parent == candidate)) {
                                reparent.emplace(candidate);
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::TextWrapped("Hidden elements still update and receive input unless disabled.");
                auto& components = object["components"];
                auto& transform = components["TransformComp"];
                float position[] = {transform["position"]["x"].get<float>(), transform["position"]["y"].get<float>()};
                if (ImGui::DragFloat2("Position", position, 0.1f)) {
                    transform["position"] = {{"x", position[0]}, {"y", position[1]}};
                    changed = true;
                }
                float scale[] = {transform["scale"]["x"].get<float>(), transform["scale"]["y"].get<float>()};
                if (ImGui::DragFloat2("Scale", scale, 0.01f, 0.01f, 100.0f)) {
                    transform["scale"] = {{"x", std::max(0.01f, scale[0])}, {"y", std::max(0.01f, scale[1])}};
                    changed = true;
                }
                float rotation = transform.value("rotation", 0.0f);
                if (ImGui::DragFloat("Rotation", &rotation, 0.1f)) {
                    transform["rotation"] = rotation;
                    changed = true;
                }
                if (components.contains("Sprite")) {
                    auto& sprite = components["Sprite"];
                    auto atlas = sprite["atlas"].get<std::string>();
                    if (ImGui::BeginCombo("Atlas", atlas.c_str())) {
                        const auto atlases = ResourcesLoader::AvailableTextures();
                        for (const auto& option : atlases)
                            if (ImGui::Selectable(option.c_str(), atlas == option)) {
                                sprite["atlas"] = option;
                                sprite.erase("frame");
                                changed = true;
                            }
                        if (atlases.empty())
                            ImGui::TextWrapped("Add PNG textures to the project resource directory.");
                        ImGui::EndCombo();
                    }
                    auto& source = sprite["source"];
                    std::string frameError;
                    const auto spriteFrames = SpriteAtlasFrames(atlas, &frameError);
                    const auto currentFrame = sprite.value("frame", std::string{});
                    if (!spriteFrames.empty()) {
                        std::string frameLabel = currentFrame.empty()
                            ? "Choose sprite frame" : currentFrame;
                        if (ImGui::BeginCombo("Sprite frame", frameLabel.c_str())) {
                            for (const auto& frame : spriteFrames) {
                                if (ImGui::Selectable(frame.name.c_str(),
                                                      frame.name == currentFrame)) {
                                    sprite["frame"] = frame.name;
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
                    int xy[] = {source["x"].get<int>(), source["y"].get<int>()};
                    int size[] = {source["width"].get<int>(), source["height"].get<int>()};
                    if (ImGui::DragInt2("Atlas XY", xy)) {
                        source["x"] = std::max(0, xy[0]); source["y"] = std::max(0, xy[1]);
                        sprite.erase("frame"); changed = true;
                    }
                    if (ImGui::DragInt2("Atlas size", size)) {
                        source["width"] = std::max(1, size[0]); source["height"] = std::max(1, size[1]);
                        sprite.erase("frame"); changed = true;
                    }
                    if (!SourceFits(*loaded.entity->getComponent<Sprite>()))
                        ImGui::TextWrapped("Atlas unavailable or rectangle outside atlas.");
                }
                if (components.contains("Panel_Sprite")) {
                    auto& panel = components["Panel_Sprite"];
                    float size[] = {panel["width"].get<float>(), panel["height"].get<float>()};
                    float margin = panel["margin"].get<float>();
                    if (ImGui::DragFloat2("Panel size", size)) {
                        panel["width"] = std::max(1.0f, size[0]); panel["height"] = std::max(1.0f, size[1]); changed = true;
                    }
                    if (ImGui::DragFloat("Margin", &margin, 0.1f)) {
                        panel["margin"] = std::max(0.0f, margin); changed = true;
                    }
                }
                if (components.contains("InventoryComp")) {
                    auto& inventory = components["InventoryComp"];
                    int slotCount = inventory.value("slot_count", 10);
                    if (ImGui::DragInt("Slots", &slotCount, 1, 1, 100)) {
                        inventory["slot_count"] = std::clamp(slotCount, 1, 100);
                        changed = true;
                    }
                    bool vertical = inventory.value("layout", "horizontal") == "vertical";
                    if (ImGui::Checkbox("Vertical layout", &vertical)) {
                        inventory["layout"] = vertical ? "vertical" : "horizontal";
                        changed = true;
                    }
                    float spacing = inventory.value("spacing", 4.0f);
                    if (ImGui::DragFloat("Slot spacing", &spacing, 0.1f, 0.0f, 4096.0f)) {
                        inventory["spacing"] = std::max(0.0f, spacing);
                        changed = true;
                    }
                    const auto& slotSource = inventory["slot_source"];
                    const float defaultIconSize = std::min(
                        slotSource.value("width", 32), slotSource.value("height", 32));
                    auto iconSize = inventory.value(
                        "icon_size", nlohmann::json{{"width", defaultIconSize},
                                                    {"height", defaultIconSize}});
                    float iconDimensions[] = {
                        iconSize.value("width", defaultIconSize),
                        iconSize.value("height", defaultIconSize)
                    };
                    if (ImGui::DragFloat2("Icon size", iconDimensions, 0.1f, 1.0f, 4096.0f)) {
                        inventory["icon_size"] = {
                            {"width", std::max(1.0f, iconDimensions[0])},
                            {"height", std::max(1.0f, iconDimensions[1])}
                        };
                        changed = true;
                    }
                    auto atlas = inventory.value("slot_atlas", std::string("ui_page.png"));
                    if (ImGui::BeginCombo("Slot atlas", atlas.c_str())) {
                        const auto atlases = ResourcesLoader::AvailableTextures();
                        for (const auto& option : atlases)
                            if (ImGui::Selectable(option.c_str(), atlas == option)) {
                                inventory["slot_atlas"] = option;
                                changed = true;
                            }
                        if (atlases.empty())
                            ImGui::TextWrapped("Add PNG textures to the project resource directory.");
                        ImGui::EndCombo();
                    }
                    auto& source = inventory["slot_source"];
                    int xy[] = {source["x"].get<int>(), source["y"].get<int>()};
                    int size[] = {source["width"].get<int>(), source["height"].get<int>()};
                    if (ImGui::DragInt2("Slot atlas XY", xy)) {
                        source["x"] = std::max(0, xy[0]);
                        source["y"] = std::max(0, xy[1]);
                        changed = true;
                    }
                    if (ImGui::DragInt2("Slot atlas size", size)) {
                        source["width"] = std::max(1, size[0]);
                        source["height"] = std::max(1, size[1]);
                        changed = true;
                    }
                }
                bool hasCollider = components.contains("ColliderComp");
                if (ImGui::Checkbox("Block scene clicks", &hasCollider)) {
                    if (hasCollider)
                        components["ColliderComp"] = {{"width", 32.0}, {"height", 32.0}};
                    else
                        components.erase("ColliderComp");
                    changed = true;
                }
                if (hasCollider && !components.contains("Sprite")) {
                    auto& collider = components["ColliderComp"];
                    float size[] = {collider["width"].get<float>(), collider["height"].get<float>()};
                    if (ImGui::DragFloat2("Click area", size)) {
                        collider["width"] = std::max(1.0f, size[0]);
                        collider["height"] = std::max(1.0f, size[1]);
                        changed = true;
                    }
                }
                bool interactive = components.contains("InteractionComp");
                if (ImGui::Checkbox("Interactive", &interactive)) {
                    if (interactive) {
                        components["InteractionComp"] = {{"actions", nlohmann::json::array()}};
                        if (!components.contains("ColliderComp"))
                            components["ColliderComp"] = {{"width", 32.0}, {"height", 32.0}};
                    } else {
                        components.erase("InteractionComp");
                    }
                    changed = true;
                }
                if (interactive) {
                    auto& actions = components["InteractionComp"]["actions"];
                    const auto actionHeader = "Actions (" + std::to_string(actions.size()) +
                                              ")##UIInteractionActions";
                    if (ImGui::CollapsingHeader(actionHeader.c_str(),
                                                ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (std::size_t i = 0; i < actions.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::BeginGroup();
                            ImGui::Dummy(ImVec2(0, 4));
                            ImGui::Indent(6);
                            ImGui::Text("Action %zu", i + 1);
                            changed |= DrawUIAction(actions[i], *this);
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
                        if (DrawAddInteractionAction(actions, *this, nullptr, false, true))
                            changed = true;
                        ImGui::TextWrapped("The click runs these actions in order on this UI element.");
                    }
                }
                if (components.contains("TextComp")) {
                    auto& text = components["TextComp"];
                    std::array<char, 1024> buffer{};
                    const auto value = text["text"].get<std::string>();
                    std::copy(value.begin(), value.end(), buffer.begin());
                    if (ImGui::InputText("Text", buffer.data(), buffer.size())) { text["text"] = buffer.data(); changed = true; }
                    int size = text["size"].get<int>();
                    if (ImGui::DragInt("Font size", &size)) { text["size"] = std::clamp(size, 1, 512); changed = true; }
                    const auto font = text.value("font", std::string{});
                    if (ImGui::BeginCombo("Font", font.empty() ? "Default" : font.c_str())) {
                        if (ImGui::Selectable("Default", font.empty())) { text.erase("font"); changed = true; }
                        for (const auto& option : ResourcesLoader::AvailableFonts())
                            if (ImGui::Selectable(option.c_str(), option == font)) {
                                std::string error;
                                if (ResourcesLoader::GetCustomFont(option, &error)) { text["font"] = option; changed = true; }
                                else { message = error; messageIsError = true; }
                            }
                        ImGui::EndCombo();
                    }
                    auto& color = text["color"];
                    float rgba[] = {color["r"].get<float>() / 255, color["g"].get<float>() / 255,
                                    color["b"].get<float>() / 255, color["a"].get<float>() / 255};
                    if (ImGui::ColorEdit4("Color", rgba)) {
                        color = {{"r", static_cast<int>(rgba[0] * 255 + 0.5f)}, {"g", static_cast<int>(rgba[1] * 255 + 0.5f)},
                                 {"b", static_cast<int>(rgba[2] * 255 + 0.5f)}, {"a", static_cast<int>(rgba[3] * 255 + 0.5f)}};
                        changed = true;
                    }
                }
                if (changed) {
                    LoadObject(object);
                    document->SetObject(*selected, object);
                }
                if (reparent)
                    ReparentPreservingWorld(*document, *selected, *reparent);
                if (ImGui::Button("Delete UI element")) {
                    document->RemoveObject(*selected);
                    selected.reset();
                    dragging.reset();
                    message = "Removed UI element; update any scene actions that referenced it.";
                    messageIsError = false;
                }
            } catch (const std::exception& e) {
                message = e.what();
                messageIsError = true;
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("UI preview", ImVec2(0, 0), ImGuiChildFlags_Borders);
        DrawPreview(width, height);
        ImGui::EndChild();
    }
    ImGui::End();
}
