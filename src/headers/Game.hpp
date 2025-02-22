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


class UI_Manager;
class SceneStack;
class Screen;



class Game{
    
public:


    float fade = 0;
    
    Fade_manager fm;
    int game_width;
    int game_height;
    
    Screen* current_screen, *target_screen;
    //Scene current_scene;
    std::unique_ptr<SceneStack> scene_stak;
    //GameObject bg;

    UI_Manager* ui_m;

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    static Game &get_Instance();

    
    
    void Init();
    void Update(float dt);
    void Draw();
    
    void ChangeSceneF(int idx);
    void ChangeSceneStack(int idx);
    
    
    
    void Unload();
    
private:
    Game();
    ~Game() = default;
    bool change_scene;
    int target_index;
    bool close_pop;
};



#endif /* Game_hpp */
