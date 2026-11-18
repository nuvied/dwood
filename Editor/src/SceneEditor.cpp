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
#include <stdexcept>
#include <utility>

namespace ed = ax::NodeEditor;

namespace
{
const char* const atlasNames[] = {"atlas_1.png", "inv_items.png", "light_page.png", "ui_page.png"};
constexpr int MinRenderSize = 1;
constexpr int MaxRenderSize = 8192;

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
}

bool DrawUIAction(nlohmann::json& action, const UIEditor& uiEditor)
{
    bool changed = false;
    auto type = action["type"].get<std::string>();
    const std::pair<const char*, const char*> operations[] = {
        {"ShowUI", "Show"}, {"HideUI", "Hide"}, {"ToggleUI", "Toggle"},
        {"EnableUI", "Enable"}, {"DisableUI", "Disable"},
        {"FadeInUI", "Fade in group"}, {"FadeOutUI", "Fade out group"},
        {"ToggleFadeUI", "Toggle group with fade"}, {"AddItem", "Add inventory item"},
        {"RemoveItem", "Remove inventory item"}
    };
    const auto selectedOperation = std::find_if(std::begin(operations), std::end(operations),
        [&type](const auto& operation) { return type == operation.first; });
    const char* operationLabel = selectedOperation == std::end(operations) ? "Unknown" : selectedOperation->second;
    if (ImGui::BeginCombo("Operation", operationLabel)) {
        for (const auto& operation : operations)
            if (ImGui::Selectable(operation.second, type == operation.first)) {
                type = operation.first;
                action["type"] = type;
                if (type == "FadeInUI" || type == "FadeOutUI" || type == "ToggleFadeUI")
                    action["duration"] = action.value("duration", 1.0f);
                if (type == "AddItem" || type == "RemoveItem") {
                    const auto items = uiEditor.InventoryTargets();
                    action["item"] = items.empty() ? 0 : items.front().first;
                }
                changed = true;
            }
        ImGui::EndCombo();
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

namespace
{
bool CanAppendAction(const nlohmann::json& actions)
{
    return actions.empty() || actions.back()["type"] != "ChangeScene";
}

std::uintptr_t ActionNodeId(std::size_t index) { return 4 + index * 4; }

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
    const auto& components = data["components"];

    if (const auto* panel = object.getComponent<Panel_Sprite>()) {
        if (CheckCollisionPointRec(point, {position.x, position.y,
                                           panel->targetRect.width * scale.x,
                                           panel->targetRect.height * scale.y}))
            return true;
    } else if (const auto* sprite = object.getComponent<Sprite>()) {
        if (CheckCollisionPointRec(point, {position.x, position.y,
                                           sprite->src_rec.width * scale.x,
                                           sprite->src_rec.height * scale.y}))
            return true;
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
        include({position.x, position.y, panel->targetRect.width * scale.x,
                 panel->targetRect.height * scale.y});
    else if (const auto* sprite = object.getComponent<Sprite>())
        include({position.x, position.y, sprite->src_rec.width * scale.x,
                 sprite->src_rec.height * scale.y});
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
    LoadSettings();
    Refresh();
}

SceneEditor::~SceneEditor()
{
    ShutdownNodeEditor();
}

void SceneEditor::ShutdownNodeEditor()
{
    if (nodeEditor) {
        ed::DestroyEditor(nodeEditor);
        nodeEditor = nullptr;
    }
    nodeScene.clear();
    nodeObject.reset();
    positionedActionNodes = 0;
    initializeNodeView = false;
}

const SceneDocument* SceneEditor::Current() const
{
    return current ? &*current : nullptr;
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
    } catch (const std::exception& e) {
        ReportError(e.what());
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
            {"render_height", renderHeight}
        };
        output << settings.dump(2) << '\n';
        output.flush();
        if (!output)
            throw std::runtime_error("Cannot finish writing game settings: " + path.string());

        Game::get_Instance().SetRenderResolution(renderWidth, renderHeight);
        appliedRenderWidth = renderWidth;
        appliedRenderHeight = renderHeight;
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
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
    }
}

void SceneEditor::Request(Action action, const std::filesystem::path& path)
{
    if (pending != Action::None)
        return;
    pending = action;
    pendingPath = path;
    if (current && current->IsDirty())
        openConfirmation = true;
}

bool SceneEditor::RequestExit()
{
    if (!current || !current->IsDirty())
        return true;
    Request(Action::Exit);
    return false;
}

bool SceneEditor::Save()
{
    if (!current)
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
    return !current || !current->IsDirty() || Save();
}

void SceneEditor::Execute(bool& exitRequested)
{
    try {
        if (pending == Action::Exit) {
            exitRequested = true;
        } else if (pending == Action::New || pending == Action::Open) {
            SceneDocument next = pending == Action::New
                ? SceneDocument::Create(directory, idInput.data())
                : SceneDocument::Open(pendingPath);
            nameInput.fill('\0');
            std::copy(next.Name().begin(), next.Name().end(), nameInput.begin());
            current = std::move(next);
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
    std::vector<nlohmann::json> data;
    EditorSceneGraph graph;
    if (current) {
        data.reserve(current->ObjectCount());
        for (std::size_t i = 0; i < current->ObjectCount(); ++i)
            data.push_back(current->Object(i));
        graph = LoadSceneGraph(data);
    }

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
        for (const auto& root : graph.roots)
            root->DrawLayer(false);
        EndMode2D();
        if (lightPreview && current && lighting)
            lighting->DrawLighting(graph.roots, camera, true);
        BeginMode2D(camera);
        for (const auto& root : graph.roots)
            root->DrawLayer(true);
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
                DrawCircleLinesV(position, light->radius, Fade(light->color, 0.65f));
                DrawCircleV(position, 5.0f / sceneZoom, light->color);
                DrawLineV({position.x - 8.0f / sceneZoom, position.y},
                          {position.x + 8.0f / sceneZoom, position.y}, light->color);
                DrawLineV({position.x, position.y - 8.0f / sceneZoom},
                          {position.x, position.y + 8.0f / sceneZoom}, light->color);
            }
            if (selectedObject && *selectedObject == i) {
                Rectangle bounds{};
                if (ObjectBounds(*object, data[i], bounds))
                    DrawRectangleLinesEx(bounds, 2, YELLOW);
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

    std::vector<nlohmann::json> data;
    data.reserve(current->ObjectCount());
    for (std::size_t i = 0; i < current->ObjectCount(); ++i)
        data.push_back(current->Object(i));
    EditorSceneGraph graph;
    try {
        graph = LoadSceneGraph(data);
    } catch (const std::exception& e) {
        message = e.what();
        messageIsError = true;
        return;
    }

    Camera2D camera{};
    camera.offset = sceneOffset;
    camera.target = {0, 0};
    camera.zoom = sceneZoom;
    const Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);

    if (!mouseCaptured && insideScene && !panningScene &&
        !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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
                    current->SetObject(*draggingObject, objectData);
                } catch (const std::exception& e) {
                    message = e.what();
                    messageIsError = true;
                }
            }
        }
    }
}

