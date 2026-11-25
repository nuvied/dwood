
#include "Component.h"
#include "UI_Manager.h"
#include "ResourcesLoader.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include "rlgl.h"

#pragma region TRANSFORM
TransformComp::TransformComp()
{
    position = {0};
}

TransformComp::TransformComp(float x, float y, float rot)
{
    position.x = x;
    position.y = y;
}


void TransformComp::OnChangePerent()
{
    Entity* ancestor = entity->parent;
    parent = nullptr;
    while (ancestor && !parent) {
        parent = ancestor->getComponent<TransformComp>();
        ancestor = ancestor->parent;
    }
    //auto offsetpos = position - pt->position;

}

Matrix2D TransformComp::getWorldMatrix()const{
    Matrix2D local = Matrix2D::Transform(position, rotation, scale);
    if(parent) return parent->getWorldMatrix()*local;
    return local;
}

Vector2 TransformComp::getWorldScale() const
{
    const Matrix2D world = getWorldMatrix();
    return {std::sqrt(world.m[0][0] * world.m[0][0] + world.m[1][0] * world.m[1][0]),
            std::sqrt(world.m[0][1] * world.m[0][1] + world.m[1][1] * world.m[1][1])};
}

Vector2 TransformComp::getWorldPosition() const{
    return getWorldMatrix().TransformVector({0,0});
}

float TransformComp::getWorldRotation()const{
    return parent? parent->getWorldRotation() + rotation :rotation;
}



void TransformComp::Update(float dt)
{

}
void TransformComp::Draw()
{

}

#pragma endregion

#pragma region SPRITE

Sprite::Sprite()
{
    
}
Sprite::Sprite(Texture2D tex, Rectangle rec, Vector2 origin, Color c)
{
    texture = tex;
    src_rec = rec;
    this->origin = origin;
    color = c;
    
}
    // Sprite(Sprite& sp)
    // {
    //     texture = sp.texture;
    //     src_rec = sp.src_rec;
    //     origin = sp.origin;
    //     color = sp.color;
        
    // }
Sprite::~Sprite()
{
    //UnloadTexture(texture);
}
    
void Sprite::setOwner(Entity* e) 
{
    Component::setOwner(e);
    trans = entity->getComponent<TransformComp>();
    panel = entity->getComponent<Panel_Sprite>();
    
}
    
void Sprite::Update(float dt) 
{
    // std::cout << texture.width << std::endl;
    // for(auto& c: entity->childs)
    // {
    //     c->Update(dt);
    // }
}
    
void Sprite::setOrigin(float x, float y)
{
    origin = {x,y};
}

Vector2 Sprite::getWorldPosition() const
{
    const auto* transform = entity ? entity->getComponent<TransformComp>() : nullptr;
    return transform ? transform->getWorldMatrix().TransformVector(offset) : offset;
}
    
void Sprite::Draw()
{
    if(!entity)return;
    //TransformComp* t = entity->getComponent<TransformComp>();
    if(!trans)
        trans = entity->getComponent<TransformComp>(); 
    float a;
    if (entity->projectUIOpacity) {
        a = Clamp(alpha * *entity->projectUIOpacity * (color.a / 255.0f), 0.0f, 1.0f);
    }
    else if(entity->parent)
    {
        a = alpha * entity->parent->alpha;
       
        a = Clamp(a, 0,1.0f);
        entity->alpha = a;
    }
    else
    a = alpha;
    if(trans){

        Vector2 worldPos = getWorldPosition();
        Vector2 worldScale = trans->getWorldScale();
        float worldRot = trans->getWorldRotation();
        if(panel)
        {
            panel->info.source = src_rec;
            Rectangle target = panel->targetRect;
            target.x = worldPos.x;
            target.y = worldPos.y;
            target.width *= worldScale.x;
            target.height *= worldScale.y;
            DrawTextureNPatch(texture, panel->info, target,origin, worldRot, Fade(color, a));
        }
        else
        {
            DrawTexturePro(texture, src_rec,
                           {worldPos.x, worldPos.y, src_rec.width * worldScale.x,
                            src_rec.height * worldScale.y
                           },
                           {origin.x * src_rec.width * worldScale.x,
                            origin.y * src_rec.height * worldScale.y},
                           worldRot,
                           Fade(color, a));
/*
            for(auto& c: entity->childs)
            {
                c->Draw();
            }*/
        }
    }
    else
    {
        DrawTextureRec(texture, src_rec, offset,
                       Fade(color, entity->projectUIOpacity ? a : color.a));
    }
    
    // DrawTexture(ResourcesLoader::ui_page, 0,0,WHITE);
}

#pragma endregion

void RectangleComp::Draw()
{
    if (!entity || !entity->isActive())
        return;

    const auto* transform = entity->getComponent<TransformComp>();
    const Vector2 position = transform ? transform->getWorldPosition() : Vector2{0, 0};
    const Vector2 scale = transform ? transform->getWorldScale() : Vector2{1, 1};
    const float rotation = transform ? transform->getWorldRotation() : 0.0f;
    DrawRectanglePro({position.x, position.y, width * scale.x, height * scale.y},
                     {0, 0}, rotation, Fade(color, opacity));
}

#pragma region COLLIDER
ColliderComp::ColliderComp()
{
    col_rect = {0};
    local_rect = col_rect;
}

ColliderComp::ColliderComp(Rectangle &rec)
{
    col_rect = rec;
    local_rect = rec;
}

ColliderComp::ColliderComp(float x, float y, float w, float h)
{
    col_rect = {x,y,w,h};
    local_rect = col_rect;

}

void ColliderComp::setOwner(Entity* e)
{
    Component::setOwner(e);
    sprite = entity->getComponent<Sprite>();
    transform = entity->getComponent<TransformComp>();
    pSprite = entity->getComponent<Panel_Sprite>();
    SetRect();
}



void ColliderComp::Update(float dt)
{
    /*
    if(CheckCollisionPointRec(Global::mousePos, col_rect))
    {
        if(IsMouseButtonDown(0))
            entity->OnClicked();
    }*/
    SetRect();
}

void ColliderComp::Draw()
{
    if(Global::debug)
        if(transform)
            {
                Vector2 worldPos = transform->getWorldPosition();
                DrawRectangleLines(worldPos.x, worldPos.y, col_rect.width, col_rect.height, RED);
            }
            else
            {
                DrawRectangleLines(col_rect.x, col_rect.y, col_rect.width, col_rect.height, RED);
            }
}

