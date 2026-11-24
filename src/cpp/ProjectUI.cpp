#include "ProjectUI.h"

#include "SceneDocument.h"
#include "SceneObject.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>

namespace
{
constexpr float SubtitleHoldSecondsPerCharacter = 0.15f;
constexpr float SubtitleFadeInDuration = 0.25f;
constexpr float SubtitleFadeOutDuration = 0.25f;

std::vector<std::size_t> Utf8CharacterEnds(const std::string& text)
{
    std::vector<std::size_t> ends;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto lead = static_cast<unsigned char>(text[offset]);
        const std::size_t width = lead < 0x80 ? 1 :
            (lead & 0xE0) == 0xC0 ? 2 :
            (lead & 0xF0) == 0xE0 ? 3 :
            (lead & 0xF8) == 0xF0 ? 4 : 1;
        offset += std::min(width, text.size() - offset);
        ends.push_back(offset);
    }
    return ends;
}
}

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
        std::string type;
        if (data.contains("type")) {
            if (!data["type"].is_string())
                throw std::runtime_error("Project UI type must be a string.");
            type = data["type"].get<std::string>();
            if (type != "subtitle")
                throw std::runtime_error("Unsupported project UI type: " + type);
        }
        float typeInSpeed = 30.0f;
        if (type == "subtitle" && data.contains("type_in_speed")) {
            if (!data["type_in_speed"].is_number())
                throw std::runtime_error("Subtitle type-in speed must be positive.");
            const double speed = data["type_in_speed"].get<double>();
            if (!std::isfinite(speed) || speed <= 0 ||
                speed > std::numeric_limits<float>::max())
                throw std::runtime_error("Subtitle type-in speed must be positive.");
            typeInSpeed = static_cast<float>(speed);
        }
        for (const char* key : {"visible", "enabled", "group"})
            if (data.contains(key) && !data[key].is_boolean())
                throw std::runtime_error(std::string("Project UI ") + key + " must be a boolean.");
        const bool group = data.value("group", false);
        if (group && data.contains("parent"))
            throw std::runtime_error("Project UI groups must be root elements.");
        if (type == "subtitle" && !group)
            throw std::runtime_error("Subtitle UI elements must be groups.");
        if (type == "subtitle" && result->subtitleGroup)
            throw std::runtime_error("Project UI can contain only one subtitle group.");
        std::string subtitleTextId;
        if (type == "subtitle") {
            if (!data.contains("subtitle_text") || !data["subtitle_text"].is_string() ||
                data["subtitle_text"].get<std::string>().empty())
                throw std::runtime_error("Subtitle groups must select a child text object.");
            subtitleTextId = data["subtitle_text"].get<std::string>();
        }
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
        element.visible = data.value("visible", type != "subtitle");
        element.enabled = data.value("enabled", true);
        element.group = group;
        element.subtitle = type == "subtitle";
        element.typeInSpeed = typeInSpeed;
        element.subtitleTextId = subtitleTextId;
        element.alpha = element.fadeInAlpha = alpha;
        if (element.subtitle)
            result->subtitleGroup = result->elements.size();
        result->elements.push_back(std::move(element));
        objects.push_back(data);
    }
    auto graph = LoadSceneGraph(objects, true);
    for (std::size_t i = 0; i < graph.objects.size(); ++i)
        result->elements[i].entity = graph.objects[i];
    result->roots = std::move(graph.roots);
    if (result->subtitleGroup) {
        const auto groupIndex = *result->subtitleGroup;
        auto& group = result->elements[groupIndex];
        const auto textElement = std::find_if(
            result->elements.begin(), result->elements.end(),
            [&group](const Element& candidate) {
                return candidate.id == group.subtitleTextId;
            });
        if (textElement == result->elements.end() ||
            textElement->parent != groupIndex ||
            !textElement->entity->getComponent<TextComp>())
            throw std::runtime_error(
                "Subtitle groups must select a child UI object with a Text component.");
        group.subtitleTextElement =
            static_cast<std::size_t>(std::distance(result->elements.begin(), textElement));
    }
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
bool ProjectUI::IsSubtitle(const std::string& id) const { return Find(id).subtitle; }
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
    if (element.subtitle && !visible) {
        element.subtitleActive = false;
        element.subtitleTyping = false;
        element.subtitleHolding = false;
    }
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

