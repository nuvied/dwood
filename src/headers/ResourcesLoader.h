//
//  ResourcesLoader.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef ResourcesLoader_h
#define ResourcesLoader_h

#include "raylib.h"

class ResourcesLoader
{
public:
    static Texture2D boat_tex_page;
    static Texture2D ui_page;
    static Texture2D inv_items;
    static Texture2D light_page;

    static Music bg_music;
    static Shader basic_shader;
    static Texture2D  light_blob0;
    static Shader lenseShader;
    static Shader lightOnly;
    static Shader glowShader;

    
    static void Init()
    {
        boat_tex_page = LoadTexture("resource/atlas_1.png");
        ui_page = LoadTexture("resource/ui_page.png");
        inv_items = LoadTexture("resource/inv_items.png");
        bg_music = LoadMusicStream("resource/bg_music.mp3");
        basic_shader = LoadShader(0,"shaders/simple_shader.fs");
        light_blob0 = LoadTexture("resource/light_blob.png");
        light_page = LoadTexture("resource/light_page.png");

        lenseShader = LoadShader(0,"shaders/LenseShader.fs");
        lightOnly = LoadShader(0,"shaders/lightOnlyShader.fs");
        glowShader = LoadShader(0, "shaders/light_glow.fs");
    }
};



#endif /* ResourcesLoader_h */
