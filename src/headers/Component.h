//
//  Compoenent.h
//  raygame2
//
//  Created by apple on 06/02/2025.
//

#ifndef Compoenent_h
#define Compoenent_h

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <iostream>
#include "Game.hpp"
#include "Entity.h"
#include "Global.h"
#include "raymath.h"
#include "SceneStack.h"

class BoatPopup;
class PuzzlePopup;
class TransformComp;


typedef struct Circle
{
    float x, y;
    float radius;
    
}Circle;


class TransformComp : public Component
{
public:
    Vector2 position = {0,0};
    float rotation = 0.0f;
    
    TransformComp(float x, float y, float rot = 0.0f)
    {
        position.x = x;
        position.y = y;
    }
    ~TransformComp() = default;
    
    void Update(float dt) override
    {
        
    }
    void Draw() override
    {
        
    }
    
};


class Sprite :public Component
{
private:
    TransformComp* trans = nullptr;
public:
    Texture2D texture;
    Rectangle src_rec;
    
    Vector2 origin;
    
    Sprite()
    {
        
    }
    Sprite(Texture2D &tex, Rectangle rec, Vector2 origin = {0})
    {
        texture = tex;
        src_rec = rec;
        this->origin = origin;
        
    }
    Sprite(Sprite& sp)
    {
        texture = sp.texture;
        src_rec = sp.src_rec;
        origin = sp.origin;
        
    }
    ~Sprite()
    {
        //UnloadTexture(texture);
    }
    
    void setOwner(Entity* e) override
    {
        Component::setOwner(e);
        trans = entity->getComponent<TransformComp>();
        
        
    }
    
    void Update(float dt) override
    {
       // std::cout << texture.width << std::endl;
    }
    
    void setOrigin(float x, float y)
    {
        origin = {x,y};
    }
    
    void Draw() override
    {
        //TransformComp* t = entity->getComponent<TransformComp>();
       
        if(trans){
            DrawTexturePro(texture,src_rec,{trans->position.x,trans->position.y,src_rec.width, src_rec.height},origin,trans->rotation, WHITE);
        }
        else
        {
            DrawTextureRec(texture,src_rec,{0,0}, WHITE);
        }
        
       // DrawTexture(ResourcesLoader::ui_page, 0,0,WHITE);
    }
};




class ColliderComp : public Component
{
private:
    Sprite* sprite = nullptr;
    TransformComp* transform = nullptr;
public:
    
    Rectangle col_rect;
    bool set_to_sprite = false;
    
    ColliderComp(float x, float y, float w, float h)
    {
        col_rect = {x,y,w,h};

        

    }
    
    void setOwner(Entity* e) override
    {
        Component::setOwner(e);
        sprite = entity->getComponent<Sprite>();
        transform = entity->getComponent<TransformComp>();
        
        SetRect();
    }
    
    ~ColliderComp() = default;
    
    void Update(float dt) override
    {
        /*
        if(CheckCollisionPointRec(Global::mousePos, col_rect))
        {
            if(IsMouseButtonDown(0))
                entity->OnClicked();
        }*/
        SetRect();
    }
    
    void Draw() override
    {
        if(Global::debug)
        {
            DrawRectangleLines(col_rect.x, col_rect.y, col_rect.width, col_rect.height, RED);
        }
    }
    
private:
    void SetRect()
    {
        if(sprite && transform)
        {
            col_rect = {transform->position.x, transform->position.y, sprite->src_rec.width, sprite->src_rec.height};
        }
    }
};

class CircleCol:public Component
{
public:
    TransformComp *transform;
    Circle c;
    
    
    CircleCol()
    {
        
    }
    CircleCol(float x, float y, float r)
    {
        c = {x, y, r};
    }
    
    void setOwner(Entity *e) override
    {
        Component::setOwner(e);
        if(e)
        {
            transform = e->getComponent<TransformComp>();
        }
    }
    void Update(float dt) override
    {
        if(transform)
        {
            c.x = transform->position.x;
            c.y = transform->position.y;
        }
    }
    
    void Draw()override
    {
        
        if(Global::debug){
            DrawCircleLines(c.x, c.y, c.radius, RED);
        }
    }
};

class Behaviour:public Component
{
public:
    ColliderComp* col = nullptr;
    CircleCol* circle_col = nullptr;
    Behaviour()
    {
        
    }
    ~Behaviour() = default;
    
    void setOwner(Entity* e) override
    {
        Component::setOwner(e);
        col = entity->getComponent<ColliderComp>();
        circle_col = entity->getComponent<CircleCol>();
        Init();

    }
    virtual void Init()
    {
        
    }
    
    virtual void OnMouseDown()
    {
        
        
    }
    virtual void OnMouseUp()
    {
        
    }
    virtual void OnMouseHeld()
    {
        
    }
    void Update(float dt)override
    {
        if(circle_col){
            if(CheckCollisionPointCircle(Global::mousePos, {circle_col->c.x, circle_col->c.y}, circle_col->c.radius))
            {
                if(IsMouseButtonPressed(0))
                {
                    OnMouseDown();
                }
                else if(IsMouseButtonReleased(0))
                {
                    OnMouseUp();
                }
            }
        }
        
        
        //rectangle collision detection
        
        
        if(!col)return;
        
        if(CheckCollisionPointRec(Global::mousePos, col->col_rect))
        {
            if(IsMouseButtonPressed(0))
            {
                OnMouseDown();
                
            }
            
        }
    }
    void Draw() override
    {
        //DrawRectangleRec(col->col_rect, Fade(RED, 0.3f));
       // DrawRectangle(Global::mousePos.x, Global::mousePos.y, 64, 32, RED);
    }
    

};


class PlayButton_script:public Behaviour
{
public:
    
    

    void OnMouseDown() override
    {
        std::cout << "mouse over col" <<std::endl;
        Game::get_Instance().ChangeSceneF(1);
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
        
        Game::get_Instance().fm.StartFadeOut(0.25f, [](){
            Game::get_Instance().scene_stak->addScene(std::make_unique<PuzzlePopup>());
            Game::get_Instance().fm.StartFadeIn(0.25f);
        });
    }
};

class Rotor_script:public Behaviour{
    
public:
    TransformComp* t;
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
    

    
    void Update(float dt) override
    {
        
        Behaviour::Update(dt);

if (IsMouseButtonReleased(0))
{
    held = false;
    t->rotation = SnapAngle(t->rotation, 5);
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



#endif /* Compoenent_h */


