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
#include "HouseFrontScreen.h"
//#include "Fade_manager.hpp"
#include "UI_Manager.h"
#include "LensManager.h"
#include "Screen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

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

void Game::Init(const std::filesystem::path& sceneDirectory)
{
    game_width = 1024;
    game_height = 512;
    
    ui_m = new UI_Manager();

    scene_stak.reset();

    auto directory = sceneDirectory;
    if (directory.empty()) {
#ifdef DEVILWOOD_SCENES_DIR
        directory = DEVILWOOD_SCENES_DIR;
#else
        directory = "scenes";
#endif
    }

    const auto settingsPath = directory / ".devilwood" / "game_settings.json";
    if (std::filesystem::exists(settingsPath)) {
        try {
            std::ifstream input(settingsPath);
            if (!input)
                throw std::runtime_error("Cannot open game settings: " + settingsPath.string());
            nlohmann::json settings;
            input >> settings;
            if (!settings.is_object() || settings.value("version", 0) != 1 ||
                !settings.contains("render_width") ||
                !settings["render_width"].is_number_integer() ||
                !settings.contains("render_height") ||
                !settings["render_height"].is_number_integer())
                throw std::runtime_error("Game settings must contain version 1 and integer render dimensions.");
            const auto width = settings["render_width"].get<std::int64_t>();
            const auto height = settings["render_height"].get<std::int64_t>();
            if (width < 1 || width > 8192 || height < 1 || height > 8192)
                throw std::runtime_error("Game render dimensions must be between 1 and 8192 pixels.");
            SetRenderResolution(static_cast<int>(width), static_cast<int>(height));
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Unable to load game settings: %s", e.what());
        }
    } else {
        SetRenderResolution(game_width, game_height);
    }

    scene_manager = std::make_unique<SceneManager>(directory);
    variables.ResetGlobals(scene_manager->GlobalVariableDefinitions());
    variables.ResetScene({});
    soundManager.Load(directory);
    itemDB.Load(directory);
    item_tracker.Reset();
    inventoryItemFadeIn.clear();
    Global::selectedItemId = 0;
    slot_index = -1;
    ui_m->projectUI = ProjectUI::Load(directory);
    bool loadedStartupScene = false;
    try {
        const auto startupScene = scene_manager->StartupScene();
        if (startupScene) {
            LoadManagedScene(*startupScene);
            loadedStartupScene = true;
            if (scene_manager->StartupFadeIn())
                fm.StartFadeIn(0.5f);
        }
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Unable to load startup scene: %s", e.what());
    }
    if (!loadedStartupScene)
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
    std::size_t startingSlot = 0;
    for (const auto& item : itemDB.items_database)
        if (item.carryOnStart && startingSlot < runtime_inv.size())
            runtime_inv[startingSlot++] = item;
    
    //ui_m->UPdateUI();.
    for(int i = 0; i < 3; i++){
        crafting_inv[i] = nullItem;
    }
}

void Game::SetRenderResolution(int width, int height)
{
    if (width < 1 || width > 8192 || height < 1 || height > 8192)
        throw std::invalid_argument("Game render dimensions must be between 1 and 8192 pixels.");

    if (scene_stak) {
        for (auto& screen : scene_stak->stack)
            screen->SetRenderResolution(width, height);
    }
    game_width = width;
    game_height = height;
    if (ui_m)
        ui_m->ui_cam.zoom = RenderScale();
    const float scale = RenderScale();
    Global::lensPosition = {RenderTargetWidth() / (2.0f * scale),
                            RenderTargetHeight() / (2.0f * scale)};
}

float Game::RenderScale() const
{
    return std::min(static_cast<float>(RenderTargetWidth()) / RenderSettings::AuthoredWidth,
                    static_cast<float>(RenderTargetHeight()) / RenderSettings::AuthoredHeight);
}

