#include "Scheduler.h"

void Scheduler::Schedule(float delay, std::function<void()> action)
{
    tasks.push_back({action, delay, false});
}

void Scheduler::Update(float dt)
{
    for(auto& task:tasks)
    {
        if(!task.completed)
        {
            task.delay -= dt;
            if(task.delay <= 0)
            {
                task.action();
                task.completed = true;
            }
        }
    }
    ///
    //removeing completed tasks

    for (int i = 0; i < tasks.size(); i++)
    {
        /* code */
        if(tasks[i].completed)
        {
            tasks.erase(tasks.begin() + i);
        }
    }
    
}
