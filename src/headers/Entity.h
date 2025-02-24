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
#include <vector>
#include "BaseComponent.h"



class Entity
{
private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;

    bool active = true;
public:
    std::string name;
    
    std::vector<std::unique_ptr<Entity>> childs;

    Entity* parent = nullptr;

    void setActive(bool value);

    bool isActive();

    Entity();
    
    Entity(const std::string s);
    
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
// sceen init


    void addChild(std::unique_ptr<Entity> child);

    Entity* getChild(int idx);

    Entity* getChild(const std::string name);

    
    virtual void Update(float dt);
    
    virtual void Draw();
    
    virtual void OnClicked();


    void Component_update();

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
