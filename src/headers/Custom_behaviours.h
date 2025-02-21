#include "Component.h"

// continuety of components
// file split

class test_script:public Behaviour
{
    TransformComp* t;
    void Init()override
    {
        t = entity->getComponent<TransformComp>();
    }

    void Update(float dt)override
    {
        if(t)
        {
            t->position.x += dt * 2.0f;
           // std::cout << "parent" <<t->position.x << std::endl;
           t->rotation += dt * 5.0f;
            
        }
    }


};

class child_script:public Behaviour
{
    TransformComp* t;
    void Init()override
    {
        t = entity->getComponent<TransformComp>();
        
    }

    void Update(float dt)override
    {
       // std::cout<<" child" << t->position.y << std::endl;
        t->position.y += dt *2.0f;
    }


};