#include "Screen.h"
#include "Component.h"

Screen::Screen()
{
    
    alpha = 1;
    cam = {0};
    cam.target  = {0,0};
    cam.offset = {0,0};
    cam.zoom = 2.0f;
    
    ui_cam.zoom = 2.0f;
    
}

void Screen::SceneLoaded()
{
    for(auto& e:entities)
    {
        auto b = e->getComponent<Behaviour>();
        if(b)
        {
            b->SceneLoaded();
        }

        if(e->childs.size() < 1)return;

        for(auto& child:e->childs)
        {
            auto c = e->getComponent<Behaviour>();
            if(c)
            {
                c->SceneLoaded();
            }
        }
    }
}

void Screen::Init()
{
    SceneLoaded();
}
