#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <vector>
#include <memory>
#include "raylib.h"

#include "Entity.h"
#include "Screen.h"
#include "Game.hpp"
#include "Events.h"

class Crafting_main;
class InventoryManager;
class DrawTextComp;

class UI_Manager
{
    public:
    Camera2D ui_cam;
    std::vector<std::unique_ptr<Entity>> ui;
    
    Entity* panel_ui;
    InventoryManager* inv_man;
    Crafting_main *craft_man;
    

//     std::unique_ptr<Entity> inv_ui;
//     std::unique_ptr<Entity> bag;
//    // std::unique_ptr<Entity> inv;
//     std::unique_ptr<Entity> craft_btn, craft_icon;
//     std::unique_ptr<Entity> lens_btn, lens_icon;
//     std::unique_ptr<Entity> inventory;

private:
    DrawTextComp* sub_text;
public:

    Entity* main_inv_ui;
    Entity* subtitle;
    Entity* inventory;


    
    UI_Manager();
    void ShowUI(Entity* e, float fadeTime);
    void ShowInvUI();
    ~UI_Manager()
    {

    }

    void UPdateUI();
    void ShowUI(std::string name);

    void ShowSubtitle(std::string sub, float t = 0);
    void Update(float dt);

    void Draw();

    Entity* getUI(std::string name);
 
    void addUI(std::unique_ptr<Entity> ui);


};




#endif

