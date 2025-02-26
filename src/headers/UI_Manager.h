#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <vector>
#include <memory>
#include "raylib.h"

#include "Entity.h"
#include "Screen.h"
#include "Game.hpp"
#include "Custom_behaviours.h"


class UI_Manager
{
    public:
    Camera2D ui_cam;
    std::vector<std::unique_ptr<Entity>> ui;
    

    InventoryManager* inv_man;

//     std::unique_ptr<Entity> inv_ui;
//     std::unique_ptr<Entity> bag;
//    // std::unique_ptr<Entity> inv;
//     std::unique_ptr<Entity> craft_btn, craft_icon;
//     std::unique_ptr<Entity> lens_btn, lens_icon;
//     std::unique_ptr<Entity> inventory;

private:

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
        inv_man = inventory->addComponent<InventoryManager>();

        for(int i = 0; i < 10; i++){
            auto inv = std::make_unique<Entity>("Inv");
            inv->addComponent<TransformComp>(TransformComp(62 + i* 32,227));
            inv->addComponent<Sprite>(Sprite(ResourcesLoader::ui_page, {182,0,30,30}));
            inv->addComponent<ColliderComp>(0,0,0,0);

            auto slot = inv->addComponent<Slot_script>();
            slot->idx = i;
            

            auto slot_icon = std::make_unique<Entity>("slot_icon" + i);
            auto slot_icon_t = slot_icon->addComponent<TransformComp>();
            slot_icon->addComponent<Sprite>(Sprite(ResourcesLoader::inv_items, {0,0,24,24}));
            slot_icon_t->position = {3,3};
            slot_icon->setActive(false);


            inv->addChild(std::move(slot_icon));
            inventory->addChild(std::move(inv));
        }
        
        inv_man->ChildInitialized(); // call a function 

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
        
        //inv_ui->setActive(true);
        ui.push_back(std::move(inv_ui));

        //std::cout << ui[0].get()->name <<std::endl;
        main_inv_ui = getUI("ui_parent");
        main_inv_ui->setActive(false);

        inv_man->ChildInitialized();
    }
    ~UI_Manager()
    {

    }

    void UPdateUI()
    {

        for (int i = 0; i < Game::get_Instance().runtime_inv.size(); i++)
        {
            /* code */
            if(inv_man->slots.size() < i)break;

            if(inv_man->slots[i]){
                auto s = inv_man->slots[i]->getComponent<Slot_script>();
               std::cout << "inv manager slots"  << s->slot_item.rect.width << std::endl;
                s->slot_item = Game::get_Instance().runtime_inv[i];
                s->UpdateSlot();
                std::cout << s->slot_item.name << std::endl;
            }


        }
        
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

