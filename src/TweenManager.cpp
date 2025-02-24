#include "TweenManager.h"

void TweenManager::AddTween(float *target, float endValue, float duration, std::function<void()> callback)
{
    Tween t;
    t.target = target ;
    t.startValue = *target;
     t.endValue = endValue;
     t.duration = duration;
     t.action = callback;
     t.isActive = true;
     t.elapsedTime = 0;
     tweens.push_back(t);

}

void TweenManager::Update(float dt)
{
    for(auto& tween:tweens)
    {
        if(!tween.isActive)continue;;

        tween.elapsedTime += dt;

        float t = tween.elapsedTime / tween.duration;
        if(t >= 1.0f)
        {
            t = 1.0f;
            tween.isActive = false;
            // if we have a call back
            if(tween.action)tween.action();
        }

        *(tween.target) = tween.startValue + (tween.endValue - tween.startValue) * t;
        

    }
}
