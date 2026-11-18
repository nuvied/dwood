#include "Screen.h"
#include "Component.h"
#include "InvItem_Entity.h"
#include <cmath>
#include <stdexcept>

namespace
{
void TriggerSceneInteractions(Entity& entity)
{
    if (!entity.isActive())
        return;
    if (auto* interaction = entity.getComponent<InteractionComp>())
        if (interaction->runOnSceneStart)
            interaction->Trigger();
    for (auto& child : entity.childs)
        TriggerSceneInteractions(*child);
}

void AppendSceneLights(Entity& entity, const Camera2D& camera, float targetHeight,
                       std::vector<Light>& result, std::size_t capacity, bool& overflow)
{
    if (!entity.isActive() || entity.IsUiLayer())
        return;
    if (auto* component = entity.getComponent<LightComp>()) {
        if (result.size() >= capacity) {
            overflow = true;
        } else {
            const auto* transform = entity.getComponent<TransformComp>();
            const Vector2 worldPosition = transform
                ? transform->getWorldPosition() : Vector2{0, 0};
            Light light;
            light.position = {
                (worldPosition.x - camera.target.x) * camera.zoom + camera.offset.x,
                (worldPosition.y - camera.target.y) * camera.zoom + camera.offset.y
            };
            light.intensity = component->intensity;
            light.radius = component->radius * camera.zoom / targetHeight;
            light.lightColor = component->color;
            light.x_offset = 0;
            light.y_offset = 0;
            light.renderScale = camera.zoom / 2.0f;
            result.push_back(light);
        }
    }

    for (auto& child : entity.childs)
        AppendSceneLights(*child, camera, targetHeight, result, capacity, overflow);
}
}

Screen::Screen()
{
    
    alpha = 1;
    cam = {0};
    cam.target  = {0,0};
    cam.offset = {0,0};
    SetRenderResolution(1024, 512);
}

void Screen::SetRenderScale(float scale)
{
    if (!std::isfinite(scale) || scale <= 0.0f)
        throw std::invalid_argument("Screen render scale must be positive and finite.");
    cam.zoom = scale;
    ui_cam.zoom = scale;
}

void Screen::SetRenderResolution(int width, int height)
{
    if (width < 1 || width > 8192 || height < 1 || height > 8192)
        throw std::invalid_argument("Screen render dimensions must be between 1 and 8192 pixels.");

    const int renderTargetWidth = width * RenderSettings::SupersampleFactor;
    const int renderTargetHeight = height * RenderSettings::SupersampleFactor;
    RenderTexture2D resized{};
    if (screen_target.id > 0 &&
        (screen_target.texture.width != width || screen_target.texture.height != height)) {
        resized = LoadRenderTexture(width, height);
        if (!IsRenderTextureValid(resized))
            throw std::runtime_error("Unable to resize the screen render target.");
    }

    if (IsRenderTextureValid(resized)) {
        UnloadRenderTexture(screen_target);
        screen_target = resized;
    }
    renderWidth = width;
    renderHeight = height;
    const float scale = std::min(
        static_cast<float>(renderTargetWidth) / RenderSettings::AuthoredWidth,
        static_cast<float>(renderTargetHeight) / RenderSettings::AuthoredHeight);
    SetRenderScale(scale);
}

void Screen::SceneLoaded()
{
    for(auto& e:entities)
    {
        TriggerSceneInteractions(*e);
        auto b = e->getComponent<Behaviour>();
        if(b)
        {
            b->SceneLoaded();
        }

        if(e->childs.empty()) continue;

        for(auto& child:e->childs)
        {
            auto c = child->getComponent<Behaviour>();
            if(c)
            {
                c->SceneLoaded();
            }
        }
    }
}

void Screen::Init()
{
    SceneLoaded();
    Light lenseLight = Light();
    lenseLight.radius = 0.5f;
     //lenseLight.position = {5000,5000};
     lenseLight.intensity = 1.0f;
    lights.push_back(lenseLight);
    lightCount = lights.size();
    InitLighting();
    ligthPositionLoc = GetShaderLocation(ResourcesLoader::lenseShader,"lightPos");

    screen_target = LoadRenderTexture(renderWidth, renderHeight);
}

