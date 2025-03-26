#ifndef CUSTOM_BEHAVIOUR_H
#define CUSTOM_BEHAVIOUR_H

#include "Component.h"
#include "Entity.h"
#include "Global.h"
#include "BoatScreen.h"



// continuety of components
// file split
class PlayButton_script:public Behaviour
{
public:

    void Init()override
    {
        
        auto sp = entity->getComponent<Sprite>();
        sp->alpha = 0;
        
       Game::get_Instance().tween_manager.AddTween(&sp->alpha, 1.0f,1.0f, 
            []()
            {

            }
        );
    }
    void OnMouseDown() override
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


};

class OnBoat_script:public Behaviour
{
public:

    
    void OnMouseDown()override
    {
        std::cout << "mouse over boat" <<std::endl;
        
        Game::get_Instance().fm.StartFadeOut(0.25f, [](){
            Game::get_Instance().scene_stak->addScene(std::make_unique<BoatPopup>());
            Game::get_Instance().fm.StartFadeIn(0.25f);
        });
        
    }
};


class On_PuzzleHut:public Behaviour
{
public:

   
    void OnMouseDown()override
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
};

class Rotor_script:public Behaviour{
    
public:
    TransformComp* t;
    float correct_order[5] = {45,180,225,0,90};
    int current_idx = 0;
    
private:
    bool held;
    float offset_rotation;

    void Init()override
    {
        t = entity->getComponent<TransformComp>();
        offset_rotation = t->rotation;

       
    }

    
    void OnMouseDown()override
    {
//std::cout << "mouse held" <<std::endl;
        held =true;
        float angle = atan2(Global::mousePos.y - t->position.y, Global::mousePos.x - t->position.x) * RAD2DEG;
        offset_rotation = angle - t->rotation;

        
    }
    
    void CheckforAngle()
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
    void ResetColor()
    {
        for (int i = 0; i < entity->childs.size(); i++)
        {
            /* code */
            entity->getChild(i)->getComponent<Sprite>()->color = {120,120,120,255};
        }
    }
    void SetColor(int i)
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

    void Complete_puzzle()
    {
        
        // close current popup
        Game::get_Instance().fm.StartFadeOut(0.25f, [](){
            Game::get_Instance().scene_stak->ClosePopup();
            Game::get_Instance().scene_stak->addScene(std::make_unique<Hut_interior_popup>());
            Game::get_Instance().fm.StartFadeIn(0.25f);

            Global::rotor_puzzle_done = true;
            


        });


    }
    
    void Update(float dt) override
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


if(held){
    float angle = atan2(Global::mousePos.y - t->position.y , Global::mousePos.x - t->position.x) * RAD2DEG;
    float final_angle = angle - offset_rotation;
    //final_angle = SnapAngle(final_angle, 5.0f);

    t->rotation = AngleLerp(t->rotation, final_angle, dt * 10.0f);
    
    //offset_rotation = target_angle - t->rotation
   
}
    }
    
    float SnapAngle(float angle, float step) {
        return round(angle / step) * step;
    }
    
    float AngleLerp(float current, float target, float t) {
        float diff = fmod(target - current + 540, 360) - 180; // Keep angle between -180 and 180
        return current + diff * t;
    }

    void Draw() override
    {
        if(Global::debug)
            DrawText(TextFormat("rotation %f, %f",t->rotation, offset_rotation ), 300, 10, 10, YELLOW);
    }
    
};



class ClosePopup:public Behaviour
{
public:
    void OnMouseDown()override
    {
        std::cout << "closing pop up"<<std::endl;
        
        Game::get_Instance().fm.StartFadeOut(0.25f, [](){
            Game::get_Instance().scene_stak->ClosePopup();
            Game::get_Instance().fm.StartFadeIn(0.25f);
        });
    }
};
class Slot_script:public Behaviour
{
    public:
        int idx;
        TransformComp *transform;
        Sprite* sp;
        Item slot_item;
        bool selected = false;

    void Init()
    {
       
        transform = entity->getComponent<TransformComp>();
       // sp = entity->getChild(0)->getComponent<Sprite>();
        //std::cout << "childs of slot :" << entity->childs.size() <<  idx <<std::endl;
        drawOrder = 1;
    }


    void OnMouseDown()override
    {
        if(selected) {
            selected = false;
            return;
        }
        if(slot_item.id < 0) {
           // LOG(" item name is : %s", slot_item.name.c_str());
            Game::get_Instance().SelectItem(slot_item.id);
            selected  = true;

        }


    }

    void Draw()override
    {
        //Behaviour::Draw();
        //DrawRectangleLine(transform->getWorldPosition().x, transform->getWorldPosition().y, 30,30,YELLOW);
        if(selected)
            DrawRectangleLines(transform->getWorldPosition().x + 2, transform->getWorldPosition().y + 2, 26,26,YELLOW);
    }

