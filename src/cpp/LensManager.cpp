//
// Created by Naveed on 3/9/2025.
//
#include "LensManager.h"
#include "Game.hpp"
#include "ResourcesLoader.h"
#include "Global.h"
#include <iostream>

void LensManager::Init()
{
    lensePosLoc = GetShaderLocation(ResourcesLoader::lenseShader, "lensePos");
    renderSizeLoc = GetShaderLocation(ResourcesLoader::lenseShader, "renderSize");
    lightPosLoc = GetShaderLocation(ResourcesLoader::lightOnly, "lightPos");
    lightRenderSizeLoc = GetShaderLocation(ResourcesLoader::lightOnly, "renderSize");
}


void LensManager::Update()
{
    if (!Global::lensOn)
        return;

    auto& game = Game::get_Instance();
    const float scale = game.RenderScale();
    lensPos = {Global::lensPosition.x * scale, Global::lensPosition.y * scale};
    const Vector2 renderSize = {
        static_cast<float>(game.RenderTargetWidth()),
        static_cast<float>(game.RenderTargetHeight())
    };
    SetShaderValue(ResourcesLoader::lenseShader, lensePosLoc, &lensPos, SHADER_UNIFORM_VEC2);
    if (renderSizeLoc >= 0)
        SetShaderValue(ResourcesLoader::lenseShader, renderSizeLoc, &renderSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(ResourcesLoader::lightOnly, lightPosLoc, &lensPos, SHADER_UNIFORM_VEC2);
    if (lightRenderSizeLoc >= 0)
        SetShaderValue(ResourcesLoader::lightOnly, lightRenderSizeLoc, &renderSize, SHADER_UNIFORM_VEC2);
}

void LensManager::Draw()
{
    BeginShaderMode(ResourcesLoader::lenseShader);

    DrawRectangle(0, 0, GetRenderWidth(), GetRenderHeight(), WHITE);// color of this rect come from shader
    EndShaderMode();
}