void Screen::InitLighting()
{
    shader = LoadShader(0,"shaders/simple_shader.fs");
    if (!IsShaderValid(shader))
        throw std::runtime_error("Unable to load shaders/simple_shader.fs.");
    lightCountLoc = GetShaderLocation(shader, "light_count");
    shaderRenderSizeLoc = GetShaderLocation(shader, "renderSize");
    for (int i = 0; i < RenderSettings::MaxShaderLights; ++i) {
        lightPositionLocations[i] =
            GetShaderLocation(shader, TextFormat("lights[%i].pos", i));
        lightRadiusLocations[i] =
            GetShaderLocation(shader, TextFormat("lights[%i].radius", i));
        lightIntensityLocations[i] =
            GetShaderLocation(shader, TextFormat("lights[%i].intensity", i));
        lightColorLocations[i] =
            GetShaderLocation(shader, TextFormat("lights[%i].color", i));
    }
    if (lightCountLoc < 0 || shaderRenderSizeLoc < 0 ||
        lightPositionLocations[0] < 0 || lightRadiusLocations[0] < 0 ||
        lightIntensityLocations[0] < 0 || lightColorLocations[0] < 0) {
        UnloadLighting();
        throw std::runtime_error(
            "shaders/simple_shader.fs is outdated or missing required uniforms. Rebuild the editor.");
    }
    lightCount = 0;
    SetShaderValue(shader, lightCountLoc, &lightCount, SHADER_UNIFORM_INT);
}

void Screen::UnloadLighting()
{
    if (shader.id > 0) {
        UnloadShader(shader);
        shader = {};
    }
}
void Screen::addEntity(std::unique_ptr<Entity> e)
{
    entities.push_back(std::move(e));
}
void Screen::addUi(std::unique_ptr<UI_Entity> ui_e)
{
    ui_elements.push_back(std::move(ui_e));
}

void Screen::addLensOnly(std::unique_ptr<Entity> e)
{
    lenseOnly.push_back(std::move(e));
}

