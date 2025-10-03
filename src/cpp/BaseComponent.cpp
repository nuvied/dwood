#include "BaseComponent.h"

Entity *Component::getEntity()
{
    if(entity)return entity;
    return nullptr;
}