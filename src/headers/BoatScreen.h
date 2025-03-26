//
//  BoatScreen.h
//  raygame2
//
//  Created by apple on 05/02/2025.
//

#ifndef BoatScreen_h
#define BoatScreen_h
#include "Global.h"
#include "Screen.h"


#include "ActionManager.h"


class BoatScreen:public Screen
{
private:
    ActionList al;
public:

    BoatScreen();

    ~BoatScreen()
    {
        //sScreen::Unload();
    }
    void Init()override;

    void Update(float dt);
    void Unload() override;
};




class BoatPopup:public Screen
{
public:

    int lightPosLoc;
    Vector2 lightPos;
    BoatPopup();

    
    void Init();
    

    void Draw()override;

    
};

class PuzzlePopup:public Screen
{
public:

    Color default_Color;
    PuzzlePopup();
    void Init();

};


class Hut_interior_popup:public Screen
{
    public:
    Hut_interior_popup();


    void Init();
    
};
#endif /* BoatScreen_h */
