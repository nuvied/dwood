//
// Created by Naveed on 3/9/2025.
//
#include "LensManager.h"
#include "ResourcesLoader.h"
#include "Global.h"
#include <iostream>

void LensManager::Init()
{
    lensePosLoc = GetShaderLocation(ResourcesLoader::lenseShader, "lensePos");
}


void LensManager::Update()
{

    Vector2 mousePos = Global::lensPosition;

    //lensPos = {mousePos.x / 2.0f, mousePos.y/2.0f};
    lensPos = mousePos;
    SetShaderValue(ResourcesLoader::lenseShader,lensePosLoc, &lensPos, SHADER_UNIFORM_VEC2);
}

void LensManager::Draw()
{
    BeginShaderMode(ResourcesLoader::lenseShader);

    DrawRectangle(0,0,1024,512,WHITE);// color of this rect come from shader
    EndShaderMode();
}