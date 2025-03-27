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
#include "LensManager.h"




class Screen
{
private:
    float alpha = 1;

public:
    bool active = false;        /// use to set active scene in the stack
    int ligthPositionLoc;
    Vector2 lightPos;
    LensManager lm;
    ///
    ///
    ///
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<UI_Entity>> ui_elements;
    std::vector<std::unique_ptr<Entity>> lenseOnly;
    //std::vector<UI_Button*> gui_objects;
    Camera2D cam;
    Camera2D ui_cam;
    
    Screen();

    
    virtual ~Screen()=default;
    void SceneLoaded();
    virtual void Init();


   


    virtual void addEntity(std::unique_ptr<Entity> e);

    virtual void addUi(std::unique_ptr<UI_Entity> ui_e);


    virtual void addLensOnly(std::unique_ptr<Entity> e);

    
    virtual void Update(float dt);

   virtual void Draw();

    
    virtual void Unload();


    
    Entity* getEntityByName(const std::string& name);

    
};
#endif /* Screen_h */
