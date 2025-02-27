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
    

    //AddItem(-4);

    //ui_m->UPdateUI();
}


void Game::Update(float dt)
{
    if(scene_stak)
        scene_stak->Update(dt);


    schedular.Update(dt);
    tween_manager.Update(dt);
    
    if(IsKeyPressed(KEY_B))
    {
        ChangeSceneStack(1);
    }
    else if(IsKeyPressed(KEY_V))
    {
        ChangeSceneStack(0);
    }
    else if(IsKeyPressed(KEY_C))
    {
        scene_stak->ClosePopup();
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
    ui_m->Update(dt);    

    fm.Update(dt);
    
}

void Game::Draw()
{
    if(scene_stak)
        scene_stak->Draw();
    
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

void Game::AddItem(int id)
{
    if(hasItem(id))return;

    runtime_inv.push_back( itemDB.getItem(id));
    ui_m->UPdateUI();

    // finaly add item in the picked item list to prevent spawning
    item_tracker.pickItem(id);
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

void Game::Unload()
{
    delete ui_m;
    ui_m = nullptr;
}




