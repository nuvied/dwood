#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <vector>
#include <memory>
#include "raylib.h"

#include "Entity.h"
#include "Screen.h"

class UI_Manager
{
    public:
    Camera2D ui_cam;

    

//     std::unique_ptr<Entity> inv_ui;
//     std::unique_ptr<Entity> bag;
//    // std::unique_ptr<Entity> inv;
//     std::unique_ptr<Entity> craft_btn, craft_icon;
//     std::unique_ptr<Entity> lens_btn, lens_icon;
//     std::unique_ptr<Entity> inventory;

private:
std::vector<std::unique_ptr<Entity>> ui;
    public:

    Entity* main_inv_ui;

    UI_Manager(){
        ui_cam = {0};
        ui_cam.target  = {0,0};
        ui_cam.offset = {0,0};
        ui_cam.zoom = 2.0f;// global zoom for pixel perfect


        auto inv_ui = std::make_unique<Entity>("ui_parent");
        inv_ui->addComponent<TransformComp>(0,0);

        auto inventory = std::make_unique<Entity>("Inv_slots");
        inventory->addComponent<TransformComp>(0,0);

        auto bag = std::make_unique<Entity>("bag");
        

        bag->addComponent<TransformComp>(TransformComp(5,227));
        bag->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {0,32,42,29}));
        bag->addComponent<ColliderComp>(ColliderComp(0,0,0,0));
        auto on_inv = bag->addComponent<OnInvButton>();
        on_inv->inventory = inventory.get();

        for(int i = 0; i < 10; i++){
            auto inv = std::make_unique<Entity>("Inv");
            inv->addComponent<TransformComp>(TransformComp(62 + i* 32,227));
            inv->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {182,0,30,30}));
            inv->addComponent<ColliderComp>(0,0,0,0);
            auto slot = inv->addComponent<Slot_script>();
            slot->idx = i;
            inventory->addChild(std::move(inv));
        }

        
        
        auto craft_btn = std::make_unique<Entity>("Craft_btn");
        craft_btn->addComponent<TransformComp>(TransformComp(396,224));
        craft_btn->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {126,0,56,32}));
        craft_btn->addComponent<ColliderComp>(ColliderComp(0,0,0,0));

        auto craft_icon = std::make_unique<Entity>("craft_icon");
        craft_icon->addComponent<TransformComp>(TransformComp(15,2));
        auto spc = Sprite(ResourcesLoader::ui_page, {242,0,28,27});
        craft_icon->addComponent<Sprite>(spc);
        craft_btn->addChild(std::move(craft_icon));

        auto lens_btn = std::make_unique<Entity>("Lens_btn");
        lens_btn->addComponent<TransformComp>(TransformComp(455,224));
        lens_btn->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {126,0,56,32}));
        lens_btn->addComponent<ColliderComp>(ColliderComp(0,0,0,0));

        auto lens_icon = std::make_unique<Entity>("lens_icon");
        lens_icon->addComponent<TransformComp>(TransformComp(5,0));
        lens_icon->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {64,22,46,30}));

        lens_btn->addChild(std::move(lens_icon));
        
        inv_ui->addChild(std::move(inventory));
        inv_ui->addChild(std::move(bag));
        inv_ui->addChild(std::move(craft_btn));
        inv_ui->addChild(std::move(lens_btn));
       // ui.push_back(std::move(lens_btn));
        //ui.push_back(std::move(craft_btn));
        //inv_ui->setActive(false);
        

        ui.push_back(std::move(inv_ui));

        //std::cout << ui[0].get()->name <<std::endl;
        main_inv_ui = getUI("ui_parent");
        main_inv_ui->setActive(false);
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

    Entity* getUI(std::string name)
    {
        for(auto& u:ui)
        {
            if(u->name == name)
            {
                return u.get();
            }
        }
        return nullptr;
    }
    Entity* getUI(int idx)
    {
        return nullptr;
    }

};












#endif

