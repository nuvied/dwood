#include "HouseFrontScreen.h"
#include "BaseComponent.h"
#include "Component.h"



HouseFrontScreen::HouseFrontScreen()
{
    Init();
}

HouseFrontScreen::~HouseFrontScreen()
{
}

void HouseFrontScreen::Init()
{
    auto bg = std::make_unique<Entity>("bg");
    bg->addComponent<TransformComp >(0,0);
    bg->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {0,256,512,256}));


    addEntity(std::move(bg));
    


    Light light1 = Light(1.3f,.8f, {166,384});
    Light light2 = Light(1.3f,.8f, {405,350});
    Light light_hanging = Light(1.2f, .5f, {133,194});
    Light light_win = Light(1.0f, .3f, {305,113});
    Light light_moon = Light(1.2f, 1.5f, {1000,20});
    Color c = {255,200,40};
    light1.lightColor = c;
    light2.lightColor = c;
    light_hanging.lightColor = c;
    lights.push_back(light1);
    lights.push_back(light2);
    lights.push_back(light_hanging);
    lights.push_back(light_win);
    lights.push_back(light_moon);


    Screen::Init();
    
}