void SceneEditor::Draw(bool& exitRequested, bool sceneMode, bool showPanel, bool showSettings,
                       bool showManager)
{
    if (sceneMode)
        HandleSceneInput();

    if (showPanel) {
    ImGui::SetNextWindowPos(ImVec2(1040, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(230, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scenes");
    ImGui::TextWrapped("Directory: %s", directory.string().c_str());
    ImGui::InputText("Scene ID", idInput.data(), idInput.size());
    if (ImGui::Button("New scene"))
        Request(Action::New);
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        Refresh();

    ImGui::SeparatorText("Open scene");
    ImGui::BeginChild("SceneFiles", ImVec2(0, 160), ImGuiChildFlags_Borders);
    for (const auto& file : files) {
        const auto label = file.filename().string();
        if (ImGui::Selectable(label.c_str(), current && current->Path() == file))
            Request(Action::Open, file);
    }
    ImGui::EndChild();

    if (current) {
        ImGui::SeparatorText("Current scene");
        ImGui::TextWrapped("%s%s", current->Path().filename().string().c_str(),
                           current->IsDirty() ? " *" : "");
        if (ImGui::InputText("Name", nameInput.data(), nameInput.size()))
            current->SetName(nameInput.data());
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
            current->AddObject();
            const auto index = current->ObjectCount() - 1;
            const std::string lightName = "Light " + std::to_string(sceneLightCount + 1);
            current->SetObject(index, {
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
            selectedObject = index;
            objectNameInput.fill('\0');
            std::copy(lightName.begin(), lightName.end(), objectNameInput.begin());
        }
        ImGui::EndDisabled();
        if (ImGui::Button("Add UI")) {
            current->AddObject();
            const auto index = current->ObjectCount() - 1;
            current->SetObject(index, {
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
            selectedObject = index;
            objectNameInput.fill('\0');
            const std::string objectName = "UI text";
            std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
        }
        if (ImGui::Button("Add hotspot")) {
            current->AddObject();
            const auto index = current->ObjectCount() - 1;
            current->SetObject(index, {
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
            selectedObject = index;
            objectNameInput.fill('\0');
            const std::string objectName = "Hotspot";
            std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
        }
        ImGui::SameLine();
        if (ImGui::Button("Add interaction")) {
            current->AddObject();
            const auto index = current->ObjectCount() - 1;
            current->SetObject(index, {
                {"name", "Interaction"},
                {"components", {{"InteractionComp", {{"actions", nlohmann::json::array()}}}}}
            });
            selectedObject = index;
            objectNameInput.fill('\0');
            const std::string objectName = "Interaction";
            std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
        }
        if (sceneLightCount >= RenderSettings::MaxSceneLights)
            ImGui::TextDisabled("Maximum of %d scene lights.", RenderSettings::MaxSceneLights);
        ImGui::BeginChild("Objects", ImVec2(0, 110), ImGuiChildFlags_Borders);
        for (std::size_t i = 0; i < current->ObjectCount(); ++i) {
            std::string label = "Unsupported object";
            std::string objectName;
            try {
                auto object = LoadObject(current->Object(i));
                objectName = object.entity->name;
                label = objectName;
            } catch (const std::runtime_error&) {
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
                selectedObject = i;
                objectNameInput.fill('\0');
                std::copy(objectName.begin(), objectName.end(), objectNameInput.begin());
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
        ImGui::EndChild();
        if (openObjectContextMenu) {
            ImGui::OpenPopup("Scene object menu");
            openObjectContextMenu = false;
        }
        if (ImGui::BeginPopup("Scene object menu")) {
            const bool hasTarget = contextObject &&
                *contextObject < current->ObjectCount();
            if (hasTarget) {
                auto object = current->Object(*contextObject);
                const bool locked = object.value("locked", false);
                if (ImGui::MenuItem("Delete", nullptr, false, !locked)) {
                    current->RemoveObject(*contextObject);
                    if (selectedObject == contextObject)
                        selectedObject.reset();
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
                    current->AddObject();
                    current->SetObject(current->ObjectCount() - 1, duplicate);
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
            }
            if (ImGui::MenuItem("Paste", nullptr, false, copiedObject.has_value())) {
                auto pasted = *copiedObject;
                pasted.erase("parent");
                pasted.erase("locked");
                if (pasted.contains("name") && pasted["name"].is_string()) {
                    const auto original = pasted["name"].get<std::string>();
                    pasted["name"] = original.substr(0, 122) + " copy";
                }
                current->AddObject();
                current->SetObject(current->ObjectCount() - 1, pasted);
                selectedObject = current->ObjectCount() - 1;
            }
            ImGui::EndPopup();
        }
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
                    auto& target = components["HotspotComp"]["interaction"];
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
                    if (ImGui::BeginCombo("Interaction", label.c_str())) {
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
                    ImGui::TextWrapped("The rectangle is visible only in the editor.");
                }
                if (components.contains("InteractionComp")) {
                    ImGui::SeparatorText("Interaction actions");
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
                    for (std::size_t i = 0; i < actions.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        if (actions[i]["type"] == "ChangeScene") {
                            ImGui::TextUnformatted("Change Scene");
                            if (!game.scene_manager)
                                ImGui::TextWrapped("Scene manager is unavailable.");
                            else
                                changed |= DrawActionTarget(actions[i]["scene"], scenes);
                        } else {
                            ImGui::TextUnformatted("UI visibility");
                            changed |= DrawUIAction(actions[i], uiEditor);
                        }
                        if (ImGui::Button("Remove action")) {
                            actions.erase(i);
                            changed = true;
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::BeginDisabled(!game.scene_manager || !CanAppendAction(actions));
                    if (ImGui::Button("Add Change Scene")) {
                        actions.push_back({{"type", "ChangeScene"}, {"scene", ""}});
                        changed = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(!CanAppendAction(actions));
                    if (ImGui::Button("Add action")) {
                        if (uiEditor.HasManager())
                            actions.push_back({{"type", "ShowUI"}, {"ui", ""}});
                        else {
                            const auto items = uiEditor.InventoryTargets();
                            actions.push_back({{"type", "AddItem"},
                                               {"item", items.empty() ? 0 : items.front().first}});
                        }
                        changed = true;
                    }
                    ImGui::EndDisabled();
                    if (!uiEditor.HasManager() &&
                        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("Create a UI Manager first.");
                    ImGui::TextWrapped("Changing scene ends the current interaction.");
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
                        for (const char* atlas : atlasNames) {
                            if (ImGui::Selectable(atlas, edited.atlas == atlas)) {
                                edited.atlas = atlas;
                                const auto* texture = AtlasTexture(edited.atlas);
                                sprite->texture = *texture;
                                components["Sprite"]["atlas"] = edited.atlas;
                                components["Sprite"].erase("frame");
                                changed = true;
                            }
                        }
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
                    if (!SourceFits(*sprite))
                        ImGui::TextWrapped("Atlas unavailable or rectangle outside atlas.");
                    if (ImGui::Button("Remove Sprite")) {
                        components.erase("Sprite");
                        changed = true;
                    }
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
                    ImGui::TextWrapped("Radius is in scene units.");
                    float intensity = components["LightComp"]["intensity"].get<float>();
                    if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f,
                                         "%.2f")) {
                        intensity = std::clamp(intensity, 0.0f, 10.0f);
                        components["LightComp"]["intensity"] = intensity;
                        light->intensity = intensity;
                        changed = true;
                    }
                    float radius = components["LightComp"]["radius"].get<float>();
                    if (ImGui::DragFloat("Radius", &radius, 1.0f, 1.0f, 4096.0f,
                                         "%.1f")) {
                        radius = std::clamp(radius, 1.0f, 4096.0f);
                        components["LightComp"]["radius"] = radius;
                        light->radius = radius;
                        changed = true;
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
                for (auto it = components.begin(); it != components.end(); ++it) {
                    if (it.key() != "TransformComp" && it.key() != "Sprite" &&
                        it.key() != "Panel_Sprite" && it.key() != "ColliderComp" &&
                        it.key() != "CircleCol" && it.key() != "TextComp" &&
                        it.key() != "LightComp" && it.key() != "HotspotComp" &&
                        it.key() != "InteractionComp")
                        ImGui::TextWrapped("Unsupported component: %s (preserved)", it.key().c_str());
                }
                if (changed && !locked) {
                    try {
                        LoadObject(object);
                        current->SetObject(*selectedObject, object);
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

    if (sceneMode && showInteractionGraph)
        DrawInteractionGraph();
    if (!showInteractionGraph && nodeEditor)
        ShutdownNodeEditor();

    if (showSettings) {
        ImGui::SetNextWindowPos(ImVec2(1040, 360), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(250, 220), ImGuiCond_FirstUseEver);
        ImGui::Begin("Scene Settings");
        ImGui::TextWrapped("Project-wide game render area. The scene workspace fills the editor window.");
        settingsDirty |= ImGui::DragInt("Render width", &renderWidth, 1.0f,
                                        MinRenderSize, MaxRenderSize);
        settingsDirty |= ImGui::DragInt("Render height", &renderHeight, 1.0f,
                                        MinRenderSize, MaxRenderSize);
        if (ImGui::Button("Apply and save"))
            SaveSettings();
        ImGui::Text("Game View: %d x %d", appliedRenderWidth, appliedRenderHeight);
        if (settingsDirty)
            ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Unsaved settings");
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
        ImGui::TextUnformatted("Save changes to the current scene?");
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
    } else if (pending != Action::None && (!current || !current->IsDirty())) {
        Execute(exitRequested);
    } else if (pending != Action::None && !ImGui::IsPopupOpen("Unsaved scene")) {
        openConfirmation = true;
    }
}

void SceneEditor::DrawInteractionGraph()
{
    ImGui::SetNextWindowSize(ImVec2(680, 460), ImGuiCond_FirstUseEver);
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
            ed::Config config;
            config.SettingsFile = nullptr;
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

        ImGui::Text("Interaction: %s", object["name"].get<std::string>().c_str());
        ImGui::TextWrapped("Actions run in order; Change Scene ends the interaction. Links are automatic.");
        auto& game = Game::get_Instance();
        ImGui::BeginDisabled(!game.scene_manager || !CanAppendAction(actions));
        if (ImGui::Button("Add Change Scene")) {
            actions.push_back({{"type", "ChangeScene"}, {"scene", ""}});
            changed = true;
            selectAddedAction = actions.size() - 1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!CanAppendAction(actions));
        if (ImGui::Button("Add action")) {
            if (uiEditor.HasManager())
                actions.push_back({{"type", "ShowUI"}, {"ui", ""}});
            else {
                const auto items = uiEditor.InventoryTargets();
                actions.push_back({{"type", "AddItem"},
                                   {"item", items.empty() ? 0 : items.front().first}});
            }
            changed = true;
            selectAddedAction = actions.size() - 1;
        }
        ImGui::EndDisabled();
        if (!uiEditor.HasManager() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Create a UI Manager first.");
        ImGui::SameLine();
        if (ImGui::Button("Fit graph"))
            ed::NavigateToContent();

        ed::NodeId selectedNode;
        const bool actionSelected = ed::GetSelectedNodes(&selectedNode, 1) == 1 &&
            selectedNode.Get() >= ActionNodeId(0) &&
            (selectedNode.Get() - ActionNodeId(0)) % 4 == 0 &&
            (selectedNode.Get() - ActionNodeId(0)) / 4 < actions.size();
        std::vector<std::filesystem::path> scenes;
        const auto selectedIndex = actionSelected
            ? static_cast<std::size_t>((selectedNode.Get() - ActionNodeId(0)) / 4) : 0;
        if (actionSelected && actions[selectedIndex]["type"] == "ChangeScene" && game.scene_manager)
            scenes = game.scene_manager->RegisteredScenes();
        ImGui::BeginChild("Action details", ImVec2(0, 150));
        if (actionSelected) {
            if (actions[selectedIndex]["type"] == "ChangeScene") {
                if (!game.scene_manager)
                    ImGui::TextWrapped("Scene manager is unavailable.");
                else {
                    if (scenes.empty())
                        ImGui::TextWrapped("Register a scene in Scene Manager first.");
                    changed |= DrawActionTarget(actions[selectedIndex]["scene"], scenes);
                }
            } else
                changed |= DrawUIAction(actions[selectedIndex], uiEditor);
            if (ImGui::Button("Remove selected action")) {
                actions.erase(selectedIndex);
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

        for (std::size_t i = 0; i < actions.size(); ++i) {
            const auto id = ActionNodeId(i);
            ed::BeginNode(ed::NodeId(id));
            ed::BeginPin(ed::PinId(id + 1), ed::PinKind::Input);
            ImGui::TextUnformatted(">  In");
            ed::EndPin();
            const auto type = actions[i]["type"].get<std::string>();
            ImGui::TextUnformatted(type == "ChangeScene" ? "Change Scene"
                                   : type == "ShowUI" ? "Show UI"
                                   : type == "HideUI" ? "Hide UI"
                                   : type == "ToggleUI" ? "Toggle UI"
                                   : type == "EnableUI" ? "Enable UI"
                                   : type == "DisableUI" ? "Disable UI"
                                   : type == "FadeInUI" ? "Fade in group"
                                   : type == "FadeOutUI" ? "Fade out group"
                                   : type == "ToggleFadeUI" ? "Toggle group with fade"
                                   : type == "AddItem" ? "Add inventory item"
                                   : "Remove inventory item");
            const std::string target = type == "ChangeScene"
                ? actions[i]["scene"].get<std::string>()
                : (type == "AddItem" || type == "RemoveItem")
                    ? std::to_string(actions[i].value("item", 0))
                    : actions[i]["ui"].get<std::string>();
            std::string targetLabel = target.empty() ? "(choose target)" : target;
            if (type != "ChangeScene" && !target.empty()) {
                const auto uiTargets = uiEditor.Targets();
                const auto match = std::find_if(uiTargets.begin(), uiTargets.end(),
                    [&target](const auto& entry) { return entry.first == target; });
                if (match != uiTargets.end())
                    targetLabel = match->second;
            }
            ImGui::TextUnformatted(targetLabel.c_str());
            if (i + 1 < actions.size()) {
                ed::BeginPin(ed::PinId(id + 2), ed::PinKind::Output);
                ImGui::TextUnformatted("Out  >");
                ed::EndPin();
            }
            ed::EndNode();
            ed::Link(ed::LinkId(id + 3), ed::PinId(i == 0 ? 2 : ActionNodeId(i - 1) + 2),
                     ed::PinId(id + 1));
        }
        ed::End();

        if (selectAddedAction) {
            ed::ClearSelection();
            ed::SelectNode(ed::NodeId(ActionNodeId(*selectAddedAction)));
        }
        if (initializeNodeView) {
            ed::SetNodePosition(ed::NodeId(1), ImVec2(40, 50));
            if (!actions.empty())
                ed::SelectNode(ed::NodeId(ActionNodeId(0)));
        }
        for (std::size_t i = positionedActionNodes; i < actions.size(); ++i)
            ed::SetNodePosition(ed::NodeId(ActionNodeId(i)), ImVec2(260.0f + i * 220.0f, 50));
        positionedActionNodes = actions.size();
        if (initializeNodeView) {
            ed::NavigateToContent(0.0f);
            initializeNodeView = false;
        }
        ed::SetCurrentEditor(nullptr);

        if (changed) {
            LoadObject(object);
            current->SetObject(*selectedObject, object);
        }
    } catch (const std::exception& e) {
        ed::SetCurrentEditor(nullptr);
        ImGui::TextWrapped("Cannot edit interaction: %s", e.what());
    }
    ImGui::End();
}