    void UpdateSlot()
    {
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

};


class OnInvButton:public Behaviour
{

      
private:
    bool invShow = true; 
    TransformComp* inv_transform;  


public:
    Entity* inventory;

    void Init() override
    {
        // Game::get_Instance().schedular.Schedule(0.5f, 
        // [this]()
        //     {
        //         if(inventory)
        //         inv_transform = inventory->getComponent<TransformComp>();
            
        //         if(inv_transform)
        //             inv_transform->position.y = 0;

        //         invShow = true;
        //     }
        // );

    }

    void Start()override
    {

        inv_transform = inventory->getComponent<TransformComp>();
        
    
    }

    void onEnable()override
    {
        invShow = true;
        std::cout << "enable " <<entity->name <<std::endl;
    }
    void OnMouseDown()override
    {

        if(inventory){
            //inventory->setActive(!inventory->isActive());
           if(invShow)invShow = false; else invShow = true;
        }
    }

    void Update(float dt)override
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


};


class InventoryManager:public Behaviour
{
    public:
    std::vector<Slot_script*> slots;
    void Init()override
    {

        
       
    }
/*    void ClearSelection()
    {
        for(int i = 0; i < entity->childs.size(); i++)
        {
            auto slot_script = entity->getChild(i)->getComponent<Slot_script>();
        }
    }*/

    void selectItemAt(int idx)
    {
        for(auto& e : slots)
        {
           e->selected = false;
        }

    }
/*    void ChildInitialized()
    {
        slots.clear();
        for(int i = 0; i < entity->childs.size(); i++)
        {
            auto slot = entity->getChild(i);
            slots.push_back(slot);
        }


    }*/

    void UpdateInv(int idx)
    {
        
    }
};


//
// in scene item pickable for inv item
class InvItem_script:public Behaviour
{
    private:
    bool picked = false;
    public:
        std::string name;
        int id = 1;
        Vector2 pos;

        
    InvItem_script(std::string name, Vector2 pos = {0})
    {
        this->name = name;
        this->pos = pos;
    }

    InvItem_script(int id, Vector2 pos = {0})
    {
        this->id = id;
        this->pos = pos;
    }

    void Init() override
    {
        TransformComp* t = entity->getComponent<TransformComp>();
        t->position =  pos;

        if(Game::get_Instance().item_tracker.hasItem(id))
        {
            entity->setActive(false);
        }
    }
    void OnMouseDown()override
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

    
};


class LensButtonScript:public Behaviour
{
public:

    void Init()override
    {

    }

    void OnMouseDown()override
    {
        Game::get_Instance().SetLenseActive();
    }
};

class On_oarPlacement:public Behaviour
{

    void OnMouseDown() override
    {
       // Game::get_Instance().ui_m->ShowSubtitle("oar is missing.....",1.0f);
    }

};

class DrawBg:public Behaviour
{
private:
    TransformComp* transform;
public:
    int w, h;
    DrawBg(int w, int h)
    {
        this->w = w;
        this->h = h;
    }

    void Init() override
    {
        transform = entity->getComponent<TransformComp>();
    }
    void Draw() override
    {
        if(transform)
            DrawRectangle(transform->getWorldPosition().x,
                          transform->getWorldPosition().y
                          ,w,
                          h,Fade(BLACK, 0.25f));
    }
};

class DrawTextComp:public Behaviour
{
private:
    TransformComp* transform;

    float currentIdx;
    float duration;
    float elapsedTime;
    bool isPlaying;
public:
    std::string subtitle = "Default text example loaded";
    int x, y, size;
    float fade = 1.0;

    DrawTextComp(int x, int y, int size) : x(x), y(y), size(size){}

    void Init() override
    {
        currentIdx = 0;
        transform = entity->getComponent<TransformComp>();
    }



    void Update(float dt) override
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

    void Draw() override
    {
        if(transform) {

            DrawText(subtitle.substr(0,currentIdx).c_str(), x + transform->getWorldPosition().x,
                         y + transform->getWorldPosition().y,
                         size, Fade(WHITE, fade));


        }
    }



    void SetText(std::string sub, float t)
    {
        subtitle = sub;
        duration = t;
        currentIdx = 0;
        isPlaying = true;
        elapsedTime = 0;
    }
};

class Craft_btn_script:public Behaviour
{
private:
    bool enabled = false;
public:
    

    void Init()override
    {

    }
    void OnMouseDown()override
    {
        enabled = !enabled;
       // Game::get_Instance().ui_m->panel_ui->setActive(enabled);
        
    }

};
#endif