void ColliderComp::SetRect()
{
    if(transform)
    {
        Vector2 worldPos = transform->getWorldPosition();
        const Vector2 worldScale = transform->getWorldScale();
        const float width = pSprite ? pSprite->targetRect.width :
                            sprite ? sprite->src_rec.width : local_rect.width;
        const float height = pSprite ? pSprite->targetRect.height :
                             sprite ? sprite->src_rec.height : local_rect.height;
        col_rect = {worldPos.x, worldPos.y, width * worldScale.x, height * worldScale.y};
    } else {
        col_rect = local_rect;
    }

}

Vector2 ColliderComp::LocalSize() const
{
    if (pSprite)
        return {pSprite->targetRect.width, pSprite->targetRect.height};
    if (sprite)
        return {sprite->src_rec.width, sprite->src_rec.height};
    return {local_rect.width, local_rect.height};
}
#pragma endregion

#pragma region CIRCLECOL

CircleCol::CircleCol()
{
    
}
CircleCol::CircleCol(float x, float y, float r)
{
    c = {x, y, r};
    localRadius = r;
}

void CircleCol::setOwner(Entity *e)
{
    Component::setOwner(e);
    if(e)
    {
        transform = e->getComponent<TransformComp>();
    }
}
void CircleCol::Update(float dt)
{
    if(transform)
    {
        const Vector2 worldPosition = transform->getWorldPosition();
        const Vector2 worldScale = transform->getWorldScale();
        c.x = worldPosition.x;
        c.y = worldPosition.y;
        c.radius = localRadius * std::max(worldScale.x, worldScale.y);
    }
}

void CircleCol::Draw()
{
    
    if(Global::debug){
        DrawCircleLines(c.x, c.y, c.radius, RED);
    }
}

#pragma endregion CIRCLECOL

#pragma region BEHAVIOR
Behaviour::Behaviour()
{
    
}


void Behaviour::setOwner(Entity* e){
    Component::setOwner(e);
    col = entity->getComponent<ColliderComp>();
    circle_col = entity->getComponent<CircleCol>();
    Init();

}

Entity* Behaviour::getEntity(int id)
{
    return nullptr;
}

Entity* Behaviour::getEntity(std::string name)
{
    return Game::get_Instance().getEntity(name);
}

void Behaviour::Init()
{
    
}

void Behaviour::SceneLoaded()
{
    
}

void Behaviour::OnMouseDown()
{
    
    
}
void Behaviour::OnMouseUp()
{
    
}
void Behaviour::OnMouseHeld()
{
    
}
void Behaviour::Update(float dt)
{
    if(Game::get_Instance().IsOverUI) return;

    if(circle_col){
        if(CheckCollisionPointCircle(Global::mousePos, {circle_col->c.x, circle_col->c.y}, circle_col->c.radius))
        {
            if(IsMouseButtonPressed(0))
            {
                OnMouseDown();
            }
            else if(IsMouseButtonReleased(0))
            {
                OnMouseUp();
            }
        }
    }
    
    
    //rectangle collision detection
    
    
    if(!col)return;
    
    if(CheckCollisionPointRec(Global::mousePos, col->col_rect))
    {
        if(IsMouseButtonPressed(0))
        {
            OnMouseDown();
            
        }
        
    }
}
void Behaviour::Draw()
{
   // DrawRectangleRec(col->col_rect, Fade(RED, 0.3f));
   // DrawRectangle(Global::mousePos.x, Global::mousePos.y, 64, 32, RED);
}
#pragma endregion BEHAVIOUR

#pragma region PANEL
Panel_Sprite::Panel_Sprite( float margin, Rectangle rect) {

    info.source = {0};
    info.left = margin;
    info.right = margin;
    info.top = margin;
    info.bottom= margin;

    info.layout = NPATCH_NINE_PATCH;
    targetRect = rect;
}



#pragma endregion

AudioComp::AudioComp(std::string asset, float volume, bool playOnStart)
    : asset(std::move(asset)), volume(volume), playOnStart(playOnStart)
{
}

AudioComp::~AudioComp()
{
    if (loaded && IsAudioDeviceReady()) {
        StopMusicStream(music);
        UnloadMusicStream(music);
    }
}

void AudioComp::Load()
{
    if (!IsAudioDeviceReady())
        throw std::runtime_error("Cannot play audio before the audio device is initialized.");
    const auto path = AssetPaths::Resource(asset);
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Audio asset does not exist: " + path.string());
    music = LoadMusicStream(path.string().c_str());
    if (!IsMusicValid(music))
        throw std::runtime_error("Cannot load audio asset: " + path.string());
    SetMusicVolume(music, volume);
    loaded = true;
}

void AudioComp::Play()
{
    if (!loaded)
        Load();
    if (paused) {
        ResumeMusicStream(music);
        paused = false;
    } else {
        PlayMusicStream(music);
    }
}

void AudioComp::Pause()
{
    if (loaded && IsMusicStreamPlaying(music)) {
        PauseMusicStream(music);
        paused = true;
    }
}

void AudioComp::Stop()
{
    if (loaded) {
        StopMusicStream(music);
        paused = false;
    }
}

void AudioComp::Update(float)
{
    if (loaded && IsMusicStreamPlaying(music))
        UpdateMusicStream(music);
}

