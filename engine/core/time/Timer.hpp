#pragma once
#include <chrono>

// a simple timer class designed to keep track of delta time throughout the engine
class Timer
{
    public:
        Timer()
        {
            lastTime = std::chrono::high_resolution_clock::now();
        }

        // designed to be called once per frame
        float tick()
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> delta = currentTime - lastTime;
            lastTime = currentTime;
            elapsedTime += delta.count();
            frameCount++;
            return delta.count();
        }

        float getElapsedTime() const { return elapsedTime; }
        uint64_t getFrameCount() const { return frameCount; }

    private:
        std::chrono::high_resolution_clock::time_point lastTime;
        float elapsedTime = 0.0f;
        uint64_t frameCount = 0;
};
