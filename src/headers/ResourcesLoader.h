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
    static Music bg_music;
    
    
    static void Init()
    {
        boat_tex_page = LoadTexture("resource/atlas_1.png");
        ui_page = LoadTexture("resource/ui_page.png");
        inv_items = LoadTexture("resource/inv_items.png");
        bg_music = LoadMusicStream("resource/bg_music.mp3");
    }
};



#endif /* ResourcesLoader_h */
