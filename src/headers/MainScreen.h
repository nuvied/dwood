//
//  MainScreen.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef MainScreen_h
#define MainScreen_h


#include "Custom_behaviours.h"
#include "Screen.h"




class MainScreen:public Screen
{
    Vector2 pos;
public:

    
    MainScreen()
    {
        
        Init();
    }
    ~MainScreen()
    {
        //Unload();
    }
    void Init() override
    {
        
        //Texture2D tex = ResourcesLoader::boat_tex_page;
        auto play_btn = std::make_unique<Entity>("play_button");
        Sprite sp = Sprite(ResourcesLoader::ui_page, {0,0,64,32});
        Sprite sp2 = Sprite(ResourcesLoader::boat_tex_page, {0,0,512,256});
        play_btn->addComponent<TransformComp>(TransformComp(400, 120));
        play_btn->addComponent<Sprite>(sp);
        play_btn->addComponent<ColliderComp>(ColliderComp(0, 0, 0, 0));
        play_btn->addComponent<PlayButton_script>();


        auto bg = std::make_unique<Entity>("bg");
        bg->addComponent<TransformComp>(TransformComp(0, 0));
        bg->addComponent<Sprite>(sp2);
        // add entity to the scene

      
        addEntity(std::move(bg));
        
        //addUi(std::move(play_btn));
        addEntity(std::move(play_btn));
        
        std::cout << "main screen loaded" <<std::endl; 
        lights.clear();
        Light l = Light(1.0f,0.8f,{900,200});
        Light l2 = Light(1.0,0.8f,{100,150});
        Light l3 = Light(1.0,0.8f,{500,150});
        
        lights.push_back(l);
        lights.push_back(l2);
        lights.push_back(l3);

        Screen::Init();


    }
    
    void Update(float dt)override
    {
        Screen::Update(dt);
    }
    
    void Draw()override
    {
        Screen::Draw();      
    }
    
   
    
    void Unload() override
    {
        Screen::Unload();
    }
    
    
    
};


#endif /* MainScreen_h */

