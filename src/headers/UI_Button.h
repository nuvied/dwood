//
//  UI_Button.h
//  raygame2
//
//  Created by apple on 05/02/2025.
//

#ifndef UI_Button_h
#define UI_Button_h

#include "GameObject.h"
#include "Global.h"

class UI_Button:public GameObject
{
public:
    
    bool clicked = false;
    
    UI_Button()
    {}
    UI_Button(Texture2D &tex,Vector2 &pos, Rectangle &rec )
    {
        
        texture = tex;
        position = pos;
        src_rect = rec;
        collision_rect = {pos.x, pos.y, rec.width, rec.height};
        
        
        
    }
    ~UI_Button()
    {
        
    }
    
    void Update(float dt) override
    {
        GameObject::Update(dt);
        if(CheckCollisionPointRec(Global::mousePos, collision_rect))
        {
            //mouse over
            if(IsMouseButtonPressed(0))
            {
                //std::cout << "clicked" << std::endl;
                clicked = true;
            }
            else
            {
                clicked = false;
            }
        }
        
        collision_rect = {position.x, position.y, src_rect.width, src_rect.height};
        
        
    }
    
    
    void Draw() override
    {
        GameObject::Draw();
        DrawTextureRec(texture, src_rect, position, WHITE);
        
        if(Global::debug || debug){
            DrawRectangleRec(collision_rect, Fade(RED, 0.3f));
        }
        
    }
};


#endif /* UI_Button_h */
