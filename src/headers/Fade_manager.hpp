//
//  Fade_manager.hpp
//  raygame2
//
//  Created by apple on 14/02/2025.
//

#ifndef Fade_manager_hpp
#define Fade_manager_hpp

#include <functional>
#include "raylib.h"


class Fade_manager
{
public:
    float alpha = 0.0f;
    float duration = 1.0f;
    float elapsed = 0.0f;
    
    bool fadein = false, fadeout = false;
    std::function<void()> onFadeComplete;
    
    void StartFadeIn(float time, std::function<void()> callback = nullptr);
    void StartFadeOut(float time, std::function<void()> callback = nullptr);
    
    
    
    void Update(float dt);
    void Draw();
};

#endif /* Fade_manager_hpp */
