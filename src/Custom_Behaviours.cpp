#include "Custom_behaviours.h"
#include "Game.hpp"
#include "UI_Manager.h"

//==================== play button script
#pragma region PLAY BUTTON
void PlayButton_script::Init()
{
    transform = entity->getComponent<TransformComp>();
    auto sp = entity->getComponent<Sprite>();
    sp->alpha = 0;
    
   Game::get_Instance().tween_manager.AddTween(&sp->alpha, 1.0f,1.0f, 
        []()
        {

        }
    );
}

void PlayButton_script::OnMouseDown()
{
    std::cout << "mouse over col" <<std::endl;
    Game::get_Instance().ChangeSceneF(1);
    //Game::get_Instance().enable_inv_ui();
    
    //Game::get_Instance().schedular.Schedule(0.5f, [](){Game::get_Instance().enable_inv_ui();});
   //auto e = Game::get_Instance().ui_m->main_inv_ui;
    auto mainInv = Game::get_Instance().getUIEntity("ui_parent");
    mainInv->setActive(true);
    mainInv->alpha = 0.0f;
    Game::get_Instance().tween_manager.AddTween(&(mainInv->alpha), 1.0f, 0.5f, nullptr);
}

void PlayButton_script::Draw()
{
    DrawText("PLAY", transform->getWorldPosition().x + 18, transform->getWorldPosition().y + 11, 10,WHITE);
}

# pragma endregion
/////////////////////////////////////////////////////////////////////
//================ on boat script ===============================

#pragma region ON BOAT

void OnBoat_script::OnMouseDown()
{
    std::cout << "mouse over boat" <<std::endl;
    
    Game::get_Instance().fm.StartFadeOut(0.25f, [](){
        Game::get_Instance().scene_stak->addScene(std::make_unique<BoatPopup>());
        Game::get_Instance().fm.StartFadeIn(0.25f);
    });
    
}

//======================= on puzzle hut =========================

void On_PuzzleHut::OnMouseDown()
{
    std::cout << "on hut click" <<std::endl;
    if(!Global::rotor_puzzle_done){
        Game::get_Instance().fm.StartFadeOut(0.25f, [](){
            Game::get_Instance().scene_stak->addScene(std::make_unique<PuzzlePopup>());
            Game::get_Instance().fm.StartFadeIn(0.25f);
        });
    }
    else
    {
        Game::get_Instance().fm.StartFadeOut(0.25f, [](){
            Game::get_Instance().scene_stak->addScene(std::make_unique<Hut_interior_popup>());
            Game::get_Instance().fm.StartFadeIn(0.25f);
        });
    }
}

#pragma endregion
//======================= rotor script ==========================

#pragma region ROTOR SCRIPT
void Rotor_script::Init()
{
    t = entity->getComponent<TransformComp>();
    offset_rotation = t->rotation;

   
}

void Rotor_script::OnMouseDown()
{
    
    held =true;
    float angle = atan2(Global::mousePos.y - t->position.y, Global::mousePos.x - t->position.x) * RAD2DEG;
    offset_rotation = angle - t->rotation;
    
            
}

void Rotor_script::CheckforAngle()
{
    //t->get_child(i);
    int i = 0;

    if(t->rotation == 0 || t->rotation == 360)
    {
        i = 0;
    }
    else if(t->rotation == 45)
    {
        i = 1;
    }
    else if(t->rotation == 90)
    {
        i = 2;
    }
    else if(t->rotation == 135)
    {
        i = 3;
    }
    else if(t->rotation == 180)
    {

        i = 4;
    }
    else if(t->rotation == 225)
    {
        i = 5;
    }
    else if(t->rotation == 270)
    {
        i = 6;
    }
    else if(t->rotation == 315)
    {
        i = 7;
    }
    else
    {
        current_idx = 0;
        ResetColor();
        return;
    }
    //
    SetColor(i);
    // check for correct order


    if(t->rotation == correct_order[current_idx])
    {
        if(current_idx == 4)
        {
            // last index and it is correct
            // puzzle is sovled now
            Complete_puzzle();
        }
        current_idx++;
    }
    else
    {
        ResetColor();
        current_idx = 0;
    }
    

    
    
}

void Rotor_script::ResetColor()
{
    for (int i = 0; i < entity->childs.size(); i++)
    {
        /* code */
        entity->getChild(i)->getComponent<Sprite>()->color = {120,120,120,255};
    }
}

