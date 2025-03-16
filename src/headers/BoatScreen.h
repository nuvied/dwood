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
#include "Custom_behaviours.h"




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
        Sprite spr_child = Sprite(ResourcesLoader::boat_tex_page, {0,0,100,100} );
        bg->addComponent<TransformComp>(TransformComp(0,0));
        bg->addComponent<Sprite>(sp);
       
        
        auto hotspot = std::make_unique<Entity>("Hotspot_boat");
       // hotspot->addComponent<TransformComp>(TransformComp(0,0));
        hotspot->addComponent<ColliderComp>(ColliderComp(77,141,260,80));
        hotspot->addComponent<OnBoat_script>();
        
        auto hotspot_hut = std::make_unique<Entity>("Hotspot_hut");
       // hotspot->addComponent<TransformComp>(TransformComp(0,0));
        hotspot_hut->addComponent<ColliderComp>(ColliderComp(440,30,80,160));
        hotspot_hut->addComponent<On_PuzzleHut>();
        

 // adding hotspot of hut
        
        
        // auto bamboo_stick = std::make_unique<InvItem_Entity>
        // (
        //     InvItem_Entity("bamboo_stick", ResourcesLoader::boat_tex_page, {942,334,17,87})
        // );
        // bamboo_stick->addComponent<InvItem_script>(InvItem_script( -1, {361,45}));
        
        auto bamboo_stick = std::make_unique<Entity>("bamboo_stick");
        bamboo_stick->addComponent<TransformComp>();
        bamboo_stick->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {942,334,17,87}));
        bamboo_stick->addComponent<ColliderComp>();
        bamboo_stick->addComponent<InvItem_script>(InvItem_script(-1,{361,45}));
        
        auto digits = std::make_unique<Entity>("Digit");
        auto tr_digit = digits->addComponent<TransformComp>();
        digits->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {768,896,242,20},{0},RED));
        tr_digit->position = {100,50};
        // for(auto& e:entities)
        // {
        //     auto b = e->getComponent<Behaviour>();
        //     if(b)
        //     {
        //         b->Init();
        //     }
        // }
        addEntity(std::move(bg));
        addEntity(std::move(bamboo_stick));
        addEntity(std::move(hotspot)); 
        addEntity(std::move(hotspot_hut));

        addLensOnly(std::move(digits));
        Screen::Init();

    }
    void Unload() override
    {
        Screen::Unload();
    }
};




class BoatPopup:public Screen
{
public:

    int lightPosLoc;
    Vector2 lightPos;
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
        
        auto rope_in_scene = std::make_unique<Entity>("rope_in_scene");
        rope_in_scene->addComponent<TransformComp>();
        rope_in_scene->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {967,310,56,27}));
        rope_in_scene->addComponent<ColliderComp>();
        rope_in_scene->addComponent<InvItem_script>(InvItem_script(-3,{262,167}));

        addEntity(std::move(rope_in_scene));


       lightPosLoc =  GetShaderLocation(ResourcesLoader::basic_shader,"lightPos");
       lightPos = {10,10};

    }

    void Draw()override
    {
        //SetShaderValue(ResourcesLoader::basic_shader,lightPosLoc, &lightPos, SHADER_UNIFORM_VEC2);
        //lightPos = GetMousePosition();
        Screen::Draw();
//        BeginBlendMode(BLEND_MULTIPLIED);
       // BeginShaderMode(ResourcesLoader::basic_shader);
        //DrawRectangle(0,0,1024,1024,WHITE);
//        //DrawTexture(lightShape, lightPosition.x - lightShape.width/2, lightPosition.y - lightShape.height/2, WHITE);
        //EndShaderMode();
//        EndBlendMode();
        //DrawText(TextFormat("LightPos [%f, %f]", lightPos.x, lightPos.y),10,10,10,YELLOW);


    }
    
};

class PuzzlePopup:public Screen
{
public:

