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
};


class Scheduler
{
private:
    std::vector<ScheduledTask> tasks;
public:
    void Schedule(float delay, std::function<void()> action);
    void Update(float dt);


};

#endif