void Rotor_script::SetColor(int i)
{
    auto e = entity->getChild(i);
    if(e)
    {
        e->getComponent<Sprite>()->color = ORANGE;
    }
    else
    {
        std::cout << "no object" << std::endl;
    }
}

void Rotor_script::Complete_puzzle()
{
        
    // close current popup
    Game::get_Instance().fm.StartFadeOut(0.25f, [](){
        Game::get_Instance().scene_stak->ClosePopup();
        Game::get_Instance().scene_stak->addScene(std::make_unique<Hut_interior_popup>());
        Game::get_Instance().fm.StartFadeIn(0.25f);

        Global::rotor_puzzle_done = true;
        


    });


}

void Rotor_script::Update(float dt)
{
        
    Behaviour::Update(dt);

    if(IsKeyDown(KEY_O))
    {
        Complete_puzzle();
    }

    if (IsMouseButtonReleased(0))
    {
    held = false;
    t->rotation = SnapAngle(t->rotation, 5);
    // if(t->rotation > 360)
    // {t->rotation -= 360;}
    // else if(t->rotation < 0)
    // {
    //     t->rotation += 360;
    // }
    t->rotation  = fmod(t->rotation, 360);
    if(t->rotation < 0)
    {
        t->rotation += 360;
    }

    CheckforAngle();


    }


    if(held)
    {
        float angle = atan2(Global::mousePos.y - t->position.y , Global::mousePos.x - t->position.x) * RAD2DEG;
        float final_angle = angle - offset_rotation;
        //final_angle = SnapAngle(final_angle, 5.0f);

        t->rotation = AngleLerp(t->rotation, final_angle, dt * 10.0f);
    
//offset_rotation = target_angle - t->rotation

    }
}

void Rotor_script::Draw()
{
    if(Global::debug)
        DrawText(TextFormat("rotation %f, %f",t->rotation, offset_rotation ), 300, 10, 10, YELLOW);
}


#pragma endrgion
//======================= close popup ==========================

#pragma region  CLOSE POPUP
void ClosePopup::OnMouseDown()
{
    std::cout << "closing pop up"<<std::endl;
    
    Game::get_Instance().fm.StartFadeOut(0.25f, [](){
        Game::get_Instance().scene_stak->ClosePopup();
        Game::get_Instance().fm.StartFadeIn(0.25f);
    });
}

#pragma endregion
//======================= slot script ==========================
#pragma region SLOT SCRIPT(INV)
void Slot_script::Init()
{
       
    transform = entity->getComponent<TransformComp>();
   // sp = entity->getChild(0)->getComponent<Sprite>();
    //std::cout << "childs of slot :" << entity->childs.size() <<  idx <<std::endl;
    drawOrder = 1;
}

void Slot_script::OnMouseDown()
{
    if(slot_item.id == Global::selectedItemId) {
        selected = false;
        Global::selectedItemId = 0;
        return;
    }

    // we have a selected item
    if(Global::selectedItemId < 0 && slot_item.id != Global::selectedItemId)
    {
        int id = Global::selectedItemId;
        if(Game::get_Instance().hasItem(id))
        {
            Game::get_Instance().removeItem(id);
        }
        else if(Game::get_Instance().hasCraftingItem(id))
        {
            Game::get_Instance().removeCraftingItem(id);
        }
        Game::get_Instance().AddItemAt(id, idx);
        Game::get_Instance().SelectItem(slot_item.id);
        selected = true;
    }
    else{

        if(slot_item.id < 0) {
        // LOG(" item name is : %s", slot_item.name.c_str());
            Game::get_Instance().SelectItem(slot_item.id);
            selected  = true;

        }
    }


}

void Slot_script::Draw()
{
    //Behaviour::Draw();
    //DrawRectangleLine(transform->getWorldPosition().x, transform->getWorldPosition().y, 30,30,YELLOW);
    if(selected)
        DrawRectangleLines(transform->getWorldPosition().x + 2, transform->getWorldPosition().y + 2, 26,26,YELLOW);
}

void Slot_script::ClearSlots()
{
    selected= false;
    entity->getChild(0)->setActive(false);
}

void Slot_script::UpdateSlot()
{
    if(Global::selectedItemId >= 0)selected = false;
    // check if 
    auto c = entity->getChild(0);
    if(!c)return;

    //std::cout <<  <<std::endl;
    sp = c->getComponent<Sprite>();
    if(!sp)return;
    // less then zero mean slot has an item
    if(slot_item.id < 0)
    {
        sp->src_rec = slot_item.rect;
        entity->getChild(0)->setActive(true);
    }
}

