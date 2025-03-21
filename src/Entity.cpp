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

    for(auto& [type, component]:components)
    {
        component->Draw();
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
