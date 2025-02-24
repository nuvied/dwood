#ifndef CUSTOM_BEHAVIOUR_H
#define CUSTOM_BEHAVIOUR_H

#include "Component.h"
#include "Entity.h"


class BoatPopup;
class PuzzlePopup;
class Hut_interior_popup;

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
        
        Game::get_Instance().schedular.Schedule(0.5f, [](){Game::get_Instance().enable_inv_ui();});
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
    void SceneLoaded()
    {
        
    }

    void OnMouseDown()override
    {
        std::cout << "slot :" <<  idx <<std::endl;
    }


};


class OnInvButton:public Behaviour
{
    public:
        Entity* inventory = nullptr;
    private:
    bool invShow = true;

    void Init() override
    {
        
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
        auto inv_transform = inventory->getComponent<TransformComp>();
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


#endif