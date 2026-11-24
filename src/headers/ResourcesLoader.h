//
//  ResourcesLoader.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef ResourcesLoader_h
#define ResourcesLoader_h

#include "raylib.h"
#include "ProjectConfig.h"
#include <string>

#include <vector>

class ResourcesLoader
{
public:
    static Texture2D boat_tex_page;
    static Texture2D ui_page;
    static Texture2D inv_items;
    static Texture2D light_page;

    static Shader basic_shader;
    static Texture2D  light_blob0;
    static Shader lenseShader;
    static Shader lightOnly;
    static Shader glowShader;

    static bool IsFontAssetName(const std::string& name);
    static std::vector<std::string> AvailableTextures();
    static std::vector<std::string> AvailableSounds();
    static const Texture2D* GetTexture(const std::string& name,
                                       std::string* error = nullptr);
    static void UnloadTextures();
    static std::vector<std::string> AvailableFonts();
    static const Font* GetCustomFont(const std::string& name, std::string* error = nullptr);
    static void ClearFontErrors();
    static void UnloadFonts();
    
    static void Init()
    {
        UnloadTextures();
        boat_tex_page = LoadTexture(AssetPaths::Resource("atlas_1.png").string().c_str());
        ui_page = LoadTexture(AssetPaths::Resource("ui_page.png").string().c_str());
        inv_items = LoadTexture(AssetPaths::Resource("inv_items.png").string().c_str());
        basic_shader = LoadShader(0, AssetPaths::Shader("simple_shader.fs").string().c_str());
        light_blob0 = LoadTexture(AssetPaths::Resource("light_blob.png").string().c_str());
        light_page = LoadTexture(AssetPaths::Resource("light_page.png").string().c_str());

        lenseShader = LoadShader(0, AssetPaths::Shader("LenseShader.fs").string().c_str());
        lightOnly = LoadShader(0, AssetPaths::Shader("lightOnlyShader.fs").string().c_str());
        glowShader = LoadShader(0, AssetPaths::Shader("light_glow.fs").string().c_str());

    }
};



#endif /* ResourcesLoader_h */
