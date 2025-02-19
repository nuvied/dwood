//
//  BoatScreen.h
//  raygame2
//
//  Created by apple on 05/02/2025.
//

#ifndef BoatScreen_h
#define BoatScreen_h
#include "Global.h"
#include "Component.h"
#include "Screen.h"



class BoatScreen:public Screen
{
private:
    
public:
    
    BoatScreen()
    {
        
       // gameObjects.push_back(bg);
        Init();
        cam.zoom = 2.0f;
    }
    ~BoatScreen()
    {
        //sScreen::Unload();
    }
    void Init()override
    {
        auto bg = std::make_unique<Entity>("bg");
        
        Sprite sp =  Sprite(ResourcesLoader::boat_tex_page, {512,0,512,256});
        bg->addComponent<TransformComp>(TransformComp(0,0));
        bg->addComponent<Sprite>(sp);
        addEntity(std::move(bg));
        
        auto hotspot = std::make_unique<Entity>("Hotspot_boat");
        //hotspot->addComponent<TransformComp>(TransformComp(14,0));
        hotspot->addComponent<ColliderComp>(ColliderComp(77,141,260,80));
        hotspot->addComponent<OnBoat_script>();
        
        auto hotspot_hut = std::make_unique<Entity>("Hotspot_hut");
        //hotspot->addComponent<TransformComp>(TransformComp(14,0));
        hotspot_hut->addComponent<ColliderComp>(ColliderComp(440,30,80,160));
        hotspot_hut->addComponent<On_PuzzleHut>();
        
        addEntity(std::move(hotspot));
        addEntity(std::move(hotspot_hut));

    }
    void Unload() override
    {
        Screen::Unload();
    }
};

class BoatPopup:public Screen
{
public:
    BoatPopup()
    {
        Init();
    }
    
    void Init()
    {
        auto popup = std::make_unique<Entity>("boat_popup");
        
        popup->addComponent<TransformComp>(TransformComp(128, 40));
        auto popup_sprite = Sprite(ResourcesLoader::boat_tex_page, {256,512,256,167});
        popup->addComponent<Sprite>(popup_sprite);
        addEntity(std::move(popup));
        
        auto close_btn = std::make_unique<Entity>("Close button");
        close_btn->addComponent<TransformComp>(TransformComp(345, 42));
        close_btn->addComponent<ColliderComp>(ColliderComp(345, 42, 40, 40));
        close_btn->addComponent<ClosePopup>();
        addEntity(std::move(close_btn));
        
        std::cout << "boat scene" <<std::endl;
    }
    
};

class PuzzlePopup:public Screen
{
public:
    PuzzlePopup()
    {
        Init();
    }
    void Init()
    {
        auto popup = std::make_unique<Entity>("puzzle");
        
        popup->addComponent<TransformComp>(TransformComp(90, 20));
        auto popup_sprite = Sprite(ResourcesLoader::boat_tex_page, {512,256,333,216});
        popup->addComponent<Sprite>(popup_sprite);
        addEntity(std::move(popup));
        
        auto close_btn = std::make_unique<Entity>("Close button");
        //close_btn->addComponent<TransformComp>(TransformComp(385, 25));
        close_btn->addComponent<ColliderComp>(ColliderComp(375, 25, 40, 40));
        close_btn->addComponent<ClosePopup>();
        addEntity(std::move(close_btn));
        
        auto rotor = std::make_unique<Entity>("rotor");
        rotor->addComponent<TransformComp>(TransformComp(252, 128));
        auto rotor_spr = Sprite(ResourcesLoader::boat_tex_page,{256,679,187,187}, (Vector2){93,93});
        //rotor_spr.setOrigin(93,93);
        rotor->addComponent<Sprite>(rotor_spr);
        rotor->addComponent<CircleCol>(CircleCol(0,0,93.5f));
        rotor->addComponent<Rotor_script>();
        
        addEntity(std::move(rotor));
        
        
        
        std::cout << "puzzle" <<std::endl;
    }
};



#endif /* BoatScreen_h */
