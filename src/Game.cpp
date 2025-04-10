//
//  Game.cpp
//  my_first_raygame
//
//  Created by apple on 01/02/2025.
//

#include "Game.hpp"
#include "SceneStack.h"
#include "BoatScreen.h"
#include "MainScreen.h"
//#include "Fade_manager.hpp"
#include "UI_manager.h"
#include "LensManager.h"

Game::Game()
{
    
}

Game &Game::get_Instance()
{
    static Game instance;
    return instance;
}

// void Game::Invoke(float delay, std::function<void()> action)
// {
//     schedular.Schedule(delay, action);
// }

void Game::Init()
{
    
    ui_m = new UI_Manager();

    scene_stak.reset();

    ChangeSceneStack(0);
    
    lm = new LensManager();

    lm->Init();
    //AddItem(-4);

    auto nullItem = Item();

    runtime_inv.clear();
    for (int i = 0;i < 10;i++)
    {
        /* code */
        runtime_inv.push_back(nullItem);
    }
    

    crafting_inv.clear();
    //ui_m->UPdateUI();.
    for(int i = 0; i < 3; i++){
        crafting_inv.push_back(nullItem);
    }
}


void Game::Update(float dt)
{
    ui_m->Update(dt);    

    if(scene_stak)
        scene_stak->Update(dt);


    schedular.Update(dt);
    tween_manager.Update(dt);
    


    if(lenseActive) {
        lm->Update();
    }

    if(Global::lensOn)
    {
        if(IsMouseButtonDown(0)) {
            if(Vector2Distance(GetMousePosition(), Global::lensPosition) < 200.0f) {
                Global::lensPosition = Vector2Lerp(Global::lensPosition, GetMousePosition(),
                                                   dt * 100.0f);
            }
        }

    }
    
//    if(change_scene)
//    {
//        fade += dt * 5;
//    }
//    if(fade >= 1)
//    {
//        ChangeSceneStack(target_index);
//        fade = 0;
//        change_scene = false;  
//    }
//    
//    if(close_pop)
//    {
//        fade += dt * 5;
//    }

    

    fm.Update(dt);
    
}

void Game::Draw()
{
    
    if(scene_stak)
        scene_stak->Draw();

    if(lenseActive)
        lm->Draw();
    ui_m->Draw();

    fm.Draw();
   // DrawRectangle(0, 0, 1024, 512, Fade(BLACK, fade));

    
   

}




void Game::ChangeSceneF(int idx)
{
    target_index = idx;
    fm.StartFadeOut(0.5f, [this]()
                    {
        std::cout << "complete fadeout" <<std::endl;
        ChangeSceneStack(target_index);
        fm.StartFadeIn(0.5);
    });
}
void Game::ChangeSceneStack(int idx)
{
    switch (idx) {
        case 0:
            
            scene_stak = std::make_unique<SceneStack>();
            scene_stak->addScene(std::make_unique<MainScreen>());
            
            break;
        case 1:
            scene_stak = std::make_unique<SceneStack>();
            scene_stak->addScene(std::make_unique<BoatScreen>());
            //scene_stak->SetPopupScene(0);
            
            break;
        default:
            break;
    }
    
}
/// @brief get entity by name
/// @param Get entity in the scene
/// @return Entity
Entity *Game::getEntity(std::string name)
{
    for(auto& c:current_screen->entities)
    {
        if(c->name == name)
            return c.get();
    }
    return nullptr;
}

Entity *Game::getEntity(int id)
{
    return nullptr;
}

Entity *Game::getUIEntity(std::string name)
{
    for(auto& ui:ui_m->ui)
    {
        if(ui->name == name)
        {
            return ui.get();
        }
    }
    return nullptr;
}

void Game::enable_inv_ui()
{
    if(ui_m->main_inv_ui)
        ui_m->main_inv_ui->setActive(true);
}

