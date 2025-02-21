//
//  Entitiy.h
//  raygame2
//
//  Created by apple on 06/02/2025.
//

#ifndef Entitiy_h
#define Entitiy_h
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <iostream>
#include "Global.h"
#include <string>


class Entity;
class Component
{
protected:
    Entity* entity;
public:
    virtual ~Component() = default;
    
    virtual void Update(float dt) = 0;
    
    virtual void Draw()= 0;
    
    virtual void setOwner(Entity* e){entity = e;}

    virtual void OnChangePerent(){}
};

class Entity
{
private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;
public:
    std::string name;
    
    std::vector<std::unique_ptr<Entity>> childs;

    Entity* parent = nullptr;


    Entity()
    {
        name = "new Entity";
    }
    
    Entity(const std::string s)
    {
        name = s;
    }
    
    template <typename T, typename ...Args>
    T* addComponent(Args&&... args)
    {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->setOwner(this);
        T* ptr = component.get();
        components[typeid(T)] = std::move(component);
        return ptr;
        
    }
    
    template <typename T>
    T* getComponent()
    {
        auto it = components.find(typeid(T));
        if(it != components.end())
        {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    void addChild(std::unique_ptr<Entity> child)
    {
        
        child.get()->parent = this;
        child.get()->Component_update();
        childs.push_back(std::move(child));
    }

    Entity* getChild(int idx)
    {
        if(childs.size() < 1)return nullptr;

        return childs[idx].get();
    }

    Entity* getChild(const std::string name)
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

    
    virtual void Update(float dt)
    {
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
    }
    
    virtual void Draw()
    {
        for(auto& [type, component]:components)
        {
            component->Draw();
        }
        
        
    }
    
    virtual void OnClicked()
    {
        
    }


    void Component_update()
    {
        for(auto& [type, component]:components)
        {
            component->OnChangePerent();
        }
    }
};

class UI_Entity:public Entity
{
private:

    bool mouse_over = false;
public:
    Rectangle col_rect;
    
    UI_Entity()
    {
        
    }
    
    UI_Entity(std::string name)
    {
        this->name = name;
    }

    
    void OnClicked() override
    {
        
    }
    

};

#endif /* Entitiy_h */
