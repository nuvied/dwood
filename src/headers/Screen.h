//
//  Screen.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef Screen_h
#define Screen_h

#include <vector>
#include "Entity.h"
#include "InvItem_Entity.h"
#include <algorithm>




class Screen
{
private:
    float alpha = 1;
public:
    bool active = false;        /// use to set active scene in the stack
    ///
    ///
    ///
    ///
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<UI_Entity>> ui_elements;
    //std::vector<UI_Button*> gui_objects;
    Camera2D cam;
    Camera2D ui_cam;
    
    Screen();

    
    virtual ~Screen()=default;
    void SceneLoaded();
    virtual void Init();
    
    
   


    virtual void addEntity(std::unique_ptr<Entity> e)
    {
        entities.push_back(std::move(e));
    }
    virtual void addUi(std::unique_ptr<UI_Entity> ui_e)
    {
        ui_elements.push_back(std::move(ui_e));
    }
    
    virtual void Update(float dt)
    {

        
        for(auto& entity : entities)
        {
            entity->Update(dt);
        }
        
        
        for(auto& ui : ui_elements)
        {
            if(ui)
                ui->Update(dt);
        }
        
       // std::cout << ui_elements.size() << std::endl;
        
        if(alpha <= 0)return;;
        alpha -= dt * 5;
        
    }
   virtual void Draw()
    {
       // fadein screen
       
       BeginMode2D(cam);
       for(auto& entity : entities)
       {
           entity->Draw();
       }
       
       EndMode2D();
       
       BeginMode2D(ui_cam);
       // ui elements drawing above cameras and do not depend on camera movement
       for(auto& ui : ui_elements)
       {
           ui->Draw();
       }
       
       if(Global::debug)
           DrawCircle(Global::mousePos.x, Global::mousePos.y, 2, RED);
       EndMode2D();
       
       //for (int i = 0; i < gui_objects.size(); i++) {
         //  gui_objects[i]->Draw();
       //}
       if(Global::debug)
        DrawText(TextFormat("mouse pos :[%i , %i]", (int)Global::mousePos.x, (int)Global::mousePos.y), 10, 10, 10, YELLOW);
       
       //fade screen
       //DrawRectangle(0, 0, 1024, 512, Fade(BLACK, alpha));
    }
    
    virtual void Unload()
    {
        

        entities.clear();

        
    }
    
    Entity* getEntityByName(const std::string& name)
    {
        auto it = std::find_if(entities.begin(), entities.end(), [&name](const std::unique_ptr<Entity>& e)
                               {
            return e->name == name;
        });
        
        return (it != entities.end()) ? it->get() : nullptr;
    }
    
};
#endif /* Screen_h */