void ProjectUI::ShowSubtitle(const std::string& text)
{
    if (!subtitleGroup)
        throw std::runtime_error("Project UI has no subtitle group.");
    auto& element = elements[*subtitleGroup];

    element.subtitleText = text;
    element.subtitleCharacterEnds = Utf8CharacterEnds(text);
    element.subtitleTypeElapsed = 0.0f;
    element.subtitleHoldElapsed = 0.0f;
    element.subtitleHoldDuration =
        element.subtitleCharacterEnds.size() * SubtitleHoldSecondsPerCharacter;
    element.subtitleTyping = !element.subtitleCharacterEnds.empty();
    element.subtitleHolding = !element.subtitleTyping;
    element.subtitleActive = true;
    auto* textComponent =
        elements[*element.subtitleTextElement].entity->getComponent<TextComp>();
    textComponent->SetText({});
    FadeGroup(element.id, true, SubtitleFadeInDuration);
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
                if (element.hideAfterFade) {
                    element.visible = false;
                    if (element.subtitle)
                        element.subtitleActive = false;
                }
            }
        }
    }
    for (auto index : drawOrder)
        if (EffectiveEnabled(elements[index]))
            elements[index].entity->UpdateSelf(dt);
    for (auto& element : elements) {
        if (!element.subtitleActive)
            continue;
        if (element.subtitleTyping) {
            element.subtitleTypeElapsed += dt;
            const float revealPosition =
                element.subtitleTypeElapsed * element.typeInSpeed;
            const std::size_t characterCount = element.subtitleCharacterEnds.size();
            const std::size_t revealed = !std::isfinite(revealPosition) ||
                    revealPosition >= characterCount
                ? characterCount : static_cast<std::size_t>(revealPosition);
            const std::size_t byteCount = revealed == 0
                ? 0 : element.subtitleCharacterEnds[revealed - 1];
            const auto visibleText = element.subtitleText.substr(0, byteCount);
            auto* textComponent =
                elements[*element.subtitleTextElement].entity->getComponent<TextComp>();
            textComponent->SetText(visibleText);
            if (revealed == characterCount) {
                element.subtitleTyping = false;
                element.subtitleHolding = true;
            }
        } else if (element.subtitleHolding) {
            element.subtitleHoldElapsed += dt;
            if (element.subtitleHoldElapsed >= element.subtitleHoldDuration) {
                element.subtitleHolding = false;
                element.fadeStart = element.alpha;
                element.fadeTarget = 0.0f;
                element.fadeDuration = SubtitleFadeOutDuration;
                element.fadeElapsed = 0.0f;
                element.fading = true;
                element.hideAfterFade = true;
            }
        }
    }
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
        if (!EffectiveEnabled(element))
            continue;
        if (const auto* inventory = element.entity->getComponent<InventoryUIComp>();
            inventory && IsEffectivelyVisible(element.id) && inventory->Contains(Global::mousePos))
            return true;
        const auto* collider = element.entity->getComponent<ColliderComp>();
        if (collider && CheckCollisionPointRec(Global::mousePos, collider->col_rect))
            return true;
    }
    return false;
}

bool ProjectUI::TriggerAt(Vector2 point)
{
    for (auto it = drawOrder.rbegin(); it != drawOrder.rend(); ++it) {
        const auto& element = elements[*it];
        if (!IsEffectivelyVisible(element.id) || !EffectiveEnabled(element))
            continue;
        if (auto* inventory = element.entity->getComponent<InventoryUIComp>();
            inventory && inventory->ClickAt(point))
            return true;
        auto* interaction = element.entity->getComponent<InteractionComp>();
        const auto* collider = element.entity->getComponent<ColliderComp>();
        if (!interaction || !collider ||
            !CheckCollisionPointRec(point, collider->col_rect))
            continue;
        interaction->Trigger();
        return true;
    }
    return false;
}
