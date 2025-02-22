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
class Hut_interior_popup;
class TransformComp;


typedef struct Circle
{
    float x, y;
    float radius;
    
}Circle;




struct Matrix2D {
    float m[3][3]; // 3x3 Transformation Matrix

    // Initialize as identity matrix
    Matrix2D() {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    // Multiply two matrices
    Matrix2D operator*(const Matrix2D& other) const {
        Matrix2D result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = m[i][0] * other.m[0][j] +
                                 m[i][1] * other.m[1][j] +
                                 m[i][2] * other.m[2][j];
            }
        }
        return result;
    }

    // Create a transformation matrix from position, rotation, and scale
    static Matrix2D Transform(Vector2 position, float rotation, Vector2 scale) {
        Matrix2D result;
        float cosR = cos(rotation * DEG2RAD);
        float sinR = sin(rotation * DEG2RAD);

        result.m[0][0] = cosR * scale.x;
        result.m[0][1] = -sinR * scale.y;
        result.m[0][2] = position.x;

        result.m[1][0] = sinR * scale.x;
        result.m[1][1] = cosR * scale.y;
        result.m[1][2] = position.y;

        result.m[2][0] = 0;
        result.m[2][1] = 0;
        result.m[2][2] = 1;

        return result;
    }

    // Transform a vector by the matrix
    Vector2 TransformVector(Vector2 v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2],
            m[1][0] * v.x + m[1][1] * v.y + m[1][2]
        };
    }
};





class TransformComp : public Component
{
public:
    Vector2 position = {0,0};
    float rotation = 0.0f;
    Vector2 scale = {1.0f, 1.0f};


    TransformComp* parent = nullptr;
    
    TransformComp(float x, float y, float rot = 0.0f)
    {
        position.x = x;
        position.y = y;
    }
    ~TransformComp() = default;

    void OnChangePerent()override
    {
        std::cout << "parent changed" <<std::endl;
        parent = entity->parent->getComponent<TransformComp>();
        //auto offsetpos = position - pt->position;

    }
    
    Matrix2D getWorldMatrix()const{
        Matrix2D local = Matrix2D::Transform(position, rotation, scale);
        if(parent) return parent->getWorldMatrix()*local;
        return local;
    }

    Vector2 getWorldPosition() const{
        return getWorldMatrix().TransformVector({0,0});
    }

    float getWorldRotation()const{
        return parent? parent->getWorldRotation() + rotation :rotation;
    }



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
    

    Color color;
    Sprite()
    {
        
    }
    Sprite(Texture2D tex, Rectangle rec, Vector2 origin = {0}, Color c = WHITE)
    {
        texture = tex;
        src_rec = rec;
        this->origin = origin;
        color = c;
        
    }
    // Sprite(Sprite& sp)
    // {
    //     texture = sp.texture;
    //     src_rec = sp.src_rec;
    //     origin = sp.origin;
    //     color = sp.color;
        
    // }
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
       for(auto& c: entity->childs)
        {
            c->Update(dt);
        }
    }
    
    void setOrigin(float x, float y)
    {
        origin = {x,y};
    }
    
    void Draw() override
    {
        //TransformComp* t = entity->getComponent<TransformComp>();
        if(!trans)
            trans = entity->getComponent<TransformComp>(); 
       
        if(trans){

            Vector2 worldPos = trans->getWorldPosition();
            float worldRot = trans->getWorldRotation();
            DrawTexturePro(texture,src_rec,
                {worldPos.x,worldPos.y,src_rec.width * trans->scale.x, 
                    src_rec.height * trans->scale.y
                }
                ,
                {origin.x * src_rec.width, origin.y * src_rec.height }
                ,
                worldRot, 
                Fade(color, color.a));

                for(auto& c: entity->childs)
                {
                    c->Draw();
                }
        }
        else
        {
            DrawTextureRec(texture,src_rec,{0,0}, Fade(color, color.a));
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
            Vector2 worldPos = transform->getWorldPosition();
            col_rect = {worldPos.x, worldPos.y, sprite->src_rec.width, sprite->src_rec.height};
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



#endif /* Compoenent_h */


