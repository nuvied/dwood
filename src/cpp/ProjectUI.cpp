#include "ProjectUI.h"

#include "SceneDocument.h"
#include "SceneObject.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

std::filesystem::path ProjectUI::Path(const std::filesystem::path& sceneDirectory)
{
    return sceneDirectory / ".devilwood" / "project_ui.json";
}

std::unique_ptr<ProjectUI> ProjectUI::Load(const std::filesystem::path& sceneDirectory)
{
    const auto path = Path(sceneDirectory);
    if (!std::filesystem::exists(path))
        return nullptr;
    return FromDocument(SceneDocument::Open(path));
}

std::unique_ptr<ProjectUI> ProjectUI::FromDocument(const SceneDocument& document)
{
    auto result = std::make_unique<ProjectUI>();
    std::set<std::string> ids;
    const std::set<std::string> supported = {
        "TransformComp", "Sprite", "Panel_Sprite", "ColliderComp", "CircleCol", "TextComp",
        "InteractionComp", "InventoryComp"
    };
    std::vector<nlohmann::json> objects;
    for (std::size_t i = 0; i < document.ObjectCount(); ++i) {
        const auto data = document.Object(i);
        if (!data.is_object() || !data.contains("id") || !data["id"].is_string() ||
            data["id"].get<std::string>().empty())
            throw std::runtime_error("Every project UI element needs a non-empty ID.");
        const auto id = data["id"].get<std::string>();
        if (!ids.insert(id).second)
            throw std::runtime_error("Duplicate project UI element ID: " + id);
        for (const char* key : {"visible", "enabled", "group"})
            if (data.contains(key) && !data[key].is_boolean())
                throw std::runtime_error(std::string("Project UI ") + key + " must be a boolean.");
        const bool group = data.value("group", false);
        if (group && data.contains("parent"))
            throw std::runtime_error("Project UI groups must be root elements.");
        float alpha = 1.0f;
        if (data.contains("alpha")) {
            if (!data["alpha"].is_number())
                throw std::runtime_error("Project UI alpha must be a number from 0 to 1.");
            const double value = data["alpha"].get<double>();
            if (!std::isfinite(value) || value < 0 || value > 1)
                throw std::runtime_error("Project UI alpha must be a number from 0 to 1.");
            alpha = static_cast<float>(value);
        }
        auto object = LoadObject(data);
        for (auto component = data["components"].begin(); component != data["components"].end(); ++component)
            if (supported.find(component.key()) == supported.end())
                throw std::runtime_error("Unsupported project UI component: " + component.key());
        if (const auto* sprite = object.entity->getComponent<Sprite>(); sprite && !SourceFits(*sprite))
            throw std::runtime_error("Project UI sprite is outside its atlas or atlas is unavailable.");
        if (const auto* text = object.entity->getComponent<TextComp>(); text && !text->FontName().empty()) {
            std::string error;
            if (!ResourcesLoader::GetCustomFont(text->FontName(), &error))
                throw std::runtime_error(error);
        }
        Element element;
        element.id = id;
        element.parent = document.Parent(i);
        element.visible = data.value("visible", true);
        element.enabled = data.value("enabled", true);
        element.group = group;
        element.alpha = element.fadeInAlpha = alpha;
        result->elements.push_back(std::move(element));
        objects.push_back(data);
    }
    auto graph = LoadSceneGraph(objects, true);
    for (std::size_t i = 0; i < graph.objects.size(); ++i)
        result->elements[i].entity = graph.objects[i];
    result->roots = std::move(graph.roots);
    for (std::size_t i = 0; i < result->elements.size(); ++i)
        if (!result->elements[i].parent)
            result->AppendDrawOrder(i);
    return result;
}

void ProjectUI::AppendDrawOrder(std::size_t index)
{
    drawOrder.push_back(index);
    for (std::size_t i = 0; i < elements.size(); ++i)
        if (elements[i].parent && *elements[i].parent == index)
            AppendDrawOrder(i);
}

bool ProjectUI::HasElement(const std::string& id) const
{
    return std::any_of(elements.begin(), elements.end(),
                       [&id](const Element& element) { return element.id == id; });
}

ProjectUI::Element& ProjectUI::Find(const std::string& id)
{
    const auto found = std::find_if(elements.begin(), elements.end(),
        [&id](const Element& element) { return element.id == id; });
    if (found == elements.end())
        throw std::runtime_error("Project UI element not found: " + id);
    return *found;
}

const ProjectUI::Element& ProjectUI::Find(const std::string& id) const
{
    const auto found = std::find_if(elements.begin(), elements.end(),
        [&id](const Element& element) { return element.id == id; });
    if (found == elements.end())
        throw std::runtime_error("Project UI element not found: " + id);
    return *found;
}

