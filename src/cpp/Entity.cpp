#include "Entity.h"

#include "Component.h"

void Entity::setActive(bool value)
{
    if(value) onEnable();
    else onDisable();
    active = value;    
}

void Entity::onEnable()
{
    for(auto& [type, component]:components)
    {
        component->onEnable();
    }
}

void Entity::onDisable()
{
    for(auto& [type, component]:components)
    {
        component->onDisable();
    }
}

bool Entity::isActive()
{
    return active;
}

bool Entity::IsUnderMouse()
{
    if(!active)return false;
    
    for(int i = childs.size() - 1; i >= 0; --i)
    {
        if(childs[i]->IsUnderMouse()) return true;
    }

    auto comp = getComponent<ColliderComp>();
    if(!comp)return false;
    
    
    if(CheckCollisionPointRec(Global::mousePos ,comp->col_rect))
    { 
        if(Global::debug){
        std::cout << "click on : "<< comp->getEntity()->name <<std::endl;
        }
        return true;
    }

    return false;
}

Entity::Entity()
{
    name = "new Entity";
}

Entity::Entity(const std::string s)
{
    name = s;
}



// sceen init


void Entity::addChild(std::unique_ptr<Entity> child)
{
    
    child.get()->parent = this;
    child.get()->Component_update();
    childs.push_back(std::move(child));
}

Entity* Entity::getChild(int idx)
{
    if(childs.size() < 1)return nullptr;

    return childs[idx].get();
}

Entity* Entity::getChild(const std::string name)
{
    for(auto& c: childs)
    {
        if(c->name == name)
        {
            return c.get();
        }
    }
    return nullptr;
}


void Entity::Update(float dt)
{
    if(!active)return;

    UpdateSelf(dt);


    if(childs.size() < 1)return;
    for(int i = 0; i < childs.size(); i++)
    {
        if(childs[i].get()->parent == nullptr)
            childs.erase(childs.begin()+i);
    }

    for(auto& c: childs)
    {
        c->Update(dt);
    }
}

void Entity::UpdateSelf(float dt)
{
    if (!active) return;
    for (auto& [type, component] : components)
        component->Update(dt);
}

void Entity::DrawSelf()
{
    if (!active) return;
    for (auto* component : drawList)
        component->Draw();
}

bool Entity::IsUiLayer() const
{
    return uiLayer || (parent && parent->IsUiLayer());
}

void Entity::DrawLayer(bool ui)
{
    if (!active)
        return;
    if (IsUiLayer() == ui) {
        for (auto* component : drawList)
            component->Draw();
    }
    for (auto& child : childs)
        child->DrawLayer(ui);
}

void Entity::DrawLayer(const std::vector<std::unique_ptr<Entity>>& roots, bool ui,
                       bool includeLensOnly)
{
    std::vector<Entity*> objects;
    const auto append = [&objects](auto&& self, Entity& entity) -> void {
        if (!entity.isActive())
            return;
        objects.push_back(&entity);
        for (auto& child : entity.childs)
            self(self, *child);
    };
    for (const auto& root : roots)
        append(append, *root);

    const auto renderOrder = [](Entity* entity) {
        const auto* sprite = entity->getComponent<Sprite>();
        if (sprite)
            return sprite->drawOrder;
        const auto* rectangle = entity->getComponent<RectangleComp>();
        return rectangle ? rectangle->drawOrder : 0;
    };
    std::stable_sort(objects.begin(), objects.end(),
                     [&renderOrder](Entity* a, Entity* b) {
                         return renderOrder(a) < renderOrder(b);
                     });
    for (auto* object : objects) {
        if (object->IsUiLayer() != ui)
            continue;
        if (!includeLensOnly) {
            if (const auto* sprite = object->getComponent<Sprite>(); sprite && sprite->lensOnly)
                continue;
        }
        object->DrawSelf();
    }
}

void Entity::DrawLensOnly(const std::vector<std::unique_ptr<Entity>>& roots)
{
    std::vector<Entity*> objects;
    const auto append = [&objects](auto&& self, Entity& entity) -> void {
        if (!entity.isActive())
            return;
        objects.push_back(&entity);
        for (auto& child : entity.childs)
            self(self, *child);
    };
    for (const auto& root : roots)
        append(append, *root);

    const auto renderOrder = [](Entity* entity) {
        const auto* sprite = entity->getComponent<Sprite>();
        return sprite ? sprite->drawOrder : 0;
    };
    std::stable_sort(objects.begin(), objects.end(),
                     [&renderOrder](Entity* a, Entity* b) {
                         return renderOrder(a) < renderOrder(b);
                     });
    for (auto* object : objects) {
        if (object->IsUiLayer())
            continue;
        if (const auto* sprite = object->getComponent<Sprite>(); sprite && sprite->lensOnly)
            object->DrawSelf();
    }
}

    void Entity::Draw()
{
    if(!active)return;

//    for(auto& [type, component]:components)
//    {
//        component->Draw();
//    }
    DrawSelf();
    //drawing childs

    
    for(auto& c: childs)
    {
        c->Draw();
    }
    
}

void Entity::OnClicked()
{
    
}

void Entity::Component_update()
{
    for(auto& [type, component]:components)
    {
        component->OnChangePerent();
    }
}


/// @brief hotspot class entity 
/// @param name "HotSpot"
/// @param bound 

Hotspot_entity::Hotspot_entity(std::string name,Rectangle bound)
{
    this->name = name;
    this->addComponent<ColliderComp>(bound);
}

void Hotspot_entity::SetCallback(std::function<void()> callback)
{
    _callback = callback;
}

void Hotspot_entity::OnClicked()
{
    if(_callback)
    {
        _callback();
    }
}
