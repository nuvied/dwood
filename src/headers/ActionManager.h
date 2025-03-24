//
// Created by Naveed on 3/19/2025.
//

#ifndef DEVILWOOD_ACTIONMANAGER_H
#define DEVILWOOD_ACTIONMANAGER_H

#include <utility>

#include "Action.h"

class ActionList {
private:
    std::vector<std::unique_ptr<Action>> actions;
    size_t currentAction = 0;

    std::function<void()> onFinish = [](){};

public:
    void AddAction(std::unique_ptr<Action> action)
    {
        actions.push_back(std::move(action));
    }

    void OnFinish(std::function<void()> callback)
    {
        onFinish = std::move(callback);
    }
    void Update(float dt)
    {
        if(currentAction < actions.size())
        {
            actions[currentAction]->Update(dt);
            if(actions[currentAction]->IsFinished())
            {
                actions[currentAction]->OnFinish();
                currentAction++;


                if(onFinish && currentAction >= actions.size())
                {
                    onFinish();
                    return;
                }

                actions[currentAction]->OnStart();
            }
        }
    }

    bool IsFinished()
    {
        return currentAction >= actions.size();
    }
};


#endif //DEVILWOOD_ACTIONMANAGER_H
