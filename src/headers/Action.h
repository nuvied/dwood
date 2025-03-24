//
// Created by Naveed on 3/18/2025.
//

#ifndef DEVILWOOD_ACTION_H
#define DEVILWOOD_ACTION_H

#include <raylib.h>
#include <memory>
#include <functional>

class Action {
public:
    virtual  void Start(){};
    virtual  void Update(float dt) = 0;
    virtual  bool IsFinished() = 0;
    virtual  ~Action() = default;

    virtual  void OnStart(){};
    virtual  void OnFinish(){};



};
//=======================================================

class ActionMove:public Action
{
private:
    Vector2 *targetPos;
    Vector2 start, end;

    float duration, elapsedTime;
public:
    ActionMove(Vector2 *obj, Vector2 to, float time):targetPos(obj), start(*obj), end(to), duration(time), elapsedTime(0.0)
    {

    }
    void OnStart() override
    {
        start = (*targetPos);
    }


    void Update(float dt) override
    {
        if(elapsedTime < duration)
        {
            elapsedTime += dt;
            float t = elapsedTime / duration;
            targetPos->x = start.x + (end.x - start.x) * t;
            targetPos->y = start.y + (end.y - start.y) * t;
        }
    }
    bool IsFinished() override
    {
        return elapsedTime >= duration;
    }
    void OnFinish() override
    {
        //
    }
};


#endif //DEVILWOOD_ACTION_H