    Color default_Color;
    PuzzlePopup()
    {
        Init();
    }
    void Init()
    {
        default_Color = {120,120,130,255};
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
        auto rotor_spr = Sprite(ResourcesLoader::boat_tex_page,{256,679,187,187}, (Vector2){.5,.5});
        //rotor_spr.setOrigin(93,93);
        rotor->addComponent<Sprite>(rotor_spr);
        rotor->addComponent<CircleCol>(CircleCol(0,0,110.0f));
        rotor->addComponent<Rotor_script>();
        
        
        // digits setup
        auto digit_I = std::make_unique<Entity>("digit_I");
        auto t_digit_I = digit_I->addComponent<TransformComp>(TransformComp(77,-5));
        t_digit_I->scale = {.5f,.5f};
        auto digit_sp_I = Sprite(ResourcesLoader::boat_tex_page, {768, 896,7, 20},{0});
        digit_sp_I.color = default_Color;

        digit_I->addComponent<Sprite>(digit_sp_I);
        // digti 2

        auto digit_II = std::make_unique<Entity>("digit_II");
        auto t_digit_II = digit_II->addComponent<TransformComp>(TransformComp(48,-58));
        auto digit_sp_II = Sprite(ResourcesLoader::boat_tex_page, {787, 896,14, 20},{0});
        digit_sp_II.color = default_Color;
        t_digit_II->rotation = -45;
        t_digit_II->scale = {.5f,.5f};
        digit_II->addComponent<Sprite>(digit_sp_II);

        //digtit 3

        auto digit_III = std::make_unique<Entity>("digit_III");
        auto t_digit_III = digit_III->addComponent<TransformComp>(TransformComp(-5,-74));
        auto digit_sp_III = Sprite(ResourcesLoader::boat_tex_page, {812, 896,21, 20},{0});
        digit_sp_III.color = default_Color;
        t_digit_III->rotation = -90;
        t_digit_III->scale = {.5f,.5f};
        digit_III->addComponent<Sprite>(digit_sp_III);

        auto digit_IV = std::make_unique<Entity>("digit_IV");
        auto t_digit_IV = digit_IV->addComponent<TransformComp>(TransformComp(-54,-50));
        auto digit_sp_IV = Sprite(ResourcesLoader::boat_tex_page, {842, 896,24, 20},{0});
        digit_sp_IV.color = default_Color;
        t_digit_IV->rotation = -135;
        t_digit_IV->scale = {.5f,.5f};
        digit_IV->addComponent<Sprite>(digit_sp_IV);

        
        auto digtit_V = std::make_unique<Entity>("digtit_V");
        auto v_t = digtit_V->addComponent<TransformComp>(TransformComp(-75,5));
        v_t->scale = {0.5f,0.5f};
        v_t->rotation = 180;
        digtit_V->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {871,896,17,20},{0}, default_Color));

        auto digtit_VI = std::make_unique<Entity>("digti_VI");
        auto vi_t = digtit_VI->addComponent<TransformComp>(TransformComp(-48,55));
        vi_t->scale = {0.5f,0.5f};
        vi_t->rotation = 135;
        digtit_VI->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {895,896,24,20},{0}, default_Color));

        auto digtit_IX = std::make_unique<Entity>("digti_IX");
        auto ix_t = digtit_IX->addComponent<TransformComp>(TransformComp(5,72));
        ix_t->scale = {0.5f,0.5f};
        ix_t->rotation = 90;
        digtit_IX->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {969,896,22,20},{0}, default_Color));

        auto digtit_X = std::make_unique<Entity>("digti_X");
        auto x_t = digtit_X->addComponent<TransformComp>(TransformComp(55,50));
        x_t->scale = {0.5f,0.5f};
        x_t->rotation = 45;
        digtit_X->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {995,896,15,20},{0}, default_Color));





        rotor->addChild(std::move(digit_I));
        rotor->addChild(std::move(digit_II));
        rotor->addChild(std::move(digit_III));
        rotor->addChild(std::move(digit_IV));
        rotor->addChild(std::move(digtit_V));
        rotor->addChild(std::move(digtit_VI));
        rotor->addChild(std::move(digtit_IX));
        rotor->addChild(std::move(digtit_X));

        addEntity(std::move(rotor));

        // detector piece

        auto detector = std::make_unique<Entity>("detector");

        detector->addComponent<TransformComp>(TransformComp(307,90));
        detector->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {845,288,97,62}));

        addEntity(std::move(detector));
        std::cout << "puzzle" <<std::endl;
    }

};


class Hut_interior_popup:public Screen
{
    public:
    Hut_interior_popup()
    {
        Init();
    }

    void Init()
    {
        auto popup = std::make_unique<Entity>("Hut_popup");
        popup->addComponent<TransformComp>(TransformComp(128,40));
        popup->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {0,512,256,167}));
        addEntity(std::move(popup));

        auto close_btn = std::make_unique<Entity>("Close button");
        close_btn->addComponent<TransformComp>(TransformComp(345, 42));
        close_btn->addComponent<ColliderComp>(ColliderComp(345, 42, 40, 40));
        close_btn->addComponent<ClosePopup>();
        addEntity(std::move(close_btn));


        // placing inventory item
        // check if requred item is already picked

        auto oar_invItem = std::make_unique<InvItem_Entity>
        (
            InvItem_Entity("broken_oar", ResourcesLoader::boat_tex_page, {959,337,17,45})    
        );
       // oar_invItem->addComponent<TransformComp>();
        //oar_invItem->addComponent<Sprite>(Sprite(ResourcesLoader::boat_tex_page, {959,337,17,45}));
        oar_invItem->addComponent<InvItem_script>(InvItem_script(-2, {320,110})); // broken oar inv item


        addEntity(std::move(oar_invItem));
    }
};
#endif /* BoatScreen_h */
