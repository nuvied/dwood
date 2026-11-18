
#include "Component.h"
#include "UI_Manager.h"

#include <algorithm>
#include <cmath>
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

        Vector2 worldPos = trans->getWorldPosition();
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
        DrawTextureRec(texture,src_rec,{0,0}, Fade(color, entity->projectUIOpacity ? a : color.a));
    }
    
    // DrawTexture(ResourcesLoader::ui_page, 0,0,WHITE);
}

#pragma endregion

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
    }
    void Update(float) override {}
    bool IsFinished() override { return true; }
private:
    std::string id;
    bool visible;
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
        else if (action.type == Type::ToggleUI)
            ui.SetVisible(action.target, !ui.IsVisible(action.target));
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
        auto next = std::make_unique<ActionList>();
        for (const auto& action : actions) {
            if (action.type == ActionData::Type::AddItem) {
                if (!game.itemDB.HasItem(action.itemId))
                    throw std::runtime_error("Interaction target inventory item is unavailable: " +
                                             std::to_string(action.itemId));
                next->AddAction(std::make_unique<AddItemAction>(action.itemId));
            } else if (action.type == ActionData::Type::RemoveItem) {
                if (!game.itemDB.HasItem(action.itemId))
                    throw std::runtime_error("Interaction target inventory item is unavailable: " +
                                             std::to_string(action.itemId));
                next->AddAction(std::make_unique<RemoveItemAction>(action.itemId));
            } else if (action.type == ActionData::Type::ChangeScene) {
                if (!game.scene_manager)
                    throw std::runtime_error("Scene manager is unavailable.");
                const auto registered = game.scene_manager->RegisteredScenes();
                const auto path = game.scene_manager->Directory() / action.target;
                if (std::find(registered.begin(), registered.end(), path) == registered.end())
                    throw std::runtime_error("Interaction target scene is not registered: " + action.target);
                next->AddAction(std::make_unique<ChangeSceneAction>(path));
            } else {
                if (!game.ui_m || !game.ui_m->projectUI ||
                    !game.ui_m->projectUI->HasElement(action.target))
                    throw std::runtime_error("Interaction target project UI element is unavailable: " + action.target);
                if (action.type == ActionData::Type::ShowUI || action.type == ActionData::Type::HideUI)
                    next->AddAction(std::make_unique<SetUIVisibilityAction>(
                        action.target, action.type == ActionData::Type::ShowUI));
                else {
                    if ((action.type == ActionData::Type::FadeInUI ||
                         action.type == ActionData::Type::FadeOutUI ||
                         action.type == ActionData::Type::ToggleFadeUI) &&
                        !game.ui_m->projectUI->IsGroup(action.target))
                        throw std::runtime_error("UI fade target is not a group: " + action.target);
                    next->AddAction(std::make_unique<ProjectUIAction>(action));
                }
            }
        }
        running = std::move(next);
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

void InventoryUIComp::Draw()
{
    auto* transform = entity->getComponent<TransformComp>();
    if (!transform)
        return;
    const auto matrix = transform->getWorldMatrix();
    const auto scale = transform->getWorldScale();
    const auto& inventory = Game::get_Instance().runtime_inv;
    const float opacity = entity->projectUIOpacity.value_or(1.0f);
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
        if (index >= static_cast<int>(inventory.size()) || inventory[index].id >= 0)
            continue;
        const auto& item = inventory[index];
        const auto iconCenter = matrix.TransformVector(
            {(layout == Layout::Horizontal ? offset : 0) + slotSource.width * 0.5f,
             (layout == Layout::Vertical ? offset : 0) + slotSource.height * 0.5f});
        const float itemWidth = iconSize.x * scale.x;
        const float itemHeight = iconSize.y * scale.y;
        DrawTexturePro(ResourcesLoader::inv_items, item.rect,
                       {iconCenter.x, iconCenter.y, itemWidth, itemHeight},
                       {itemWidth * 0.5f, itemHeight * 0.5f},
                       transform->getWorldRotation(), Fade(WHITE, opacity));
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