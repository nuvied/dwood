//
//  Game.hpp
//  my_first_raygame
//
//  Created by apple on 01/02/2025.
//

#ifndef Game_hpp
#define Game_hpp

#include "raylib.h"
#include <stdio.h>
#include "ResourcesLoader.h"
#include <memory>
#include "Fade_manager.hpp"
#include "Scheduler.h"
#include "TweenManager.h"
#include "Global.h"


class UI_Manager;
class SceneStack;
class Screen;
class Entity;


class Game{
    
public:


    float fade = 0;
    int slot_index = -1;
    Fade_manager fm;
    int game_width;
    int game_height;

    Scheduler schedular;
    TweenManager tween_manager;
    
    Screen* current_screen, *target_screen;
    //Scene current_scene;
    std::unique_ptr<SceneStack> scene_stak;
    //GameObject bg;

    UI_Manager* ui_m;
    IconDB icondb;
    std::vector<Item> runtime_inv; // runtime inv;

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    static Game &get_Instance();

    void Invoke(float delay, std::function<void()> action);
    
    void Init();
    void Update(float dt);
    void Draw();
    
    void ChangeSceneF(int idx);
    void ChangeSceneStack(int idx);
    
    Entity* getEntity(std::string name);
    Entity* getEntity(int id);
    
    Entity* getUIEntity(std::string name);

    void enable_inv_ui();
    
    
    void Unload();
    
private:
    Game();
    ~Game() = default;
    bool change_scene;
    int target_index;
    bool close_pop;


};



#endif /* Game_hpp */