namespace
{
class ChangeSceneAction : public Action
{
public:
    explicit ChangeSceneAction(std::filesystem::path path) : path(std::move(path)) {}
    void OnStart() override { Game::get_Instance().ChangeSceneF(path); }
    void Update(float) override {}
    bool IsFinished() override { return !Game::get_Instance().fm.fadeout; }
private:
    std::filesystem::path path;
};

class OpenPopupSceneAction : public Action
{
public:
    OpenPopupSceneAction(std::filesystem::path path, float duration)
        : path(std::move(path)), duration(duration) {}
    void OnStart() override { Game::get_Instance().OpenPopupSceneF(path, duration); }
    void Update(float) override {}
    bool IsFinished() override {
        const auto& fade = Game::get_Instance().fm;
        return !fade.fadeout && !fade.fadein;
    }
private:
    std::filesystem::path path;
    float duration;
};

class ClosePopupSceneAction : public Action
{
public:
    explicit ClosePopupSceneAction(float duration) : duration(duration) {}
    void OnStart() override {
        Game::get_Instance().ClosePopupSceneF(duration);
    }
    void Update(float) override {}
    bool IsFinished() override {
        const auto& fade = Game::get_Instance().fm;
        return !fade.fadeout && !fade.fadein;
    }
private:
    float duration;
};

class AudioPlaybackAction : public Action
{
public:
    AudioPlaybackAction(AudioComp* audio, InteractionComp::ActionData::Type type)
        : audio(audio), type(type) {}
    void OnStart() override {
        using Type = InteractionComp::ActionData::Type;
        try {
            if (type == Type::PlayAudio)
                audio->Play();
            else if (type == Type::PauseAudio)
                audio->Pause();
            else
                audio->Stop();
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Audio interaction action failed: %s", e.what());
        }
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    AudioComp* audio;
    InteractionComp::ActionData::Type type;
};

class ManagedSoundAction : public Action
{
public:
    ManagedSoundAction(int soundId, InteractionComp::ActionData::Type type, float duration)
        : soundId(soundId), type(type), duration(duration) {}
    void OnStart() override {
        auto& manager = Game::get_Instance().soundManager;
        using Type = InteractionComp::ActionData::Type;
        try {
            if (type == Type::SoundOn)
                manager.On(soundId);
            else if (type == Type::SoundOff)
                manager.Off(soundId);
            else if (type == Type::SoundFadeIn)
                manager.FadeIn(soundId, duration);
            else
                manager.FadeOut(soundId, duration);
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Sound manager interaction action failed: %s", e.what());
            failed = true;
        }
    }
    void Update(float) override {}
    bool IsFinished() override {
        if (failed)
            return true;
        using Type = InteractionComp::ActionData::Type;
        if (type != Type::SoundFadeIn && type != Type::SoundFadeOut)
            return true;
        return !Game::get_Instance().soundManager.IsFading(soundId);
    }
private:
    int soundId;
    InteractionComp::ActionData::Type type;
    float duration;
    bool failed = false;
};

class SetUIVisibilityAction : public Action
{
public:
    SetUIVisibilityAction(std::string id, bool visible)
        : id(std::move(id)), visible(visible) {}
    void OnStart() override {
        auto* manager = Game::get_Instance().ui_m;
        if (!manager || !manager->projectUI || !manager->projectUI->HasElement(id)) {
            TraceLog(LOG_ERROR, "Cannot change visibility of missing project UI element: %s", id.c_str());
            return;
        }
        manager->projectUI->SetVisible(id, visible);
        manager->projectUI->SetEnabled(id, visible);
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    std::string id;
    bool visible;
};

class ShowSubtitleAction : public Action
{
public:
    explicit ShowSubtitleAction(std::string text) : text(std::move(text)) {}
    void OnStart() override {
        auto* manager = Game::get_Instance().ui_m;
        if (!manager || !manager->projectUI) {
            TraceLog(LOG_ERROR, "Cannot display subtitle without a project UI manager.");
            return;
        }
        try {
            manager->projectUI->ShowSubtitle(text);
            TraceLog(LOG_INFO, "[GAME] Subtitle shown: %s", text.c_str());
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Cannot display subtitle: %s", e.what());
        }
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    std::string text;
};

class ProjectUIAction : public Action
{
public:
    explicit ProjectUIAction(InteractionComp::ActionData action) : action(std::move(action)) {}
    void OnStart() override {
        auto* manager = Game::get_Instance().ui_m;
        if (!manager || !manager->projectUI || !manager->projectUI->HasElement(action.target)) {
            TraceLog(LOG_ERROR, "Cannot run action on missing project UI: %s", action.target.c_str());
            failed = true;
            return;
        }
        auto& ui = *manager->projectUI;
        using Type = InteractionComp::ActionData::Type;
        if (action.type == Type::EnableUI || action.type == Type::DisableUI)
            ui.SetEnabled(action.target, action.type == Type::EnableUI);
        else if (action.type == Type::ToggleUI) {
            const bool nextVisible = !ui.IsVisible(action.target);
            ui.SetVisible(action.target, nextVisible);
            ui.SetEnabled(action.target, nextVisible);
        }
        else if (!ui.IsGroup(action.target)) {
            TraceLog(LOG_ERROR, "Cannot fade non-group project UI: %s", action.target.c_str());
            failed = true;
        } else {
            const bool fadeIn = action.type == Type::FadeInUI ||
                (action.type == Type::ToggleFadeUI &&
                 (ui.IsFading(action.target) ? !ui.IsFadingIn(action.target)
                                             : !ui.IsVisible(action.target)));
            ui.FadeGroup(action.target, fadeIn, action.duration);
        }
    }
    void Update(float) override {}
    bool IsFinished() override {
        if (failed) return true;
        using Type = InteractionComp::ActionData::Type;
        if (action.type == Type::EnableUI || action.type == Type::DisableUI ||
            action.type == Type::ToggleUI)
            return true;
        auto* manager = Game::get_Instance().ui_m;
        if (!manager || !manager->projectUI || !manager->projectUI->HasElement(action.target)) {
            TraceLog(LOG_ERROR, "Project UI disappeared during an action: %s", action.target.c_str());
            return true;
        }
        return !manager->projectUI->IsFading(action.target);
    }
private:
    InteractionComp::ActionData action;
    bool failed = false;
};

class AddItemAction : public Action
{
public:
    explicit AddItemAction(int itemId) : itemId(itemId) {}
    void OnStart() override { Game::get_Instance().AddItem(itemId); }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    int itemId;
};

class RemoveItemAction : public Action
{
public:
    explicit RemoveItemAction(int itemId) : itemId(itemId) {}
    void OnStart() override { Game::get_Instance().removeItem(itemId); }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    int itemId;
};

class SetObjectActiveAction : public Action
{
public:
    SetObjectActiveAction(Entity* object, bool active) : object(object), active(active) {}
    void OnStart() override {
        if (object) {
            object->setActive(active);
            TraceLog(LOG_INFO, "[GAME] Object '%s' %s",
                     object->name.c_str(), active ? "shown" : "hidden");
        }
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    Entity* object;
    bool active;
};

class ChangeHotspotSettingAction : public Action
{
public:
    explicit ChangeHotspotSettingAction(InteractionComp::ActionData action)
        : action(std::move(action)) {}
    void OnStart() override {
        if (!action.object)
            return;
        auto* hotspot = action.object->getComponent<HotspotComp>();
        if (!hotspot) {
            TraceLog(LOG_ERROR, "Target object has no Hotspot component.");
            return;
        }
        hotspot->interaction = action.hotspotInteraction;
        hotspot->checkVariable = action.hotspotCheckVariable;
        hotspot->variableScope = action.variableScope;
        hotspot->variableId = action.variableId;
        hotspot->requireItem = action.hotspotRequireItem;
        hotspot->requiredItemId = action.itemId;
        hotspot->consumeItem = action.hotspotConsumeItem;
        hotspot->disableOnSuccess = action.hotspotDisableOnSuccess;
        hotspot->failMessage = action.text;
        TraceLog(LOG_INFO, "[GAME] Hotspot '%s' settings updated", action.object->name.c_str());
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    InteractionComp::ActionData action;
};

class RunScriptAction : public Action
{
public:
    RunScriptAction(std::string scriptAsset, std::string functionName, Entity* owner)
        : scriptAsset(std::move(scriptAsset)),
          functionName(std::move(functionName)),
          owner(owner) {}
    void OnStart() override {
        Game::get_Instance().scriptEngine.RunScriptAction(scriptAsset, functionName, owner);
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    std::string scriptAsset;
    std::string functionName;
    Entity* owner;
};

class SaveSlotAction : public Action
{
public:
    SaveSlotAction(InteractionComp::ActionData::Type type, int slot)
        : type(type), slot(slot) {}
    void OnStart() override {
        auto& game = Game::get_Instance();
        using Type = InteractionComp::ActionData::Type;
        if (type == Type::SaveGame) {
            game.SaveGame(slot);
        } else if (type == Type::DeleteSave) {
            game.DeleteSaveGame(slot);
        } else {
            if (game.HasSaveGame(slot)) {
                fadingLoad = true;
                game.LoadGameF(slot);
            } else {
                TraceLog(LOG_WARNING, "[GAME] Save slot %d does not exist.", slot);
            }
        }
    }
    void Update(float) override {}
    bool IsFinished() override {
        if (!fadingLoad)
            return true;
        return !Game::get_Instance().fm.fadeout;
    }
private:
    InteractionComp::ActionData::Type type;
    int slot;
    bool fadingLoad = false;
};

class LensAction : public Action
{
public:
    explicit LensAction(InteractionComp::ActionData::Type type) : type(type) {}
    void OnStart() override {
        auto& game = Game::get_Instance();
        using Type = InteractionComp::ActionData::Type;
        if (type == Type::EnableLens)
            game.SetLensEnabled(true);
        else if (type == Type::DisableLens)
            game.SetLensEnabled(false);
        else
            game.SetLensEnabled(!game.IsLensEnabled());
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    InteractionComp::ActionData::Type type;
};

std::unique_ptr<ActionList> BuildInteractionActions(
    const std::vector<InteractionComp::ActionData>& actions, Game& game);

class SetVariableAction : public Action
{
public:
    SetVariableAction(VariableScope scope, std::string id, VariableValue value)
        : scope(scope), id(std::move(id)), value(std::move(value)) {}
    void OnStart() override {
        try {
            Game::get_Instance().variables.Set(scope, id, value);
            TraceLog(LOG_INFO, "[GAME] Variable set (%s): %s",
                     scope == VariableScope::Global ? "Global" : "Scene", id.c_str());
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Variable assignment action failed: %s", e.what());
        }
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    VariableScope scope;
    std::string id;
    VariableValue value;
};

class IfElseAction : public Action
{
public:
    IfElseAction(VariableScope scope, std::string id, std::string comparison,
                 VariableValue value, std::unique_ptr<ActionList> thenActions,
                 std::unique_ptr<ActionList> elseActions)
        : scope(scope), id(std::move(id)), comparison(std::move(comparison)),
          value(std::move(value)), thenActions(std::move(thenActions)),
          elseActions(std::move(elseActions)) {}
    void OnStart() override {
        try {
            const auto& currentValue = Game::get_Instance().variables.Get(scope, id);
            activeBranch = VariableStore::Compare(currentValue, comparison, value)
                ? thenActions.get() : elseActions.get();
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "If/Else condition failed: %s", e.what());
            failed = true;
        }
    }
    void Update(float dt) override {
        if (activeBranch)
            activeBranch->Update(dt);
    }
    bool IsFinished() override {
        return failed || !activeBranch || activeBranch->IsFinished();
    }
private:
    VariableScope scope;
    std::string id;
    std::string comparison;
    VariableValue value;
    std::unique_ptr<ActionList> thenActions;
    std::unique_ptr<ActionList> elseActions;
    ActionList* activeBranch = nullptr;
    bool failed = false;
};

std::unique_ptr<Action> BuildInteractionAction(
    const InteractionComp::ActionData& action, Game& game)
{
    using Type = InteractionComp::ActionData::Type;
    if (action.type == Type::SetVariable) {
        const auto& currentValue = game.variables.Get(action.variableScope, action.variableId);
        if (currentValue.index() != action.variableValue.index())
            throw std::runtime_error("Variable assignment value does not match its definition.");
        return std::make_unique<SetVariableAction>(
            action.variableScope, action.variableId, action.variableValue);
    }
    if (action.type == Type::IfElse) {
        const auto& currentValue = game.variables.Get(action.variableScope, action.variableId);
        if (currentValue.index() != action.variableValue.index())
            throw std::runtime_error("If/Else condition value does not match its variable.");
        VariableStore::Compare(currentValue, action.comparison, action.variableValue);
        return std::make_unique<IfElseAction>(
            action.variableScope, action.variableId, action.comparison, action.variableValue,
            BuildInteractionActions(action.thenActions, game),
            BuildInteractionActions(action.elseActions, game));
    }
    if (action.type == Type::AddItem || action.type == Type::RemoveItem) {
        if (!game.itemDB.HasItem(action.itemId))
            throw std::runtime_error("Interaction target inventory item is unavailable: " +
                                     std::to_string(action.itemId));
        if (action.type == Type::AddItem)
            return std::make_unique<AddItemAction>(action.itemId);
        return std::make_unique<RemoveItemAction>(action.itemId);
    }
    if (action.type == Type::ChangeScene) {
        if (!game.scene_manager)
            throw std::runtime_error("Scene manager is unavailable.");
        const auto registered = game.scene_manager->MainScenes();
        const auto path = game.scene_manager->Directory() / action.target;
        if (std::find(registered.begin(), registered.end(), path) == registered.end())
            throw std::runtime_error("Interaction target main scene is not registered: " + action.target);
        return std::make_unique<ChangeSceneAction>(path);
    }
    if (action.type == Type::OpenPopupScene) {
        if (!game.scene_manager)
            throw std::runtime_error("Scene manager is unavailable.");
        const auto registered = game.scene_manager->RegisteredScenes();
        const auto path = game.scene_manager->Directory() / action.target;
        if (std::find(registered.begin(), registered.end(), path) == registered.end() ||
            game.scene_manager->Type(path) != SceneType::Popup)
            throw std::runtime_error("Interaction target popup scene is not registered: " +
                                     action.target);
        return std::make_unique<OpenPopupSceneAction>(path, action.duration);
    }
    if (action.type == Type::ClosePopupScene)
        return std::make_unique<ClosePopupSceneAction>(action.duration);
    if (action.type == Type::ShowSubtitle) {
        if (!game.ui_m || !game.ui_m->projectUI) {
            TraceLog(LOG_WARNING, "Skipping subtitle action without a project UI manager.");
            return nullptr;
        }
        return std::make_unique<ShowSubtitleAction>(action.text);
    }
    if (action.type == Type::ShowObject || action.type == Type::HideObject) {
        if (!action.object)
            throw std::runtime_error("Interaction target scene object is unavailable.");
        return std::make_unique<SetObjectActiveAction>(
            action.object, action.type == Type::ShowObject);
    }
    if (action.type == Type::DisableHotspot) {
        if (!action.object || !action.object->getComponent<HotspotComp>())
            throw std::runtime_error("Interaction target hotspot is unavailable.");
        return std::make_unique<SetObjectActiveAction>(action.object, false);
    }
    if (action.type == Type::ChangeHotspotSetting) {
        if (!action.object || !action.object->getComponent<HotspotComp>())
            throw std::runtime_error("Interaction target hotspot is unavailable.");
        return std::make_unique<ChangeHotspotSettingAction>(action);
    }
    if (action.type == Type::RunScript) {
        if (!ScriptEngine::IsScriptAssetName(action.target))
            throw std::runtime_error("Interaction script asset is invalid: " + action.target);
        return std::make_unique<RunScriptAction>(action.target, action.text, action.object);
    }
    if (action.type == Type::SaveGame || action.type == Type::LoadGame ||
        action.type == Type::DeleteSave) {
        if (action.itemId < 1 || action.itemId > 99)
            throw std::runtime_error("Save slot must be between 1 and 99.");
        return std::make_unique<SaveSlotAction>(action.type, action.itemId);
    }
    if (action.type == Type::EnableLens || action.type == Type::DisableLens ||
        action.type == Type::ToggleLens) {
        return std::make_unique<LensAction>(action.type);
    }
    if (action.type == Type::PlayAudio || action.type == Type::PauseAudio ||
        action.type == Type::StopAudio) {
        if (!action.audio)
            throw std::runtime_error("Interaction audio object is unavailable.");
        return std::make_unique<AudioPlaybackAction>(action.audio, action.type);
    }
    if (action.type == Type::SoundOn || action.type == Type::SoundOff ||
        action.type == Type::SoundFadeIn || action.type == Type::SoundFadeOut) {
        if (!game.soundManager.HasSound(action.soundId))
            throw std::runtime_error("Interaction target sound is unavailable: " +
                                     std::to_string(action.soundId));
        return std::make_unique<ManagedSoundAction>(
            action.soundId, action.type, action.duration);
    }
    if (!game.ui_m || !game.ui_m->projectUI) {
        TraceLog(LOG_WARNING, "Skipping UI interaction action without a project UI manager: %s",
                 action.target.c_str());
        return nullptr;
    }
    if (!game.ui_m->projectUI->HasElement(action.target)) {
        TraceLog(LOG_WARNING, "Skipping UI interaction action for missing element: %s",
                 action.target.c_str());
        return nullptr;
    }
    if (action.type == Type::ShowUI || action.type == Type::HideUI)
        return std::make_unique<SetUIVisibilityAction>(
            action.target, action.type == Type::ShowUI);
    if ((action.type == Type::FadeInUI || action.type == Type::FadeOutUI ||
         action.type == Type::ToggleFadeUI) &&
        !game.ui_m->projectUI->IsGroup(action.target))
        throw std::runtime_error("UI fade target is not a group: " + action.target);
    return std::make_unique<ProjectUIAction>(action);
}

std::unique_ptr<ActionList> BuildInteractionActions(
    const std::vector<InteractionComp::ActionData>& actions, Game& game)
{
    auto result = std::make_unique<ActionList>();
    for (const auto& action : actions)
        if (auto runtimeAction = BuildInteractionAction(action, game))
            result->AddAction(std::move(runtimeAction));
    return result;
}
}

std::array<Vector2, 4> HotspotComp::Corners() const
{
    auto* transform = entity->getComponent<TransformComp>();
    auto* collider = entity->getComponent<ColliderComp>();
    const auto matrix = transform->getWorldMatrix();
    const auto size = collider->LocalSize();
    return {matrix.TransformVector({0, 0}), matrix.TransformVector({size.x, 0}),
            matrix.TransformVector({size.x, size.y}), matrix.TransformVector({0, size.y})};
}

bool HotspotComp::Contains(Vector2 point) const
{
    const auto corners = Corners();
    const Vector2 x{corners[1].x - corners[0].x, corners[1].y - corners[0].y};
    const Vector2 y{corners[3].x - corners[0].x, corners[3].y - corners[0].y};
    const Vector2 p{point.x - corners[0].x, point.y - corners[0].y};
    const float determinant = x.x * y.y - x.y * y.x;
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
        return false;
    const float u = (p.x * y.y - p.y * y.x) / determinant;
    const float v = (x.x * p.y - x.y * p.x) / determinant;
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
}

void HotspotComp::Trigger()
{
    auto& game = Game::get_Instance();
    const char* hotspotName = entity ? entity->name.c_str() : "Hotspot";
    if (Global::selectedItemId < 0 && game.hasItem(Global::selectedItemId)) {
        for (const auto& entry : itemInteractions) {
            if (entry.itemId == Global::selectedItemId) {
                TraceLog(LOG_INFO, "[GAME] Hotspot '%s' triggered item interaction (item %d)",
                         hotspotName, entry.itemId);
                if (entry.consumeItem)
                    game.removeItem(entry.itemId);
                if (entry.interaction)
                    entry.interaction->Trigger();
                if ((entry.disableOnSuccess || disableOnSuccess) && entity) {
                    entity->setActive(false);
                    TraceLog(LOG_INFO, "[GAME] Hotspot '%s' disabled", hotspotName);
                }
                return;
            }
        }
    }

    if (!checkVariable && !requireItem) {
        TraceLog(LOG_INFO, "[GAME] Hotspot '%s' triggered", hotspotName);
        if (interaction)
            interaction->Trigger();
        if (disableOnSuccess && entity) {
            entity->setActive(false);
            TraceLog(LOG_INFO, "[GAME] Hotspot '%s' disabled", hotspotName);
        }
        return;
    }

    auto showFailMessage = [&]() {
        TraceLog(LOG_INFO, "[GAME] Hotspot '%s' condition failed", hotspotName);
        if (failMessage.empty())
            return;
        auto* manager = game.ui_m;
        if (!manager || !manager->projectUI) {
            TraceLog(LOG_ERROR, "Cannot display hotspot message without a project UI manager.");
            return;
        }
        try {
            manager->projectUI->ShowSubtitle(failMessage);
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Cannot display hotspot message: %s", e.what());
        }
    };

    if (requireItem) {
        if (requiredItemId == 0 || Global::selectedItemId != requiredItemId ||
            !game.hasItem(requiredItemId)) {
            showFailMessage();
            return;
        }
    }

    if (checkVariable) {
        bool passed = false;
        try {
            const auto& value = game.variables.Get(variableScope, variableId);
            if (const auto* boolean = std::get_if<bool>(&value))
                passed = *boolean;
            else if (const auto* integer = std::get_if<int>(&value))
                passed = (*integer != 0);
            else if (const auto* text = std::get_if<std::string>(&value))
                passed = !text->empty();
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Hotspot variable check failed: %s", e.what());
            return;
        }
        if (!passed) {
            showFailMessage();
            return;
        }
    }

    TraceLog(LOG_INFO, "[GAME] Hotspot '%s' condition passed", hotspotName);
    if (requireItem && consumeItem)
        game.removeItem(requiredItemId);
    if (interaction)
        interaction->Trigger();
    if (disableOnSuccess && entity) {
        entity->setActive(false);
        TraceLog(LOG_INFO, "[GAME] Hotspot '%s' disabled", hotspotName);
    }
}

std::array<Vector2, 4> InventoryItemComp::Corners() const
{
    const auto* transform = entity ? entity->getComponent<TransformComp>() : nullptr;
    const auto matrix = transform ? transform->getWorldMatrix()
                                  : Matrix2D::Transform({0, 0}, 0, {1, 1});
    if (const auto* sprite = entity ? entity->getComponent<Sprite>() : nullptr) {
        const auto* panel = entity->getComponent<Panel_Sprite>();
        const float width = panel ? panel->targetRect.width : sprite->src_rec.width;
        const float height = panel ? panel->targetRect.height : sprite->src_rec.height;
        const float minX = sprite->offset.x - sprite->origin.x * width;
        const float minY = sprite->offset.y - sprite->origin.y * height;
        return {
            matrix.TransformVector({minX, minY}),
            matrix.TransformVector({minX + width, minY}),
            matrix.TransformVector({minX + width, minY + height}),
            matrix.TransformVector({minX, minY + height})
        };
    }
    if (const auto* collider = entity ? entity->getComponent<ColliderComp>() : nullptr) {
        const auto size = collider->LocalSize();
        return {
            matrix.TransformVector({0, 0}),
            matrix.TransformVector({size.x, 0}),
            matrix.TransformVector({size.x, size.y}),
            matrix.TransformVector({0, size.y})
        };
    }
    return {};
}

bool InventoryItemComp::Contains(Vector2 point) const
{
    if (collecting)
        return false;
    const auto corners = Corners();
    const Vector2 x{corners[1].x - corners[0].x, corners[1].y - corners[0].y};
    const Vector2 y{corners[3].x - corners[0].x, corners[3].y - corners[0].y};
    const Vector2 p{point.x - corners[0].x, point.y - corners[0].y};
    const float determinant = x.x * y.y - x.y * y.x;
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
        return false;
    const float u = (p.x * y.y - p.y * y.x) / determinant;
    const float v = (x.x * p.y - x.y * p.x) / determinant;
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
}

bool InventoryItemComp::Collect()
{
    if (!entity || collecting)
        return false;
    for (auto* object = entity; object; object = object->parent)
        if (!object->isActive())
            return false;
    auto& game = Game::get_Instance();
    if (itemId == 0 || !game.itemDB.HasItem(itemId)) {
        TraceLog(LOG_ERROR, "Inventory item object references an unavailable item: %d", itemId);
        return false;
    }
    TraceLog(LOG_INFO, "[GAME] Picked up scene item '%s' (item %d)",
             entity->name.c_str(), itemId);
    game.AddItem(itemId);
    collecting = true;
    fadeElapsed = 0.0f;
    if (auto* sprite = entity->getComponent<Sprite>()) {
        fadeStartAlpha = sprite->alpha;
    } else {
        collecting = false;
        entity->setActive(false);
    }
    return true;
}

void InventoryItemComp::Update(float dt)
{
    if (!collecting || !entity)
        return;
    auto* sprite = entity->getComponent<Sprite>();
    if (!sprite) {
        collecting = false;
        entity->setActive(false);
        return;
    }
    if (dt > 0.0f && std::isfinite(dt))
        fadeElapsed += dt;
    const float progress = std::clamp(fadeElapsed / FadeOutDuration, 0.0f, 1.0f);
    sprite->alpha = fadeStartAlpha * (1.0f - progress);
    if (progress >= 1.0f) {
        sprite->alpha = 0.0f;
        collecting = false;
        entity->setActive(false);
    }
}

ScriptComp::ScriptComp(std::string scriptAsset, ScriptPropertyMap properties)
    : scriptAsset(std::move(scriptAsset)), properties(std::move(properties))
{
}

ScriptComp::~ScriptComp()
{
    if (instanceId > 0)
        Game::get_Instance().scriptEngine.DestroyInstance(instanceId);
}

void ScriptComp::OnSceneLoaded()
{
    if (initialized || !entity || scriptAsset.empty())
        return;
    initialized = true;
    auto& engine = Game::get_Instance().scriptEngine;
    instanceId = engine.CreateInstance(scriptAsset, entity, properties, objectRefs);
    if (instanceId > 0)
        engine.CallStart(instanceId, entity);
}

bool ScriptComp::HasClickHandler() const
{
    return instanceId > 0 && Game::get_Instance().scriptEngine.HasClickHandler(instanceId);
}

bool ScriptComp::Contains(Vector2 point) const
{
    if (!entity)
        return false;
    if (const auto* hotspot = entity->getComponent<HotspotComp>())
        return hotspot->Contains(point);
    const auto* transform = entity->getComponent<TransformComp>();
    const auto matrix = transform ? transform->getWorldMatrix()
                                  : Matrix2D::Transform({0, 0}, 0, {1, 1});
    auto containsLocalRect = [&matrix, point](float minX, float minY, float width, float height) {
        if (width <= 0.0f || height <= 0.0f)
            return false;
        const Vector2 c0 = matrix.TransformVector({minX, minY});
        const Vector2 c1 = matrix.TransformVector({minX + width, minY});
        const Vector2 c3 = matrix.TransformVector({minX, minY + height});
        const Vector2 x{c1.x - c0.x, c1.y - c0.y};
        const Vector2 y{c3.x - c0.x, c3.y - c0.y};
        const Vector2 p{point.x - c0.x, point.y - c0.y};
        const float determinant = x.x * y.y - x.y * y.x;
        if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
            return false;
        const float u = (p.x * y.y - p.y * y.x) / determinant;
        const float v = (x.x * p.y - x.y * p.x) / determinant;
        return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
    };
    if (const auto* sprite = entity->getComponent<Sprite>()) {
        const auto* panel = entity->getComponent<Panel_Sprite>();
        const float width = panel ? panel->targetRect.width : sprite->src_rec.width;
        const float height = panel ? panel->targetRect.height : sprite->src_rec.height;
        return containsLocalRect(sprite->offset.x - sprite->origin.x * width,
                                 sprite->offset.y - sprite->origin.y * height,
                                 width, height);
    }
    if (const auto* rect = entity->getComponent<RectangleComp>())
        return containsLocalRect(0.0f, 0.0f, rect->width, rect->height);
    if (const auto* collider = entity->getComponent<ColliderComp>()) {
        const auto size = collider->LocalSize();
        return containsLocalRect(0.0f, 0.0f, size.x, size.y);
    }
    return false;
}

bool ScriptComp::OnClick()
{
    if (!entity || instanceId <= 0)
        return false;
    return Game::get_Instance().scriptEngine.CallClick(instanceId, entity);
}

void ScriptComp::Update(float dt)
{
    if (!entity || !entity->isActive() || instanceId <= 0)
        return;
    Game::get_Instance().scriptEngine.CallUpdate(instanceId, entity, dt);
}

bool InteractionComp::IsRunning() const
{
    return running && !running->IsFinished();
}

void InteractionComp::Trigger()
{
    for (auto* object = entity; object; object = object->parent)
        if (!object->isActive())
            return;
    if (IsRunning() || actions.empty())
        return;
    auto& game = Game::get_Instance();
    if (game.fm.fadeout)
        return;
    try {
        TraceLog(LOG_INFO, "[GAME] Action list run: '%s' (%zu actions)",
                 entity ? entity->name.c_str() : "Interaction", actions.size());
        running = BuildInteractionActions(actions, game);
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Cannot run interaction: %s", e.what());
    }
}

void InteractionComp::Update(float dt)
{
    if (running)
        running->Update(dt);
}

InventoryUIComp::InventoryUIComp(Texture2D texture, Rectangle slotSource, int slotCount,
                                 Layout layout, float spacing, Vector2 iconSize)
    : texture(texture), slotSource(slotSource), slotCount(slotCount),
      layout(layout), spacing(spacing), iconSize(iconSize)
{
}

std::array<Vector2, 4> InventoryUIComp::SlotCorners(int index) const
{
    if (!entity || index < 0 || index >= slotCount)
        return {};
    const auto* transform = entity->getComponent<TransformComp>();
    if (!transform)
        return {};
    const auto matrix = transform->getWorldMatrix();
    const float offset = static_cast<float>(index) *
        ((layout == Layout::Horizontal ? slotSource.width : slotSource.height) + spacing);
    const float minX = layout == Layout::Horizontal ? offset : 0.0f;
    const float minY = layout == Layout::Vertical ? offset : 0.0f;
    return {
        matrix.TransformVector({minX, minY}),
        matrix.TransformVector({minX + slotSource.width, minY}),
        matrix.TransformVector({minX + slotSource.width, minY + slotSource.height}),
        matrix.TransformVector({minX, minY + slotSource.height})
    };
}

std::optional<int> InventoryUIComp::SlotAt(Vector2 point) const
{
    if (!entity || !entity->getComponent<TransformComp>())
        return std::nullopt;
    for (int index = 0; index < slotCount; ++index) {
        const auto corners = SlotCorners(index);
        const Vector2 x{corners[1].x - corners[0].x, corners[1].y - corners[0].y};
        const Vector2 y{corners[3].x - corners[0].x, corners[3].y - corners[0].y};
        const Vector2 p{point.x - corners[0].x, point.y - corners[0].y};
        const float determinant = x.x * y.y - x.y * y.x;
        if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
            continue;
        const float u = (p.x * y.y - p.y * y.x) / determinant;
        const float v = (x.x * p.y - x.y * p.x) / determinant;
        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
            return index;
    }
    return std::nullopt;
}

bool InventoryUIComp::Contains(Vector2 point) const
{
    return SlotAt(point).has_value();
}

bool InventoryUIComp::ClickAt(Vector2 point)
{
    const auto slot = SlotAt(point);
    if (!slot)
        return false;
    auto& game = Game::get_Instance();
    if (*slot < static_cast<int>(game.runtime_inv.size()) && game.runtime_inv[*slot].id < 0) {
        const int clickedId = game.runtime_inv[*slot].id;
        if (Global::selectedItemId == clickedId)
            game.SelectItem(0);
        else
            game.SelectItem(clickedId);
    } else {
        game.SelectItem(0);
    }
    return true;
}

void InventoryUIComp::Draw()
{
    auto* transform = entity->getComponent<TransformComp>();
    if (!transform)
        return;
    const auto matrix = transform->getWorldMatrix();
    const auto scale = transform->getWorldScale();
    const auto& inventory = Game::get_Instance().runtime_inv;
    const float opacity = entity->projectUIOpacity.value_or(1.0f);
    const auto hoveredSlot = (Global::mousePos.x >= 0.0f && Global::mousePos.y >= 0.0f)
        ? SlotAt(Global::mousePos) : std::nullopt;
    for (int index = 0; index < slotCount; ++index) {
        const float offset = static_cast<float>(index) *
            ((layout == Layout::Horizontal ? slotSource.width : slotSource.height) + spacing);
        const auto position = matrix.TransformVector(
            {layout == Layout::Horizontal ? offset : 0,
             layout == Layout::Vertical ? offset : 0});
        const Rectangle destination{
            position.x, position.y, slotSource.width * scale.x, slotSource.height * scale.y
        };
        DrawTexturePro(texture, slotSource, destination, {0, 0},
                       transform->getWorldRotation(), Fade(WHITE, opacity));
        const bool hasSlotItem = index < static_cast<int>(inventory.size()) && inventory[index].id < 0;
        if (hasSlotItem) {
            const auto& item = inventory[index];
            const auto iconCenter = matrix.TransformVector(
                {(layout == Layout::Horizontal ? offset : 0) + slotSource.width * 0.5f,
                 (layout == Layout::Vertical ? offset : 0) + slotSource.height * 0.5f});
            const float itemWidth = iconSize.x * scale.x;
            const float itemHeight = iconSize.y * scale.y;
            const float itemOpacity = opacity * Game::get_Instance().InventoryItemAlpha(item.id);
            DrawTexturePro(ResourcesLoader::inv_items, item.rect,
                           {iconCenter.x, iconCenter.y, itemWidth, itemHeight},
                           {itemWidth * 0.5f, itemHeight * 0.5f},
                           transform->getWorldRotation(), Fade(WHITE, itemOpacity));
            const bool isSelected = item.id == Global::selectedItemId;
            const bool isHovered = hoveredSlot && *hoveredSlot == index;
            if (isSelected || isHovered) {
                const auto corners = SlotCorners(index);
                const Color borderColor = isSelected
                    ? Fade(YELLOW, opacity)
                    : Fade(WHITE, opacity * 0.6f);
                const float thickness = isSelected ? 2.0f : 1.0f;
                for (std::size_t c = 0; c < corners.size(); ++c)
                    DrawLineEx(corners[c], corners[(c + 1) % corners.size()],
                               thickness, borderColor);
            }
        }
    }
}

namespace
{
Rectangle CraftingSlotRect(int index)
{
    return index < 3 ? Rectangle{20.0f + index * 40.0f, 40.0f, 30.0f, 30.0f}
                     : Rectangle{60.0f, 100.0f, 30.0f, 30.0f};
}
}

std::optional<int> CraftingUIComp::SlotAt(Vector2 point) const
{
    const auto* transform = entity->getComponent<TransformComp>();
    if (!transform)
        return std::nullopt;
    const auto matrix = transform->getWorldMatrix();
    const float determinant = matrix.m[0][0] * matrix.m[1][1] - matrix.m[0][1] * matrix.m[1][0];
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
        return std::nullopt;
    const float x = point.x - matrix.m[0][2], y = point.y - matrix.m[1][2];
    const Vector2 local{(matrix.m[1][1] * x - matrix.m[0][1] * y) / determinant,
                        (-matrix.m[1][0] * x + matrix.m[0][0] * y) / determinant};
    for (int i = 0; i < 4; ++i)
        if (CheckCollisionPointRec(local, CraftingSlotRect(i)))
            return i;
    return std::nullopt;
}

void CraftingUIComp::Update(float)
{
    auto& game = Game::get_Instance();
    for (auto& id : ingredients)
        if (id < 0 && !game.hasItem(id))
            id = 0;
}

bool CraftingUIComp::ClickAt(Vector2 point)
{
    const auto slot = SlotAt(point);
    if (!slot)
        return false;
    auto& game = Game::get_Instance();
    Update(0);
    if (*slot < 3) {
        const int selected = Global::selectedItemId;
        if (selected < 0 && game.hasItem(selected)) {
            if (std::find(ingredients.begin(), ingredients.end(), selected) == ingredients.end())
                ingredients[*slot] = selected;
            else
                TraceLog(LOG_WARNING, "An inventory item can only occupy one crafting slot.");
            game.SelectItem(0);
        } else {
            ingredients[*slot] = 0;
        }
    } else {
        const int result = game.craftingRecipes.Match(ingredients);
        if (result < 0 && game.itemDB.HasItem(result)) {
            const bool resultIsIngredient =
                std::find(ingredients.begin(), ingredients.end(), result) != ingredients.end();
            if (game.hasItem(result) && !resultIsIngredient) {
                TraceLog(LOG_WARNING, "Cannot craft an item already in inventory.");
                return true;
            }
            for (int id : ingredients)
                if (id < 0)
                    game.removeItem(id);
            ingredients.fill(0);
            game.AddItem(result);
        }
    }
    return true;
}

void CraftingUIComp::Draw()
{
    const auto* transform = entity->getComponent<TransformComp>();
    if (!transform)
        return;
    const auto matrix = transform->getWorldMatrix();
    const auto scale = transform->getWorldScale();
    const float opacity = entity->projectUIOpacity.value_or(1.0f);
    auto& game = Game::get_Instance();
    auto visibleIngredients = ingredients;
    for (auto& id : visibleIngredients)
        if (id < 0 && !Game::get_Instance().hasItem(id))
            id = 0;
    const int result = game.craftingRecipes.Match(visibleIngredients);
    for (int i = 0; i < 4; ++i) {
        const auto rect = CraftingSlotRect(i);
        const auto position = matrix.TransformVector({rect.x, rect.y});
        DrawTexturePro(ResourcesLoader::ui_page, {212, 0, 30, 30},
                       {position.x, position.y, rect.width * scale.x, rect.height * scale.y},
                       {0, 0}, transform->getWorldRotation(), Fade(WHITE, opacity));
        const int id = i < 3 ? visibleIngredients[i] : result;
        const auto item = game.itemDB.getItem(id);
        if (id < 0 && item.id < 0) {
            const auto center = matrix.TransformVector({rect.x + 15.0f, rect.y + 15.0f});
            const float width = 24.0f * scale.x, height = 24.0f * scale.y;
            DrawTexturePro(ResourcesLoader::inv_items, item.rect,
                           {center.x, center.y, width, height}, {width * 0.5f, height * 0.5f},
                           transform->getWorldRotation(), Fade(WHITE, opacity));
        }
    }
}

#pragma region TEXTCOMP
TextComp::TextComp(const char *text, int size, Color color)
{
    _margin = {0};
    _text = text;
    _size = size;
    _color = color;
}
TextComp::TextComp(const char *text, int size, Vector2 margin, Color color,
                   std::string fontName)
{
    _margin = margin;
    _text = text;
    _size = size;
    _color = color;
    _fontName = std::move(fontName);
}
void TextComp::setOwner(Entity *e)
{
    entity = e;

    transform = entity->getComponent<TransformComp>();
}

void TextComp::Update(float dt)
{

}

void TextComp::Draw()
{
    if(transform)
    {
        const Font* custom = _fontName.empty() ? nullptr : ResourcesLoader::GetCustomFont(_fontName);
        Color color = _color;
        if (entity->projectUIOpacity)
            color.a = static_cast<unsigned char>(std::round(color.a * *entity->projectUIOpacity));
        if (entity->projectUIOpacity) {
            const auto position = transform->getWorldPosition() + _margin;
            const auto scale = transform->getWorldScale();
            rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlRotatef(transform->getWorldRotation(), 0, 0, 1);
            rlScalef(scale.x, scale.y, 1);
            DrawTextPro(custom ? *custom : GetFontDefault(), _text.c_str(), {0, 0}, {0, 0},
                        0, _size, 1.0f, color);
            rlPopMatrix();
        } else
            DrawTextPro(custom ? *custom : GetFontDefault(),_text.c_str(),transform->getWorldPosition() + _margin, {0},
                        transform->getWorldRotation(),_size,1.0f, color);
    }
}

Vector2 TextComp::Measure() const
{
    const Font* custom = _fontName.empty() ? nullptr : ResourcesLoader::GetCustomFont(_fontName);
    return MeasureTextEx(custom ? *custom : GetFontDefault(), _text.c_str(),
                         static_cast<float>(_size), 1.0f);
}

#pragma endregion