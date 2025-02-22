#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <vector>
#include <memory>
#include "raylib.h"


#include "Screen.h"

class UI_Manager
{
    public:
    Camera2D ui_cam;

    std::vector<std::unique_ptr<Entity>> ui;

    std::unique_ptr<Entity> bag;
    std::unique_ptr<Entity> inv;
    std::unique_ptr<Entity> craft_btn, craft_icon;

    public:
    UI_Manager(){
        ui_cam = {0};
        ui_cam.target  = {0,0};
        ui_cam.offset = {0,0};
        ui_cam.zoom = 2.0f;// global zoom for pixel perfect



        bag = std::make_unique<Entity>("bag");
        

        bag->addComponent<TransformComp>(TransformComp(0,227));
        bag->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {0,32,42,29}));
        bag->addComponent<ColliderComp>(ColliderComp(0,0,0,0));

        for(int i = 0; i < 10; i++){
            inv = std::make_unique<Entity>("Inv");
            inv->addComponent<TransformComp>(TransformComp(50 + i* 32,0));
            inv->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {182,0,30,30}));
            inv->addComponent<ColliderComp>(0,0,0,0);
            bag->addChild(std::move(inv));
        }

        craft_btn = std::make_unique<Entity>("Craft_btn");
        craft_btn->addComponent<TransformComp>(TransformComp(368,224));
        craft_btn->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {126,0,56,32}));
        craft_btn->addComponent<ColliderComp>(ColliderComp(0,0,0,0));

        craft_icon = std::make_unique<Entity>("craft_icon");
        craft_icon->addComponent<TransformComp>(TransformComp(15,2));
        craft_icon->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {242,0,28,27}));
        craft_btn->addChild(std::move(craft_icon));
       
        ui.push_back(std::move(craft_btn));
        ui.push_back(std::move(bag));

    }
    ~UI_Manager()
    {

    }
    void Update(float dt)
    {
        for(auto& e:ui)
        {
            e->Update(dt);
        }
    }

    void Draw()
    {
        BeginMode2D(ui_cam);
        for(auto& e:ui)
        {
            e->Draw();
        }
        EndMode2D();
    }

    Entity* getUi(std::string name)
    {
        return nullptr;
    }
    Entity* getUI(int idx)
    {
        return nullptr;
    }

};












#endif

