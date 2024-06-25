#pragma once

#include <GLFW/glfw3.h>

#include <thread>
#include <atomic>
#include <iostream>
#include <functional>
#include <cstring>
#include "UDPReceive.h"
#include "KeyboardInputData.h"
#include "KeyboardInput.h"

class InputReceiverThread 
{
    public:
        using KeyPressedCallback = std::function<void(int key, int scancode, int action, int mods)>;
        InputReceiverThread(KeyPressedCallback callback);
        ~InputReceiverThread();
        void start(int port);
        void stop();
    private:
        std::thread receiverThread;
        std::atomic<bool> running;
        UDPReceive udpReceiver;
        KeyPressedCallback keyPressed;
        void worker();
};

InputReceiverThread::InputReceiverThread(KeyPressedCallback callback)
    : running(false), keyPressed(callback) {}

InputReceiverThread::~InputReceiverThread()
{
    if (this->running)
    {
        this->stop();
    }
}

void InputReceiverThread::start(int port)
{
    if (this->running) 
    {
        std::cerr << "Input receiver thread is already running" << std::endl;
        return;
    }

    std::cout << "Starting Input Receiver Thread!" << std::endl;
    this->running = true;
    udpReceiver.init(port);
    receiverThread = std::thread(&InputReceiverThread::worker, this);
}

void InputReceiverThread::stop()
{
    if (!this->running) 
    {
        std::cerr << "Input receiver thread is already stopped" << std::endl;
        return;
    }

    std::cout << "Stopping Input Receiver Thread!" << std::endl;
    this->running = false;

    if (this->receiverThread.joinable())
    {
        this->receiverThread.join();
        std::cout << "Joined Input Receiver Thread Successfully!" << std::endl;
    }

    udpReceiver.closeSock();
    std::cout << "Stopped Input Receiver Thread!" << std::endl;
}

void InputReceiverThread::worker()
{
    const int bufferSize = sizeof(KeyboardInput);
    char buffer[bufferSize];

    while (this->running)
    {
        double packetTime;
        int receivedBytes = udpReceiver.receive(buffer, bufferSize, &packetTime);

        if (receivedBytes > 0)
        {
            KeyboardInputData inputData;
            inputData.deserialize(buffer);

            int key;
            switch (inputData.input)
            {
                case KeyboardInput::A:
                    key = GLFW_KEY_A;
                    break;
                case KeyboardInput::D:
                    key = GLFW_KEY_D;
                    break;
                case KeyboardInput::LEFT_ARROW:
                    key = GLFW_KEY_LEFT;
                    break;
                case KeyboardInput::RIGHT_ARROW:
                    key = GLFW_KEY_RIGHT;
                    break;
                case KeyboardInput::P:
                    key = GLFW_KEY_P;
                    break;
                default:
                    key = -1;  // Unused
                    break;
            }

            if (key != -1 && keyPressed)
            {
                keyPressed(key, 0, 0, 0);
            }
        }
    }
}
