//
//  MainScreen.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef MainScreen_h
#define MainScreen_h

#include "Component.h"
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
        auto play_btn = std::make_unique<UI_Entity>("play_button");
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
        
        addUi(std::move(play_btn));
        
        std::cout << "main screen loaded" <<std::endl;
        //t->position = {100,100};
    }
    /*
    void Update(float dt)override
    {
        Screen::Update(dt);
    }
    
    void Draw()override
    {
        Screen::Draw();
    }
    */
   
    
    void Unload() override
    {
        Screen::Unload();
    }
    
    
    
};


#endif /* MainScreen_h */

