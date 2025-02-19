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

Game::Game()
{
    
}

Game &Game::get_Instance()
{
    static Game instance;
    return instance;
}


void Game::Init()
{
    scene_stak.reset();

    ChangeSceneStack(0);
}

void Game::Update(float dt)
{
    if(scene_stak)
        scene_stak->Update(dt);
    
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
    
    fm.Update(dt);
    
}

void Game::Draw()
{
    if(scene_stak)
        scene_stak->Draw();
    
    
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



void Game::Unload()
{
    
}




