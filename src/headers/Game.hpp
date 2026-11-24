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
#include <filesystem>
#include <unordered_map>
#include "ResourcesLoader.h"
#include <memory>
#include "Fade_manager.hpp"
#include "Scheduler.h"
#include "TweenManager.h"
#include "Global.h"
#include "RenderSettings.h"
#include "BoatScreen.h"
#include "SceneManager.h"
#include "SoundManager.h"
#include "VariableStore.h"

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
    int game_width = 1024;
    int game_height = 512;

    Scheduler schedular;
    TweenManager tween_manager;
    
    Screen* current_screen = nullptr;
    Screen* target_screen = nullptr;
    //Scene current_scene;
    std::unique_ptr<SceneStack> scene_stak;
    std::unique_ptr<SceneManager> scene_manager;
    SoundManager soundManager;
    VariableStore variables;
    //GameObject bg;

    UI_Manager* ui_m = nullptr;
    bool IsOverUI = false;
    LensManager* lm = nullptr;
    bool lenseActive = false;
    void SetLenseActive();

    ItemDB itemDB;
    ItemTracker item_tracker;

    std::vector<Item> runtime_inv; // runtime inv;
    std::array<Item, 3> crafting_inv; // limited to 3 items


    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    static Game &get_Instance();

    void Invoke(float delay, std::function<void()> action);
    
    void Init(const std::filesystem::path& sceneDirectory = {});
    void SetRenderResolution(int width, int height);
    float RenderScale() const;
    int RenderTargetWidth() const { return game_width * RenderSettings::SupersampleFactor; }
    int RenderTargetHeight() const { return game_height * RenderSettings::SupersampleFactor; }
    void StartGame();
    void StopGame();
    void Update(float dt);
    void Draw();
    
    void ChangeSceneF(int idx);
    void ChangeSceneF(const std::filesystem::path& scenePath);
    void ChangeSceneStack(int idx);
    void LoadManagedScene(const std::filesystem::path& scenePath);
    void OpenPopupScene(const std::filesystem::path& scenePath);
    void OpenPopupSceneF(const std::filesystem::path& scenePath, float duration);
    void ClosePopupSceneF(float duration);
    bool ClosePopupScene();
    void RequestClosePopupScene();
    
    Entity* getEntity(std::string name);
    Entity* getEntity(int id);
    
    Entity* getUIEntity(std::string name);

    void enable_inv_ui();

    void AddItem(int id);
    void AddItemAt(int id, int idx);
    bool hasItem(int id);
    void SelectItem(int id);
    void removeItem(int id);
    float InventoryItemAlpha(int id) const;
    void UpdateInventoryItemFades(float dt);

    void ShowSubtitle(std::string sub, float t = 0);

    void AddCraftingItem(int id);
    void AddCraftingItemAt(int id, int index);
    bool hasCraftingItem(int id);
    void removeCraftingItem(int id);
    void resetCraftingInv();

    int GetItemFromRecipe();
    void SetResultItem(int id);

    void UpdateUI();

    void Unload();
    
private:
    Game();
    ~Game() = default;
    bool change_scene;
    int target_index;
    bool close_pop;
    std::vector<std::filesystem::path> managedSceneStack;
    std::vector<VariableStore::SceneState> suspendedSceneVariables;
    bool closePopupRequested = false;
    std::unordered_map<int, float> inventoryItemFadeIn;

public:
    static constexpr float InventoryItemFadeDuration = 0.1f;

private:


};



#endif /* Game_hpp */
