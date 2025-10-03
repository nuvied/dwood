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

    for(auto& [type, component]:components)
    {
        component->Update(dt);
    }


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

    void Entity::Draw()
{
    if(!active)return;

//    for(auto& [type, component]:components)
//    {
//        component->Draw();
//    }
    for(auto* e:drawList)
    {
        e->Draw();
    }
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
