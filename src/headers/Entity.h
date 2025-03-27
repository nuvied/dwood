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
#include <algorithm>



class Entity
{
private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;
    std::vector<Component*> drawList;
    bool active = true;
public:
    std::string name;
    
    std::vector<std::unique_ptr<Entity>> childs;

    Entity* parent = nullptr;

    void setActive(bool value);

    virtual void onEnable();
    virtual void onDisable();

    bool isActive();

    float alpha = 1;

    bool IsUnderMouse();

    Entity();
    
    Entity(const std::string s);

    Entity(const std::string name, Texture2D tex, Rectangle rect);
    
    template <typename T, typename ...Args>
    T* addComponent(Args&&... args)
    {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->setOwner(this);
        T* ptr = component.get();
        components[typeid(T)] = std::move(component);
        drawList.push_back(ptr);
        sortDrawList();
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

    void sortDrawList()
    {
        std::sort(drawList.begin(), drawList.end(),
                  [](Component* a, Component* b)
                  {
                      return a->drawOrder < b->drawOrder;
                  });
    }

    void setDrawOrder(Component* comp, int order)
    {
        comp->drawOrder = order;
        sortDrawList();
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



class Hotspot_entity:public Entity
{
private:
    std::function<void ()> _callback = 0;
public:
    Hotspot_entity(std::string name, Rectangle bound);
    void SetCallback(std::function<void()> callback);

    void OnClicked()override;
};



#endif /* Entitiy_h */
