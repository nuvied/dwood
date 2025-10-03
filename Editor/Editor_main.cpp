#include "raylib.h"
#include "rlImGui.h"

#include "imgui.h"

#include "Game.hpp"
#include "HouseFrontScreen.h"


Vector2 Global::mousePos = {0,0};
Vector2  Global::lensPosition = {512,256};

Texture2D ResourcesLoader::boat_tex_page;
Texture2D ResourcesLoader::ui_page;
Texture2D ResourcesLoader::inv_items;
Texture2D ResourcesLoader::light_blob0;
Texture2D ResourcesLoader::light_page;

Shader ResourcesLoader::basic_shader;
Shader  ResourcesLoader::lenseShader;
Shader ResourcesLoader::lightOnly;
Shader ResourcesLoader::glowShader;


bool Global::debug = false;
Music ResourcesLoader::bg_music;
bool Global::rotor_puzzle_done = false;
bool Global::oar_placed = false;
bool Global::lensOn = false;
int Global::selectedItemId = 0;

int main()
{
    InitWindow(1280,720, "Game Editor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    RenderTexture2D target = LoadRenderTexture(1024, 512);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    bool isPlaying = false;
    rlImGuiSetup(true);
    Game::get_Instance().Init();
    ResourcesLoader::Init();


    HouseFrontScreen houseScreen;
    houseScreen.Init();



    while (!WindowShouldClose())
    {
        if(isPlaying){
            //boatScreen.Update(GetFrameTime());
            houseScreen.Update(GetFrameTime());
            houseScreen.Draw();
             Game::get_Instance().Update(GetFrameTime());

             Global::mousePos = {GetMousePosition().x / 2.0f, GetMousePosition().y / 2.0f};
             
        }

        BeginTextureMode(target);
            ClearBackground(DARKBLUE); // Game background (blue)
           
            
            
       // Game::get_Instance().Draw();
       
        EndTextureMode();



    BeginDrawing();
        ClearBackground(BLACK); // Fill unused areas with black (letterbox effect)
        

        if(isPlaying)
        {
            DrawTextureRec(target.texture, {0,0,1024,-512},{0,0},WHITE);
        }



       
        rlImGuiBegin();



        ImGui::Begin("Control");
        if(ImGui::Button("PLAY"))
        {
            isPlaying = !isPlaying;
        }
        ImGui::End();

        ImGui::Begin("Scene Objects");
        for(auto& e:houseScreen.entities)
        {
            ImGui::Text(e->name.c_str());
        }

        ImGui::End();

        rlImGuiEnd();
        
        EndDrawing();
    }
    CloseWindow();
    rlImGuiShutdown();
    Game::get_Instance().Unload();
    UnloadTexture(ResourcesLoader::boat_tex_page);
    UnloadTexture(ResourcesLoader::ui_page);
    UnloadMusicStream(ResourcesLoader::bg_music);
    UnloadTexture(ResourcesLoader::inv_items);
    UnloadShader(ResourcesLoader::lenseShader);
    UnloadShader(ResourcesLoader::lightOnly);
    UnloadShader(ResourcesLoader::basic_shader);
    UnloadTexture(ResourcesLoader::light_page);

    return 0;
}