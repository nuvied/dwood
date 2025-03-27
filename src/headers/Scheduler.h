#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <functional>
#include <vector>


struct ScheduledTask
{
    /* data */
    std::function<void()> action;
    float delay;
    bool completed;
    int task_id;
};


class Scheduler
{
private:
    std::vector<ScheduledTask> tasks;
public:
    void Schedule(float delay, std::function<void()> action, int id = 0);
    void Update(float dt);


};

#endif