#pragma endregion
//======================= on inv button ==========================
#pragma region ON INV BUTTON
void OnInvButton::Start()
{
    inv_transform = inventory->getComponent<TransformComp>();
}

void OnInvButton::onEnable()
{
    invShow = true;
    
}

void OnInvButton::OnMouseDown()
{

    if(inventory){
        //inventory->setActive(!inventory->isActive());
       if(invShow)invShow = false; else invShow = true;
    }
}

void OnInvButton::Update(float dt)
{
    Behaviour::Update(dt);
    if(!inv_transform)
        inv_transform = inventory->getComponent<TransformComp>();
    if(!invShow){
    
    inv_transform->position = Vector2Lerp(inv_transform->position, 
        {inv_transform->position.x, 30}, dt * 5.0f);
       
    }
    else
    {
        inv_transform->position = Vector2Lerp(inv_transform->position, 
            {inv_transform->position.x, 0}, dt * 5.0f);
    }
}

#pragma endregion
//======================= inv manager ==========================
#pragma region INV MANAGER
void InventoryManager::selectItemAt(int idx)
{
    for(auto& e : slots)
    {
       e->selected = false;
    }

}
#pragma endregion
//======================= inv item pickable ==========================
#pragma region INV ITEM PICKABLE
InvItem_script::InvItem_script(std::string name, Vector2 pos)
{
    this->name = name;
    this->pos = pos;
}

InvItem_script::InvItem_script(int id, Vector2 pos)
{
    this->id = id;
    this->pos = pos;
}

void InvItem_script::Init()
{
    TransformComp* t = entity->getComponent<TransformComp>();
    t->position =  pos;

    if(Game::get_Instance().item_tracker.hasItem(id))
    {
        entity->setActive(false);
    }
}

void InvItem_script::OnMouseDown()
{
    if(picked)return;
   

    // hide item and disable its clicking;
    Game::get_Instance().AddItem(id);
    auto sp = entity->getComponent<Sprite>();
    picked = true;
    Game::get_Instance().tween_manager.AddTween(&sp->alpha, 0.0f, 0.5f, [this]()
        {
            entity->setActive(false);
            
        }
    );

}

#pragma endregion
//======================= lense button script ==========================
#pragma region LENSE BUTTON
void LensButtonScript::OnMouseDown()
{
    Game::get_Instance().SetLenseActive();
}
#pragma endregion
//======================= oar placement hotspot script ==========================
#pragma region ON OAR PLACEMENT
void On_oarPlacement::OnMouseDown()
{
    if(Global::selectedItemId == itm_oar_ready)
    {
        Game::get_Instance().ui_m->ShowSubtitle("oar is placed.....",1.0f);
        Game::get_Instance().removeItem(itm_oar_ready);
        //enable oar sprite
        entity->setActive(false);
    }
    else
    {
        Game::get_Instance().ui_m->ShowSubtitle("oar is missing.....",1.0f);
    }
}

#pragma endregion
//======================= ui subtitle bg script ==========================
#pragma region DRAE BG
DrawBg::DrawBg(int w, int h)
{
    this->w = w;
    this->h = h;
}

void DrawBg::Init()
{
    transform = entity->getComponent<TransformComp>();
}

void DrawBg::Draw()
{
    if(transform)
        DrawRectangle(transform->getWorldPosition().x,
                      transform->getWorldPosition().y
                      ,w,
                      h,Fade(BLACK, 0.25f));
}

#pragma endregion
//======================= ui subtitle text script ==========================
#pragma region DRAW TEXT
DrawTextComp::DrawTextComp(int x, int y, int size)
{
    this->x = x;
    this->y = y;
    this->size = size;
}

void DrawTextComp::Init()
{
    currentIdx = 0;
    transform = entity->getComponent<TransformComp>();
}

void DrawTextComp::Update(float dt)
{
    Behaviour::Update(dt);
    if(!isPlaying)return;

    elapsedTime += dt;
    float t = elapsedTime / duration;
    if(t > 1.0)
    {
        t = 1.0f;
        isPlaying = false;
    }

    //currentIdx = static_cast<int>( TweenLinear(0,(float )subtitle.length(),t));
    if(currentIdx < subtitle.length())
        currentIdx++;

    //LOG("index %i",currentIdx);
}

void DrawTextComp::Draw()
{
    if(transform) {

        DrawText(subtitle.substr(0,currentIdx).c_str(), x + transform->getWorldPosition().x,
                     y + transform->getWorldPosition().y,
                     size, Fade(WHITE, fade));


    }
}