void Screen::Update(float dt)
{
    if(Global::lensOn) {
        lightPos = Global::lensPosition;
        SetShaderValue(ResourcesLoader::lightOnly, ligthPositionLoc, &lightPos,
                       SHADER_UNIFORM_VEC2);
        return;
    }
    for(auto& entity : entities)
    {
        entity->Update(dt);
    }
    if (!Game::get_Instance().IsOverUI && Global::mousePos.x >= 0 &&
        Global::mousePos.y >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        TriggerHotspotAt(Global::mousePos);

    for(auto& e: lenseOnly)
    {
        if(e)
        {
            e->Update(dt);
        }
    }
    
    for(auto& ui : ui_elements)
    {
        if(ui)
            ui->Update(dt);
    }
    
   // std::cout << ui_elements.size() << std::endl;
    
    if(alpha <= 0)return;;
    alpha -= dt * 5;


    
    
}

namespace
{
bool TriggerHotspot(Entity& entity, Vector2 point, bool ui)
{
    if (!entity.isActive())
        return false;
    for (auto it = entity.childs.rbegin(); it != entity.childs.rend(); ++it)
        if (TriggerHotspot(**it, point, ui))
            return true;
    auto* hotspot = entity.getComponent<HotspotComp>();
    if (entity.IsUiLayer() == ui && hotspot && hotspot->Contains(point)) {
        if (hotspot->interaction)
            hotspot->interaction->Trigger();
        return true;
    }
    return false;
}
}

bool Screen::TriggerHotspotAt(Vector2 point)
{
    for (bool ui : {true, false})
        for (auto it = entities.rbegin(); it != entities.rend(); ++it)
            if (TriggerHotspot(**it, point, ui))
                return true;
    return false;
}

void Screen::DrawTexture()
{
    // BeginTextureMode(screen_target);
    // ClearBackground(Fade(BLACK, 1));



    // for(auto l:lights)
    // {
        
    //    // DrawTexturePro(l.texPage, l.shapeInPage, {l.position.x /2.0f, l.position.y/2.0f, l.shapeInPage.width, l.shapeInPage.height},{l.x_offset, l.y_offset}, 0, l.lightColor);
    //    // DrawTexturePro(l.texPage, l.shapeInPage,{l.position.x - l.shapeInPage.width, l.position.y - l.shapeInPage.height, l.shapeInPage.width * 2.0f, l.shapeInPage.height * 2.0f},{ l.x_offset,  l.y_offset},0.0f,Fade(l.lightColor, l.intensity));
    //     if(Global::debug)
    //     {
    //         DrawText("light ", l.position.x /2.0f, l.position.y / 2.0f, 10, YELLOW);
    //     }
    // }
    // EndTextureMode();

}

void Screen::Draw()
{
    // fadein screen
    
    BeginMode2D(cam);


    for(auto& entity : entities)
    {
       // BeginShaderMode(shader);
        entity->DrawLayer(false);
        //EndShaderMode();
    }
    if(Global::lensOn) {

        lm.Update();
        lm.Draw();
        
        for (auto &e: lenseOnly) {
            if (e) {
                BeginShaderMode(ResourcesLoader::lightOnly);
                e->Draw();
                EndShaderMode();
            }
        }
        
    
    }
    //DrawTexturePro(screen_target.texture,{0,0,(float)screen_target.texture.width,-(float)(screen_target.texture.height)},{0,0,1024,512},{0,0},0,WHITE);
    //DrawTexturePro(screen_target.texture,{0,0,(float)screen_target.texture.width,-(float)(screen_target.texture.height)},{0,0,1024,512},{0,0},0,WHITE);
    EndMode2D();

    DrawLighting(entities, cam, false);

    BeginMode2D(ui_cam);
    for (auto& entity : entities)
        entity->DrawLayer(true);
    for (auto& ui : ui_elements)
        ui->Draw();
    if (Global::debug)
        DrawCircle(Global::mousePos.x, Global::mousePos.y, 2, RED);
    EndMode2D();

    if (Global::debug)
        DrawText(TextFormat("mouse pos :[%i , %i]", (int)Global::mousePos.x,
                            (int)Global::mousePos.y), 10, 10, 10, YELLOW);
}

void Screen::DrawLighting(const std::vector<std::unique_ptr<Entity>>& roots,
                          const Camera2D& camera, bool preview)
{
    const float targetHeight = static_cast<float>(GetRenderHeight());
    const std::size_t lensLightCount = preview || lights.empty() ? 0 : 1;
    const std::size_t fixedLightCount = preview ? 0 : lights.size() - lensLightCount;
    const std::size_t sceneLightCapacity = preview ? RenderSettings::MaxSceneLights :
        RenderSettings::MaxShaderLights - lensLightCount;
    std::vector<Light> activeLights;
    activeLights.reserve(RenderSettings::MaxShaderLights);
    bool lightOverflow = fixedLightCount > sceneLightCapacity;
    const std::size_t copiedFixedLights = std::min(fixedLightCount, sceneLightCapacity);
    const float legacyScale = camera.zoom / 2.0f;
    for (std::size_t i = 0; i < copiedFixedLights; ++i) {
        Light light = lights[i];
        light.position.x *= legacyScale;
        light.position.y *= legacyScale;
        light.renderScale = legacyScale;
        activeLights.push_back(light);
    }
    const std::size_t sceneLightLimit = sceneLightCapacity;
    for (auto& entity : roots)
        AppendSceneLights(*entity, camera, targetHeight, activeLights, sceneLightLimit, lightOverflow);
    if (lensLightCount > 0) {
        Light lensLight = lights.back();
        if (Global::lensOn) {
            lensLight.position = {
                (Global::lensPosition.x - cam.target.x) * cam.zoom + cam.offset.x,
                (Global::lensPosition.y - cam.target.y) * cam.zoom + cam.offset.y
            };
        } else {
            lensLight.position = {targetHeight * 10.0f, targetHeight * 10.0f};
        }
        lensLight.renderScale = legacyScale;
        activeLights.push_back(lensLight);
    }
    if (lightOverflow && !lightLimitWarningLogged) {
        TraceLog(LOG_WARNING, "Scene has more lights than the shader limit of %d.",
                 RenderSettings::MaxShaderLights);
        lightLimitWarningLogged = true;
    }

    lightCount = static_cast<int>(activeLights.size());
    SetShaderValue(shader, lightCountLoc, &lightCount, SHADER_UNIFORM_INT);
    const Vector2 renderSize{static_cast<float>(GetRenderWidth()), targetHeight};
    SetShaderValue(shader, shaderRenderSizeLoc, &renderSize, SHADER_UNIFORM_VEC2);
    for (int i = 0; i < lightCount; ++i) {
        SetShaderValue(shader, lightPositionLocations[i], &activeLights[i].position,
                       SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, lightRadiusLocations[i], &activeLights[i].radius,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, lightIntensityLocations[i], &activeLights[i].intensity,
                       SHADER_UNIFORM_FLOAT);
        const Vector3 color = {
            activeLights[i].lightColor.r / 255.0f,
            activeLights[i].lightColor.g / 255.0f,
            activeLights[i].lightColor.b / 255.0f
        };
        SetShaderValue(shader, lightColorLocations[i], &color, SHADER_UNIFORM_VEC3);
    }
    BeginShaderMode(shader);
    if (lightCount > 0 || preview)
        DrawRectangle(0, 0, GetRenderWidth(), GetRenderHeight(), BLACK);

        //DrawTexturePro(screen_target.texture,{0,0,(float)screen_target.texture.width,-(float)(screen_target.texture.height)},{0,0,1024,512},{0,0},0,WHITE);

   EndShaderMode();




   
   
    for (const auto& light : activeLights)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(ResourcesLoader::light_page, light.shapeInPage,
                       {light.position.x - light.shapeInPage.width * light.renderScale,
                        light.position.y - light.shapeInPage.height * light.renderScale,
                        light.shapeInPage.width * 2.0f * light.renderScale,
                        light.shapeInPage.height * 2.0f * light.renderScale},
                       {light.x_offset * light.renderScale,
                        light.y_offset * light.renderScale}, 0.0f,
                       Fade(light.lightColor,
                            std::clamp(light.intensity - 1.0f, 0.0f, 1.0f)));
        EndBlendMode();
        if(Global::debug)
        {
            DrawCircle(light.position.x, light.position.y,
                       light.radius * targetHeight, Fade(light.lightColor, light.intensity));
            DrawText(TextFormat("LIGHT %f", light.intensity), light.position.x,
                     light.position.y, 10, RED);
        }
    }
    //DrawTexturePro(screen_target.texture,{0,0,(float)screen_target.texture.width,-(float)(screen_target.texture.height)},{0,0,1024,512},{0,0},0,WHITE);

   
    
    //EndBlendMode();
    
//  
//  begin shader heere

// lightPos = GetMousePosition();
// SetShaderValue(ResourcesLoader::lenseShader, ligthPositionLoc, &lightPos, SHADER_UNIFORM_VEC2);



// end shader here
 }

 void Screen::Unload()
 {
        

    entities.clear();
    UnloadLighting();
    
    if(screen_target.id > 0)
    {
        UnloadRenderTexture(screen_target);
    }


    
}

Entity *Screen::getEntityByName(const std::string &name)
{
    auto it = std::find_if(entities.begin(), entities.end(), [&name](const std::unique_ptr<Entity>& e)
                           {
        return e->name == name;
    });
    
    return (it != entities.end()) ? it->get() : nullptr;
}
