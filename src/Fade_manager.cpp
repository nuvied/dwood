//
//  Fade_manager.cpp
//  raygame2
//
//  Created by apple on 14/02/2025.
//

#include "Fade_manager.hpp"
#include "Global.h"

void Fade_manager::StartFadeIn(float time, std::function<void()> callback )
{
    fadein = true;
    fadeout = false;
    duration = time;
    elapsed = 0.0f;
    alpha = 1.0;
    onFadeComplete = callback;
    
}
void Fade_manager::StartFadeOut(float time, std::function<void()> callback )
{
    fadein = false;
    fadeout = true;
    duration = time;
    elapsed = 0.0f;
    alpha = 1.0;
    onFadeComplete = callback;
}

void Fade_manager::Update(  float dt)
{
    if(fadein)
    {
        elapsed += dt;
        alpha = 1-(elapsed/duration);
        if(elapsed >= duration)
        {
            alpha = 0.0f;
            fadein = false;
            if(onFadeComplete) onFadeComplete();
                
        }
        
    }
    else if(fadeout)
    {
        elapsed += dt;
        alpha  = (elapsed / duration);
        if(elapsed >= duration)
        {
            alpha = 1.0f;
            fadeout = false;
            if(onFadeComplete) onFadeComplete();
        }
    }
}
void Fade_manager::Draw()
{
    if(Global::debug)
        DrawText(TextFormat("fade alph %f", alpha), 800, 10, 10, YELLOW);
    
    if(alpha > 0)
    {
        DrawRectangle(0, 0, 1024, 512, Fade(BLACK, alpha));
    }
}
