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

    TransformComp* transform;

    void Init()override;

    void OnMouseDown() override;

    void Draw()override;

};

class OnBoat_script:public Behaviour
{
public:

    
    void OnMouseDown()override;

};


class On_PuzzleHut:public Behaviour
{
public:

   
    void OnMouseDown()override;
    
};

class Rotor_script:public Behaviour{
    
public:
    TransformComp* t;
    float correct_order[5] = {45,180,225,0,90};
    int current_idx = 0;
    
private:
    bool held;
    float offset_rotation;

    void Init()override;


    
    void OnMouseDown()override;

    
    void CheckforAngle();

    void ResetColor();
   
    void SetColor(int i);


    void Complete_puzzle();

    
    void Update(float dt) override;

    
    float SnapAngle(float angle, float step) {
        return round(angle / step) * step;
    }
    
    float AngleLerp(float current, float target, float t) {
        float diff = fmod(target - current + 540, 360) - 180; // Keep angle between -180 and 180
        return current + diff * t;
    }

    void Draw() override;

    
};



class ClosePopup:public Behaviour
{
public:
    void OnMouseDown()override;

};
class Slot_script:public Behaviour
{
    public:
        int idx;
        TransformComp *transform;
        Sprite* sp;
        Item slot_item;
        bool selected = false;

    void Init();



    void OnMouseDown()override;


    void Draw()override;


    void UpdateSlot();


};


class OnInvButton:public Behaviour
{

      
private:
    bool invShow = true; 
    TransformComp* inv_transform;  


public:
    Entity* inventory;


    void Start()override;


    void onEnable()override;

    void OnMouseDown()override;


    void Update(float dt)override;



};


class InventoryManager:public Behaviour
{
    public:
    std::vector<Slot_script*> slots;

    void selectItemAt(int idx);

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
  
    InvItem_script(std::string name, Vector2 pos = {0});

    InvItem_script(int id, Vector2 pos = {0});

    void Init() override;

    void OnMouseDown()override;
 
};


class LensButtonScript:public Behaviour
{
public:

    void Init()override{}


    void OnMouseDown()override;

};

class On_oarPlacement:public Behaviour
{

    void OnMouseDown() override;


};

class DrawBg:public Behaviour
{
private:
    TransformComp* transform;
public:
    int w, h;
    DrawBg(int w, int h);


    void Init() override;

    void Draw() override;

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

    DrawTextComp(int x, int y, int size);

    void Init() override;




    void Update(float dt) override;


    void Draw() override;




    void SetText(std::string sub, float t);

};

class Craft_btn_script:public Behaviour
{
private:
    bool enabled = false;
public:
    

    void Init()override{}

    void OnMouseDown()override;


};
#endif