Entity& ProjectUI::GetElement(const std::string& id) { return *Find(id).entity; }
bool ProjectUI::IsVisible(const std::string& id) const { return Find(id).visible; }
bool ProjectUI::IsEnabled(const std::string& id) const { return Find(id).enabled; }
bool ProjectUI::IsGroup(const std::string& id) const { return Find(id).group; }
float ProjectUI::Alpha(const std::string& id) const { return Find(id).alpha; }
bool ProjectUI::IsFading(const std::string& id) const { return Find(id).fading; }
bool ProjectUI::IsFadingIn(const std::string& id) const { return Find(id).hideAfterFade == false; }

bool ProjectUI::IsEffectivelyVisible(const std::string& id) const
{
    const Element* current = &Find(id);
    while (current) {
        if (!current->visible) return false;
        current = current->parent ? &elements[*current->parent] : nullptr;
    }
    return true;
}

bool ProjectUI::EffectiveEnabled(const Element& element) const
{
    const Element* current = &element;
    while (current) {
        if (!current->enabled) return false;
        current = current->parent ? &elements[*current->parent] : nullptr;
    }
    return true;
}

float ProjectUI::EffectiveAlpha(const std::string& id) const
{
    const Element* current = &Find(id);
    float alpha = 1.0f;
    while (current) {
        alpha *= current->alpha;
        current = current->parent ? &elements[*current->parent] : nullptr;
    }
    return alpha;
}

void ProjectUI::SetVisible(const std::string& id, bool visible)
{
    auto& element = Find(id);
    if (visible && !element.visible && element.hideAfterFade)
        element.alpha = element.fadeInAlpha;
    element.visible = visible;
    element.fading = false;
    element.hideAfterFade = false;
}

void ProjectUI::SetEnabled(const std::string& id, bool enabled) { Find(id).enabled = enabled; }

void ProjectUI::SetAlpha(const std::string& id, float alpha)
{
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
        throw std::invalid_argument("Project UI alpha must be from 0 to 1.");
    auto& element = Find(id);
    element.alpha = element.fadeInAlpha = alpha;
    element.fading = false;
}

void ProjectUI::FadeGroup(const std::string& id, bool fadeIn, float duration)
{
    auto& element = Find(id);
    if (!element.group)
        throw std::invalid_argument("Only project UI groups can be faded.");
    if (!std::isfinite(duration) || duration < 0)
        throw std::invalid_argument("UI fade duration must be non-negative.");
    if (fadeIn && !element.visible)
        element.alpha = 0;
    if (fadeIn)
        element.visible = true;
    element.fadeStart = element.alpha;
    element.fadeTarget = fadeIn ? element.fadeInAlpha : 0.0f;
    element.fadeDuration = duration;
    element.fadeElapsed = 0;
    element.hideAfterFade = !fadeIn;
    element.fading = duration > 0;
    if (!element.fading) {
        element.alpha = element.fadeTarget;
        if (element.hideAfterFade) element.visible = false;
    }
}

void ProjectUI::Update(float dt)
{
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("Project UI update time must be non-negative.");
    for (auto& element : elements) {
        if (element.fading) {
            element.fadeElapsed = std::min(element.fadeDuration, element.fadeElapsed + dt);
            const float t = element.fadeElapsed / element.fadeDuration;
            element.alpha = element.fadeStart + (element.fadeTarget - element.fadeStart) * t;
            if (element.fadeElapsed >= element.fadeDuration) {
                element.fading = false;
                if (element.hideAfterFade) element.visible = false;
            }
        }
    }
    for (auto index : drawOrder)
        if (EffectiveEnabled(elements[index]))
            elements[index].entity->UpdateSelf(dt);
}

void ProjectUI::Draw() const
{
    for (auto index : drawOrder) {
        const auto& element = elements[index];
        if (IsEffectivelyVisible(element.id)) {
            element.entity->projectUIOpacity = EffectiveAlpha(element.id);
            element.entity->DrawSelf();
        }
    }
}

bool ProjectUI::IsUnderMouse() const
{
    for (auto it = drawOrder.rbegin(); it != drawOrder.rend(); ++it) {
        const auto& element = elements[*it];
        const auto* collider = element.entity->getComponent<ColliderComp>();
        if (EffectiveEnabled(element) && collider &&
            CheckCollisionPointRec(Global::mousePos, collider->col_rect))
            return true;
    }
    return false;
}

bool ProjectUI::TriggerAt(Vector2 point)
{
    for (auto it = drawOrder.rbegin(); it != drawOrder.rend(); ++it) {
        const auto& element = elements[*it];
        auto* interaction = element.entity->getComponent<InteractionComp>();
        const auto* collider = element.entity->getComponent<ColliderComp>();
        if (!IsEffectivelyVisible(element.id) || !EffectiveEnabled(element) ||
            !interaction || !collider ||
            !CheckCollisionPointRec(point, collider->col_rect))
            continue;
        interaction->Trigger();
        return true;
    }
    return false;
}
