#ifndef TWEEN_MANAGER_H
#define TWEEN_MANAGER_H
#include <vector>
#include <functional>

struct Tween
{
    /* data */
    float *target;
    float startValue;                      // Starting value of the property
    float endValue;      // Ending value of the property
    float duration;      // Total duration of the tween
    float elapsedTime;  // Time elapsed since the tween started
    bool isActive;      // Whether the tween is currently active

    // Pointer to the object property being tweened (e.g., position, scale, etc.)
    float* targetValue;
    std::function<void()> action;
    // Easing function type (optional, for more advanced tweening)
    float (*easingFunction)(float);
};



class TweenManager
{
public:
    std::vector<Tween> tweens;

    void AddTween(float *target, float endValue, float duration, std::function<void()> callback);

    void Update(float dt);
    

};

#endif