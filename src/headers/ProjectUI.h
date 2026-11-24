#pragma once

#include "Entity.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ProjectUI
{
public:
    static std::filesystem::path Path(const std::filesystem::path& sceneDirectory);
    static std::unique_ptr<ProjectUI> Load(const std::filesystem::path& sceneDirectory);
    static std::unique_ptr<ProjectUI> FromDocument(const class SceneDocument& document);

    bool HasElement(const std::string& id) const;
    Entity& GetElement(const std::string& id);
    bool IsSubtitle(const std::string& id) const;
    bool IsVisible(const std::string& id) const;
    bool IsEnabled(const std::string& id) const;
    void SetVisible(const std::string& id, bool visible);
    void SetEnabled(const std::string& id, bool enabled);
    bool IsGroup(const std::string& id) const;
    bool IsEffectivelyVisible(const std::string& id) const;
    float Alpha(const std::string& id) const;
    float EffectiveAlpha(const std::string& id) const;
    void SetAlpha(const std::string& id, float alpha);
    void ShowSubtitle(const std::string& text);
    void FadeGroup(const std::string& id, bool fadeIn, float duration);
    bool IsFading(const std::string& id) const;
    bool IsFadingIn(const std::string& id) const;
    const std::vector<std::size_t>& DrawOrder() const { return drawOrder; }
    void Update(float dt);
    void Draw() const;
    bool IsUnderMouse() const;
    bool TriggerAt(Vector2 point);

private:
    struct Element {
        std::string id;
        Entity* entity = nullptr;
        std::optional<std::size_t> parent;
        bool visible = true;
        bool enabled = true;
        bool group = false;
        bool subtitle = false;
        float typeInSpeed = 30.0f;
        std::string subtitleTextId;
        std::optional<std::size_t> subtitleTextElement;
        std::string subtitleText;
        std::vector<std::size_t> subtitleCharacterEnds;
        float subtitleTypeElapsed = 0.0f;
        float subtitleHoldElapsed = 0.0f;
        float subtitleHoldDuration = 0.0f;
        bool subtitleActive = false;
        bool subtitleTyping = false;
        bool subtitleHolding = false;
        float alpha = 1.0f;
        float fadeInAlpha = 1.0f;
        float fadeStart = 0.0f;
        float fadeTarget = 0.0f;
        float fadeDuration = 0.0f;
        float fadeElapsed = 0.0f;
        bool fading = false;
        bool hideAfterFade = false;
    };
    bool EffectiveEnabled(const Element& element) const;
    void AppendDrawOrder(std::size_t index);
    Element& Find(const std::string& id);
    const Element& Find(const std::string& id) const;
    std::vector<Element> elements;
    std::vector<std::unique_ptr<Entity>> roots;
    std::vector<std::size_t> drawOrder;
    std::optional<std::size_t> subtitleGroup;
};