void Game::StartGame()
{
    TraceLog(LOG_INFO, "[GAME] Play started");
    fm.alpha = 0.0f;
    fm.fadein = false;
    fm.fadeout = false;
    fm.onFadeComplete = nullptr;

    if (scene_manager) {
        variables.ResetGlobals(scene_manager->GlobalVariableDefinitions());
        variables.ResetScene({});
        soundManager.Load(scene_manager->Directory());
    }
    if (scene_manager && ui_m)
        ui_m->projectUI = ProjectUI::Load(scene_manager->Directory());
    if (scene_manager)
        itemDB.Load(scene_manager->Directory());
    item_tracker.Reset();
    inventoryItemFadeIn.clear();
    Global::selectedItemId = 0;
    slot_index = -1;
    runtime_inv.assign(10, Item());
    std::size_t slot = 0;
    for (const auto& item : itemDB.items_database) {
        if (item.carryOnStart && slot < runtime_inv.size())
            runtime_inv[slot++] = item;
    }

    if (scene_manager) {
        const auto startupScene = scene_manager->StartupScene();
        if (startupScene) {
            LoadManagedScene(*startupScene);
            if (scene_manager->StartupFadeIn())
                fm.StartFadeIn(0.5f);
            return;
        }
    }

    if (!scene_stak || scene_stak->stack.empty())
        ChangeSceneStack(0);
}

void Game::StopGame()
{
    TraceLog(LOG_INFO, "[GAME] Play stopped");
    closePopupRequested = false;
    inventoryItemFadeIn.clear();
    Global::selectedItemId = 0;
    slot_index = -1;
    fm.alpha = 0.0f;
    fm.fadein = false;
    fm.fadeout = false;
    fm.onFadeComplete = nullptr;
    soundManager.StopAll();
    while (scene_stak && scene_stak->stack.size() > 1)
        ClosePopupScene();
}


void Game::Update(float dt)
{
    UpdateInventoryItemFades(dt);
    soundManager.Update(dt);
    ui_m->Update(dt);    

    if(scene_stak)
        scene_stak->Update(dt);

    if (closePopupRequested) {
        closePopupRequested = false;
        ClosePopupScene();
    }

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

    if(Global::debug)
    {
        DrawText(TextFormat("Mouse Pos [%i, %i]", (int)GetMousePosition().x, (int)GetMousePosition().y),300,10,10,YELLOW );
    }
   

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

void Game::ChangeSceneF(const std::filesystem::path& scenePath)
{
    fm.StartFadeOut(0.5f, [this, scenePath]()
    {
        try {
            LoadManagedScene(scenePath);
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Unable to change to JSON scene: %s", e.what());
        }
        fm.StartFadeIn(0.5f);
    });
}

void Game::OpenPopupSceneF(const std::filesystem::path& scenePath, float duration)
{
    if (!std::isfinite(duration) || duration < 0)
        throw std::runtime_error("Popup transition duration must be finite and non-negative.");
    fm.StartFadeOut(duration, [this, scenePath, duration]()
    {
        try {
            OpenPopupScene(scenePath);
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR, "Unable to open popup scene: %s", e.what());
        }
        fm.StartFadeIn(duration);
    });
}

void Game::ClosePopupSceneF(float duration)
{
    if (!std::isfinite(duration) || duration < 0)
        throw std::runtime_error("Popup transition duration must be finite and non-negative.");
    if (!scene_stak || scene_stak->stack.size() <= 1) {
        TraceLog(LOG_WARNING, "Cannot close a popup when no popup scene is open.");
        return;
    }
    fm.StartFadeOut(duration, [this, duration]()
    {
        if (!ClosePopupScene())
            TraceLog(LOG_WARNING, "Popup scene was no longer open when its transition completed.");
        fm.StartFadeIn(duration);
    });
}

void Game::LoadManagedScene(const std::filesystem::path& scenePath)
{
    if (!scene_manager)
        throw std::runtime_error("Scene manager is not initialized.");
    if (scene_manager->Type(scenePath) != SceneType::Main)
        throw std::runtime_error("A popup cannot replace the active scene; open it as a popup instead.");
    const auto sceneVariables = scene_manager->SceneVariableDefinitions(scenePath);
    const auto previousSceneVariables = variables.CaptureSceneState();
    variables.ResetScene(sceneVariables);
    std::unique_ptr<Screen> screen;
    try {
        screen = scene_manager->LoadScene(scenePath);
        screen->SetRenderResolution(game_width, game_height);
        screen->Init();
    } catch (...) {
        variables.RestoreSceneState(previousSceneVariables);
        throw;
    }
    closePopupRequested = false;
    auto nextStack = std::make_unique<SceneStack>();
    nextStack->addScene(std::move(screen));
    scene_stak = std::move(nextStack);
    managedSceneStack = {scene_manager->Directory() / scenePath.filename()};
    suspendedSceneVariables.clear();
    current_screen = scene_stak->stack.back().get();
    TraceLog(LOG_INFO, "[GAME] Scene changed: %s", scenePath.filename().string().c_str());
}