void DrawTextComp::SetText(std::string sub, float t)
{
    subtitle = sub;
    duration = t;
    currentIdx = 0;
    isPlaying = true;
    elapsedTime = 0;
}
#pragma endregion

#pragma region CRAFT BUTTON SCRIPT
void Craft_btn_script::OnMouseDown()
{
    enabled = !enabled;
    Game::get_Instance().ui_m->panel_ui->setActive(enabled);
    if(!enabled)
    {
        for(int i = 0; i < Game::get_Instance().crafting_inv.size(); i++)
        {
            if(Game::get_Instance().crafting_inv[i].id < 0)
                Game::get_Instance().AddItem(Game::get_Instance().crafting_inv[i].id);
        }
        Game::get_Instance().resetCraftingInv();
        Game::get_Instance().ui_m->UPdateUI();

    }

}

#pragma endregion
//======================= crafting slot ==========================
#pragma region CRAFTING SLOT
void Crafting_Slot::Init()
{
    drawOrder = 1;
    transform = entity->getComponent<TransformComp>();
}

void Crafting_Slot::OnMouseDown()
{
    //steps
    // get item by id using global selected item.id
    //assign item to this slot
    //check fo recipe tag and get recipe
    //pass the items to Game class for checking
    
      //checking if any item is selected

    // if slot is not empty
    if(Global::selectedItemId == item.id)
    {
        selected = false;
        Global::selectedItemId = 0;
        return;
    }
    
    if(item.id <0)
    {
        if(Global::selectedItemId >= 0)
        {
            // Game::get_Instance().AddItem(item.id);
            // Game::get_Instance().removeCraftingItem(item.id);
            // Game::get_Instance().ui_m->UPdateUI();
            Global::selectedItemId = item.id;
            selected = true;
            printf("item selected");
        }
    }
    else
    {
        int id = Global::selectedItemId;

        if(id >= 0)return;

        if(id < 0 && item.id != id)
        {
            Game::get_Instance().removeCraftingItem(id);
            Game::get_Instance().AddCraftingItemAt(id, idx);
            // item = Game::get_Instance().itemDB.getItem(id);
            Global::selectedItemId = 0;
            //UpdateSlots();

        }
        
    }
    
    if(item.recipe_tag != NONE)
    {
        int r_item = Game::get_Instance().GetItemFromRecipe();

        Game::get_Instance().SetResultItem(r_item);
        
    }

}

void Crafting_Slot::UpdateSlots()
{
    auto c = entity->getChild(0);
    if(item.id < 0)
    {
        c->getComponent<Sprite>()->src_rec = item.rect;
        c->setActive(true);
    }

}

void Crafting_Slot::ClearSlots()
{
    selected = false;
    auto c = entity->getChild(0);
    if(c)
    {
        c->setActive(false);
    }
}

void Crafting_Slot::Draw()
{
    //DrawRectangleLines(transform->getWorldPosition().x + 2, transform->getWorldPosition().y + 2, 26,26,YELLOW);
    if(selected)
        DrawRectangleLines(transform->getWorldPosition().x + 2, transform->getWorldPosition().y + 2, 26,26,YELLOW);

}

#pragma endregion
//======================= result slot ==========================
#pragma region RESULT SLOT
void Result_SLot::Init()
{
}

void Result_SLot::OnMouseDown()
{
    Game::get_Instance().resetCraftingInv();
    Game::get_Instance().ui_m->UPdateUI();
    Game::get_Instance().AddItem(item.id);
    item = Item(); // assigned empty item
    UpdateSlot();
}

void Result_SLot::UpdateSlot()
{
    if(item.id < 0)
    {
        entity->getChild(0)->setActive(true);
        entity->getChild(0)->getComponent<Sprite>()->src_rec = item.rect;
    }
    else
    {
        entity->getChild(0)->setActive(false);
    }
}

#pragma endregion
//======================= CRAFTING BASE UI ==========================
#pragma region CRAFTING MAIN
void Crafting_main::OnMouseDown()
{
    std::cout << "click working craftin base"<<std::endl;
}

void Crafting_main::Update(float dt)
{
    //auto t = entity->getComponent<TransformComp>();
   // t->position = Global::mousePos;
}
#pragma endregion

#pragma region ONDEBUG
void OnDebug::OnMouseDown()
{
    Global::debug = !Global::debug;
}
#pragma endregion