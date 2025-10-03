#include "Screen.h"
#include "Component.h"
#include "InvItem_Entity.h"

Screen::Screen()
{
    
    alpha = 1;
    cam = {0};
    cam.target  = {0,0};
    cam.offset = {0,0};
    cam.zoom = 2.0f;
    
    ui_cam.zoom = 2.0f;
    

    
    
    
}

void Screen::SceneLoaded()
{
    for(auto& e:entities)
    {
        auto b = e->getComponent<Behaviour>();
        if(b)
        {
            b->SceneLoaded();
        }

        if(e->childs.size() < 1)return;

        for(auto& child:e->childs)
        {
            auto c = e->getComponent<Behaviour>();
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
    shader = LoadShader(0,"shaders/simple_shader.fs");
    ligthPositionLoc = GetShaderLocation(ResourcesLoader::lenseShader,"lightPos");

    lightCountLoc = GetShaderLocation(shader, "light_count");
    SetShaderValue(shader, lightCountLoc,&lightCount, SHADER_UNIFORM_INT);

    
    for (int i = 0; i < lightCount; i++)
    {
        /* code */
        lights[i].positionLoc = GetShaderLocation(shader, TextFormat("lights[%i].pos", i));
        lights[i].radiusLoc = GetShaderLocation(shader, TextFormat("lights[%i].radius", i));
        lights[i].intensityLoc = GetShaderLocation(shader, TextFormat("lights[%i].intensity", i));

 
    }

    screen_target = LoadRenderTexture(1024,512);
    


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
        entity->Draw();
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

    if(lightCount > 0)
    {
        if(Global::lensOn)
            lights[lightCount - 1].position = Global::lensPosition;
        else
            lights[lightCount - 1].position = {5000,5000};
    }
    for (int i = 0; i < lightCount; i++)
    {
        /* code */
        SetShaderValue(shader, lights[i].positionLoc, &lights[i].position, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, lights[i].radiusLoc, &lights[i].radius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader,lights[i].intensityLoc,  &lights[i].intensity, SHADER_UNIFORM_FLOAT);
        
    }
   BeginShaderMode(shader);
    if(lightCount> 0)
        DrawRectangle(0,0,1024,512,BLACK);

        //DrawTexturePro(screen_target.texture,{0,0,(float)screen_target.texture.width,-(float)(screen_target.texture.height)},{0,0,1024,512},{0,0},0,WHITE);

   EndShaderMode();




   
   
    for(int i = 0; i < lights.size(); i++)
    {
        Light l = lights[i];
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(ResourcesLoader::light_page, l.shapeInPage,{l.position.x - l.shapeInPage.width, l.position.y - l.shapeInPage.height, l.shapeInPage.width * 2.0f, l.shapeInPage.height * 2.0f},{ l.x_offset,  l.y_offset},0.0f,Fade(l.lightColor, l.intensity - 1.0f));
        EndBlendMode();
        if(Global::debug)
        {
            
            DrawCircle(l.position.x,l.position.y, l.radius * 100.0f, Fade(l.lightColor, l.intensity));
            DrawText(TextFormat("LIGHT %f", l.intensity), l.position.x, l.position.y, 10, RED);
        }
    }
    //DrawTexturePro(screen_target.texture,{0,0,(float)screen_target.texture.width,-(float)(screen_target.texture.height)},{0,0,1024,512},{0,0},0,WHITE);

   
    
    //EndBlendMode();
    
//  
//  begin shader heere

// lightPos = GetMousePosition();
// SetShaderValue(ResourcesLoader::lenseShader, ligthPositionLoc, &lightPos, SHADER_UNIFORM_VEC2);



// end shader here
    BeginMode2D(ui_cam);
    // ui elements drawing above cameras and do not depend on camera movement
    for(auto& ui : ui_elements)
    {
        ui->Draw();
    }
    
    if(Global::debug)
        DrawCircle(Global::mousePos.x, Global::mousePos.y, 2, RED);
   
    
    //for (int i = 0; i < gui_objects.size(); i++) {
      //  gui_objects[i]->Draw();
    //}

    
    EndMode2D();

    if(Global::debug)
     DrawText(TextFormat("mouse pos :[%i , %i]", (int)Global::mousePos.x, (int)Global::mousePos.y), 10, 10, 10, YELLOW);
    
    //fade screen
    //DrawRectangle(0, 0, 1024, 512, Fade(BLACK, alpha));
 }

 void Screen::Unload()
 {
        

    entities.clear();
    if(shader.id > 0)
        UnloadShader(shader);
    
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
