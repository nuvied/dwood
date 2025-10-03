
#ifndef INVITEM_ENTITY_H
#define INVITEM_ENTITY_H

#include "Game.hpp"
#include "Entity.h"
#include "Component.h"


class InvItem_Entity:public Entity
{
    public:
    
    InvItem_Entity() = default;


    void Init(const std::string name, Texture2D tex, Rectangle rect = {0})
    {
        this->name = name;

        addComponent<TransformComp>();
        addComponent<Sprite>(Sprite(tex, rect));
        addComponent<ColliderComp>();
    }
};


#endif