void Game::OpenPopupScene(const std::filesystem::path& scenePath)
{
    if (!scene_manager || !scene_stak || scene_stak->stack.empty() ||
        managedSceneStack.empty())
        throw std::runtime_error("A popup requires an active managed main scene.");
    if (scene_manager->Type(scenePath) != SceneType::Popup)
        throw std::runtime_error("Only popup scenes can be opened as an overlay.");
    const auto parent = scene_manager->ParentScene(scenePath);
    if (!parent || parent->filename() != managedSceneStack.front().filename())
        throw std::runtime_error("Popup scene is not attached to the active main scene.");

    const auto sceneVariables = scene_manager->SceneVariableDefinitions(scenePath);
    auto savedVariables = variables.CaptureSceneState();
    variables.ResetScene(sceneVariables);
    std::unique_ptr<Screen> screen;
    try {
        screen = scene_manager->LoadScene(scenePath);
        screen->SetRenderResolution(game_width, game_height);
        screen->Init();
    } catch (...) {
        variables.RestoreSceneState(savedVariables);
        throw;
    }
    scene_stak->PushPopup(std::move(screen));
    suspendedSceneVariables.push_back(std::move(savedVariables));
    managedSceneStack.push_back(scene_manager->Directory() / scenePath.filename());
    current_screen = scene_stak->stack.back().get();
    TraceLog(LOG_INFO, "[GAME] Popup opened: %s", scenePath.filename().string().c_str());
}

bool Game::ClosePopupScene()
{
    closePopupRequested = false;
    if (!scene_stak || !scene_stak->ClosePopup())
        return false;
    if (managedSceneStack.size() > 1 && !suspendedSceneVariables.empty()) {
        managedSceneStack.pop_back();
        variables.RestoreSceneState(suspendedSceneVariables.back());
        suspendedSceneVariables.pop_back();
    }
    current_screen = scene_stak->stack.empty() ? nullptr : scene_stak->stack.back().get();
    TraceLog(LOG_INFO, "[GAME] Popup closed");
    return true;
}

void Game::RequestClosePopupScene()
{
    if (!scene_stak || scene_stak->stack.size() <= 1) {
        TraceLog(LOG_WARNING, "Cannot close a popup when no popup scene is open.");
        return;
    }
    closePopupRequested = true;
}

void Game::ChangeSceneStack(int idx)
{
    closePopupRequested = false;
    auto replaceScene = [this](std::unique_ptr<Screen> screen)
    {
        variables.ResetScene({});
        screen->SetRenderResolution(game_width, game_height);
        scene_stak = std::make_unique<SceneStack>();
        scene_stak->addScene(std::move(screen));
        managedSceneStack.clear();
        suspendedSceneVariables.clear();
    };

    switch (idx) {
        case 0:
            replaceScene(std::make_unique<MainScreen>());
            break;
        case 1:
            replaceScene(std::make_unique<BoatScreen>());
            //scene_stak->SetPopupScene(0);
            
            break;
        case 2:
            replaceScene(std::make_unique<HouseFrontScreen>());
            break;
        default:
            break;
    }

    if (scene_stak && !scene_stak->stack.empty())
        current_screen = scene_stak->stack.back().get();
    
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

    RECIPE_TAG firstTag = NONE;
    for (int i = 0; i < crafting_inv.size(); i++)
    {
        /* code */
        if(crafting_inv[i].id < 0)
        {
            firstTag = crafting_inv[i].recipe_tag;

        }
    }
    
    
    printf("tag %i \n", firstTag);

    if(firstTag == NONE)return 0;

   

    for(const auto& i:crafting_inv)
    {
        if(i.recipe_tag != firstTag)
            return 0;
    }
    
    auto r = Game::get_Instance().itemDB.getRecipe(firstTag);

    bool allfound = true;
    for (int i = 0; i < r.items.size(); i++)
    {
        /* code */
        if(!hasCraftingItem( r.items[i]))
        {
            allfound = false;
            break;
        }
        
    }
    
    if(allfound)return r.result_item;


    return 0;
}

