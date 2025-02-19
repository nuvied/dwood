//
//  GameObject.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef GameObject_h
#define GameObject_h

#include "ResourcesLoader.h"
#include <iostream>
#include "Global.h"

class GameObject
{
protected:
    Vector2 position;
    Rectangle src_rect;
    Rectangle collision_rect;
public:
    int test;

    bool debug = false;
    GameObject()
    {
        
    }
    GameObject(Texture2D &tex,Vector2 &v, Rectangle &r)
    {
        texture = tex;
        position = v;
        src_rect = r;
        scale_rect = {0,0,512, 256};
        collision_rect = {position.x,position.y,src_rect.width,src_rect.height};
    }
    ~GameObject() = default;
    
    
    ///==================================================================================================
    ///
    void set_position(Vector2 &pos)
    {
        position = pos;
    }
    
    
    void set_position(float x, float y)
    {
        position = {x,y};
    }
    
    Vector2& get_position()
    {
        return position;
    }
    
    void set_col_rect(Rectangle &rec)
    {
        collision_rect = rec;
    }
    
    Rectangle& get_col_rect()
    {
        return  collision_rect;
    }
        
    
    
    virtual void Update(float dt)
    {
        collision_rect = {position.x,position.y,src_rect.width,src_rect.height};
        //std::cout << position.x << std::endl;
    }
    
    virtual void Draw()
    {
        DrawTextureRec(texture, src_rect, position, WHITE);
       // DrawTexturePro(texture, src_rect, scale_rect, position, 0.0f, WHITE);
        if(Global::debug || debug)
        {
            DrawRectangle(collision_rect.x, collision_rect.y, collision_rect.width, collision_rect.height, Fade(RED, 0.3f));
        }
    }
    
    
protected:
    Texture2D texture;
    Rectangle scale_rect;
};


#endif /* GameObject_h */

