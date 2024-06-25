#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <SDL2/SDL.h>
#include "SDLWindow.h"

#include "UDPSend.h"
#include "KeyboardInput.h"
#include "KeyboardInputData.h"

class InputSenderThread 
{
    private:
        std::thread inputThread;
        std::atomic<bool> running;
        std::queue<SDL_Event> eventQueue;
        std::mutex queueMutex;
        std::condition_variable queueCV;
        void worker();
    public:
        InputSenderThread();
        ~InputSenderThread();
        void start();
        void stop();
        void pushEvent(const SDL_Event& event);
};