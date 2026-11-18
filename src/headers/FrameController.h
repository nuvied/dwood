#pragma once

#include "raylib.h"

#ifdef DEVILWOOD_CUSTOM_FRAME_CONTROL
#include <chrono>
#include <thread>
#endif

class FrameController
{
public:
    float GetDeltaTime() const
    {
#ifdef DEVILWOOD_CUSTOM_FRAME_CONTROL
        return frameTime;
#else
        return GetFrameTime();
#endif
    }

    void EndFrame()
    {
#ifdef DEVILWOOD_CUSTOM_FRAME_CONTROL
        SwapScreenBuffer();
        double currentTime = GetTime();
        double remaining = 1.0 / 60.0 - (currentTime - previousFrameTime);
        if (remaining > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(remaining));
            currentTime = GetTime();
        }
        frameTime = static_cast<float>(currentTime - previousFrameTime);
        previousFrameTime = currentTime;
        PollInputEvents();
#endif
    }

private:
#ifdef DEVILWOOD_CUSTOM_FRAME_CONTROL
    double previousFrameTime = GetTime();
    float frameTime = 0.0f;
#endif
};