void Game::SetResultItem(int id)
{
    if(id >= 0)return;

    ui_m->craft_man->r_slot->item = itemDB.getItem(id);
    ui_m->craft_man->r_slot->UpdateSlot();
}

void Game::UpdateUI()
{
    ui_m->UPdateUI();
}

void Game::AddItem(int id)
{
    if(hasItem(id))return;

    bool added = false;
    for (int i = 0; i < runtime_inv.size(); i++)
    {
        /* code */
        if(runtime_inv[i].id >= 0)
        {
            runtime_inv[i] = itemDB.getItem(id);
            added = true;
            break;
        }
    }
    if (added) {
        inventoryItemFadeIn[id] = 0.0f;
        const auto addedItem = itemDB.getItem(id);
        TraceLog(LOG_INFO, "[GAME] Inventory item added: %s (%d)",
                 addedItem.name.c_str(), id);
    }
    
    if (ui_m)
        ui_m->UPdateUI();

    // finaly add item in the picked item list to prevent spawning
    item_tracker.pickItem(id);
}

void Game::AddItemAt(int id, int idx)
{
    if(hasItem(id))return;
    runtime_inv[idx] = itemDB.getItem(id);
    inventoryItemFadeIn[id] = 0.0f;

    if(hasCraftingItem(id))removeCraftingItem(id);
    if(Global::selectedItemId == id)Global::selectedItemId = 0;

    if (ui_m)
        ui_m->UPdateUI();
}

void Game::AddCraftingItem(int id)
{
    if(hasCraftingItem(id))return;

    
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
            const std::string removedName = runtime_inv[i].name;
            runtime_inv[i] = Item(); // assined a NULL item
            inventoryItemFadeIn.erase(id);
            if (Global::selectedItemId == id)
                Global::selectedItemId = 0;
            if (ui_m)
                ui_m->UPdateUI();
            TraceLog(LOG_INFO, "[GAME] Inventory item removed: %s (%d)",
                     removedName.c_str(), id);

            return;
        }
    }
}

float Game::InventoryItemAlpha(int id) const
{
    const auto it = inventoryItemFadeIn.find(id);
    if (it == inventoryItemFadeIn.end())
        return 1.0f;
    return std::clamp(it->second / InventoryItemFadeDuration, 0.0f, 1.0f);
}

void Game::UpdateInventoryItemFades(float dt)
{
    if (dt <= 0.0f || !std::isfinite(dt))
        return;
    for (auto it = inventoryItemFadeIn.begin(); it != inventoryItemFadeIn.end();) {
        it->second += dt;
        if (it->second >= InventoryItemFadeDuration)
            it = inventoryItemFadeIn.erase(it);
        else
            ++it;
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
    if(id >= 0)return;

     for(int i = 0; i < crafting_inv.size(); i++)
     {
        if(crafting_inv[i].id == id)
        {
            crafting_inv[i] = Item();
            break;
        }
     }

    int r = GetItemFromRecipe();
     SetResultItem(r);
    ui_m->UPdateUI();
}

void Game::SelectItem(int id)
{
    if(id >= 0 || !hasItem(id))
    {
        if (Global::selectedItemId < 0)
            TraceLog(LOG_INFO, "[GAME] Inventory item deselected");
        Global::selectedItemId = 0;
        slot_index = -1;
        return;
    }
    if (ui_m && ui_m->inv_man)
        ui_m->inv_man->selectItemAt(id);
    Global::selectedItemId = id;
    slot_index = id;
    const auto selected = itemDB.getItem(id);
    TraceLog(LOG_INFO, "[GAME] Inventory item selected: %s (%d)",
             selected.name.c_str(), id);
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


    ui_m->craft_man->r_slot->item = nullItem; // assign empty item
    ui_m->craft_man->r_slot->UpdateSlot();
    //ui_m->UPdateUI();.
    for(int i = 0; i < 3; i++){
        crafting_inv[i] = nullItem;
    }
    
}



void Game::Unload()
{
    soundManager.Unload();
    if(ui_m) {
        delete ui_m;
        ui_m = nullptr;
    }

    if(lm) {
        delete lm;
        lm = nullptr;
    }
}









