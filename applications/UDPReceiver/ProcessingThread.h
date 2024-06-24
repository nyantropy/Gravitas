#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <cmath>

#include "ThreadSafeQueue.h"
#include "FrameData.h"
#include "IMGUtility.h"
#include "SDLWindow.h"

class ProcessingThread 
{
	private:
        std::thread processingThread;
        ThreadSafeQueue<FrameData>& frameQueue;

        std::atomic<bool> running;

        void worker();
    public:
        ProcessingThread(ThreadSafeQueue<FrameData>& queue);
        ~ProcessingThread();
        void start();
        void stop();
};