int Game::GetItemFromRecipe()
{
    RECIPE_TAG firstTag = crafting_inv[0].recipe_tag;
    if(firstTag == NONE)return 0;

    for(const auto& i:crafting_inv)
    {
        if(i.recipe_tag != firstTag)
            return 0;
    }
    
    auto r = Game::get_Instance().itemDB.getRecipe(firstTag);

    if(Game::get_Instance().hasCraftingItem(r.item_1) && 
    Game::get_Instance().hasCraftingItem(r.item_2) && 
    Game::get_Instance().hasCraftingItem(r.item_3))
    {
        return r.result_item;
    }

    return 0;
}

void Game::SetResultItem(int id)
{
    
    ui_m->craft_man->r_slot->item = itemDB.getItem(id);
    ui_m->craft_man->r_slot->UpdateSlot();
}

void Game::AddItem(int id)
{
    if(hasItem(id))return;

    for (int i = 0; i < runtime_inv.size(); i++)
    {
        /* code */
        if(runtime_inv[i].id >= 0)
        {
            runtime_inv[i] = itemDB.getItem(id);
            break;
        }
    }
    
    ui_m->UPdateUI();

    // finaly add item in the picked item list to prevent spawning
    item_tracker.pickItem(id);
}

void Game::AddItemAt(int id, int idx)
{
    if(hasItem(id))return;
    runtime_inv[idx] = itemDB.getItem(id);

    if(hasCraftingItem(id))removeCraftingItem(id);
    if(Global::selectedItemId == id)Global::selectedItemId = 0;

    ui_m->UPdateUI();
}

void Game::AddCraftingItem(int id)
{
    if(hasCraftingItem(id))return;

    crafting_inv.push_back(itemDB.getItem(id));

    ui_m->UPdateUI();
}

void Game::AddCraftingItemAt(int id, int index)
{
    if(hasCraftingItem(id))return;

    crafting_inv[index] = itemDB.getItem(id);

    if(hasItem(id))removeItem(id);

    ui_m->UPdateUI();
    
}

bool Game::hasItem(int id)
{
    for(int i = 0; i < runtime_inv.size(); i++)
    {
        if(runtime_inv[i].id == id)
            return true;
    }
    return false;
}

void Game::removeItem(int id)
{
    if(!hasItem(id))return;

    for (int i = 0; i < runtime_inv.size(); i++)
    {
        /* code */
        if(runtime_inv[i].id == id)
        {
            runtime_inv[i] = Item(); // assined a NULL item
            ui_m->UPdateUI();

            return;
        }
    }
    
    

}


bool Game::hasCraftingItem(int id)
{
    for(int i = 0; i < crafting_inv.size(); i++)
    {
        if(crafting_inv[i].id == id)
            return true;
    }
    return false;
}

void Game::removeCraftingItem(int id)
{
    if(!hasCraftingItem(id))return;

    for(int i = 0; i < 3; i++)
    {
        if(crafting_inv[i].id == id)
        {
            crafting_inv[i] = Item();
            break;
        }
    }

    int r = GetItemFromRecipe();
    SetResultItem(r);
}

void Game::SelectItem(int id)
{
    if(id >= 0)
    {
        Global::selectedItemId = 0;
        return;
    }
    auto item = itemDB.getItem(id);
    ui_m->inv_man->selectItemAt(id);
    Global::selectedItemId = id;
    slot_index = id;
}


void Game::SetLenseActive() {

    lenseActive = !lenseActive;
    lm->ResetLens();
    Global::lensPosition = {512,256};

    Global::lensOn = lenseActive;
}

void Game::resetCraftingInv()
{
    auto nullItem = Item();

    crafting_inv.clear();
    //ui_m->UPdateUI();.
    for(int i = 0; i < 3; i++){
        crafting_inv.push_back(nullItem);
    }
}



void Game::Unload()
{
    if(ui_m) {
        delete ui_m;
        ui_m = nullptr;
    }

    if(lm) {
        delete lm;
        lm = nullptr;
    }
}









