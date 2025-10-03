#include "Scheduler.h"
#include <Global.h>

void Scheduler::Schedule(float delay, std::function<void()> action, int id)
{
    auto v = std::find_if(tasks.begin(), tasks.end(), [id] (const ScheduledTask &t)
    {
    return t.task_id == id;
    });

    if(v != tasks.end())
    { 
        //std::cout << "task running " <<std::endl;
        return;
    }

    tasks.push_back({action, delay, false, id});
    
    printf("tasks count %i \n", tasks.size());
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
