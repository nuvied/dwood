//
//  Screen.h
//  raygame2
//
//  Created by apple on 04/02/2025.
//

#ifndef Screen_h
#define Screen_h

#include <vector>
#include "Entity.h"
#include "RenderSettings.h"

#include <algorithm>
#include "LensManager.h"




class Screen
{
private:
    float alpha = 1;
    int renderWidth = 1024;
    int renderHeight = 512;
    int shaderRenderSizeLoc = -1;
    int lightPositionLocations[RenderSettings::MaxShaderLights]{};
    int lightRadiusLocations[RenderSettings::MaxShaderLights]{};
    int lightIntensityLocations[RenderSettings::MaxShaderLights]{};
    int lightColorLocations[RenderSettings::MaxShaderLights]{};
    bool lightLimitWarningLogged = false;

public:
    Shader shader{};
    RenderTexture2D screen_target{};
    std::vector<Light> lights;
    int lightCountLoc;
    int lightCount;

    bool active = false;        /// use to set active scene in the stack
    int ligthPositionLoc;
    Vector2 lightPos;
    LensManager lm;
    ///
    ///
    ///
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<UI_Entity>> ui_elements;
    std::vector<std::unique_ptr<Entity>> lenseOnly;
    //std::vector<UI_Button*> gui_objects;
    Camera2D cam{};
    Camera2D ui_cam{};
    
    Screen();

    
    virtual ~Screen()=default;
    void SceneLoaded();
    virtual void Init();
    void InitLighting();
    void UnloadLighting();
    void DrawLighting(const std::vector<std::unique_ptr<Entity>>& roots,
                      const Camera2D& camera, bool preview);
    void SetRenderResolution(int width, int height);
    void SetRenderScale(float scale);


   


    virtual void addEntity(std::unique_ptr<Entity> e);

    virtual void addUi(std::unique_ptr<UI_Entity> ui_e);


    virtual void addLensOnly(std::unique_ptr<Entity> e);

    
    virtual void Update(float dt);
    bool TriggerHotspotAt(Vector2 point);

    virtual void DrawTexture();
    
    virtual void Draw();

    
    virtual void Unload();


    
    Entity* getEntityByName(const std::string& name);

    
};
#endif /* Screen_h */
