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

class LensManager;
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
    bool IsOverUI = false;
    LensManager* lm;
    bool lenseActive = false;
    void SetLenseActive();

    ItemDB itemDB;
    ItemTracker item_tracker;

    std::vector<Item> runtime_inv; // runtime inv;
    std::vector<Item> crafting_inv; // limited to 3 items


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

    void AddItem(int id);
    void AddItemAt(int id, int idx);
    bool hasItem(int id);
    void SelectItem(int id);
    void removeItem(int id);

    void ShowSubtitle(std::string sub, float t = 0);

    void AddCraftingItem(int id);
    void AddCraftingItemAt(int id, int index);
    bool hasCraftingItem(int id);
    void removeCraftingItem(int id);
    void resetCraftingInv();

    int GetItemFromRecipe();
    void SetResultItem(int id);

    void Unload();
    
private:
    Game();
    ~Game() = default;
    bool change_scene;
    int target_index;
    bool close_pop;


};



#endif /* Game_hpp */
