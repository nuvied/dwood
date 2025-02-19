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
    
    static void Init()
    {
        boat_tex_page = LoadTexture("resources/atlas_1.png");
        ui_page = LoadTexture("resources/ui_page.png");
    }
};



#endif /* ResourcesLoader_h */
