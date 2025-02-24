#include "Screen.h"


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
    // for(auto& e:entities)
    // {
    //     auto b = e->getComponent<Behaviour>();
    //     if(b)
    //     {
    //         b->SceneLoaded();
    //     }
    // }
}

void Screen::Init()
{
    SceneLoaded();
}
