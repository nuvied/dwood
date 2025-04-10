
#ifndef BASE_COMP_H
#define BASE_COMP_H

class Entity;
class Component
{
protected:
    Entity* entity;
public:
    virtual ~Component() = default;

    int drawOrder = 0;

    virtual void Update(float dt) = 0;
    
    virtual void Draw()= 0;
    
    virtual void setOwner(Entity* e){entity = e;}

    virtual void OnChangePerent(){}

    virtual void onEnable(){};
    virtual void onDisable(){};
    virtual Entity *getEntity();
};

#endif