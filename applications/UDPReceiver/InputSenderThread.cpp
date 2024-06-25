#include "InputSenderThread.h"

InputSenderThread::InputSenderThread() : running(false) {}

InputSenderThread::~InputSenderThread()
{
    if (this->running)
    {
        this->stop();
    }
}

void InputSenderThread::start()
{
    if (this->running) 
    {
        std::cerr << "Input sender thread is already running" << std::endl;
        return;
    }

    std::cout << "Starting Input Sender Thread!" << std::endl;
    this->running = true;
    inputThread = std::thread(&InputSenderThread::worker, this);
    inputThread.detach();
}

void InputSenderThread::stop()
{
    if (!this->running) 
    {
        std::cerr << "Input sender thread is already stopped" << std::endl;
        return;
    }

    std::cout << "Stopping Input Sender Thread!" << std::endl;
    this->running = false;
    queueCV.notify_all();

    if (this->inputThread.joinable())
    {
        this->inputThread.join();
        std::cout << "Joined Input Sender Thread Successfully!" << std::endl;
    }

    std::cout << "Stopped Input Sender Thread!" << std::endl;
}

void InputSenderThread::pushEvent(const SDL_Event& event)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push(event);
    queueCV.notify_one();
}

void InputSenderThread::worker()
{
    UDPSend sender;
    char ip[] = "127.0.0.1";
	sender.init(ip, 50000);
    
    const int bufferSize = 3000000;
    char buffer[bufferSize];
    
    double packetTime;
    int receivedBytes;

    int totalReceivedBytes = 0;
    int receivedPackets = 0;
    int lostPackets = 0;

    while (this->running)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCV.wait(lock, [this] { return !eventQueue.empty() || !running; });

        while (!eventQueue.empty())
        {
            SDL_Event event = eventQueue.front();
            eventQueue.pop();
            lock.unlock();


            KeyboardInputData inputData;

            switch (event.key.keysym.sym)
            {
                case SDLK_a:
                    inputData = {KeyboardInput::A};
                    break;             
                case SDLK_d:
                    inputData = {KeyboardInput::D};
                    break;
                case SDLK_LEFT:
                    inputData = {KeyboardInput::LEFT_ARROW};
                    break;         
                case SDLK_RIGHT:
                    inputData = {KeyboardInput::RIGHT_ARROW};
                    break;   
                case SDLK_p:
                    inputData = {KeyboardInput::P};
                    break;
                default:
                    inputData = {KeyboardInput::UNUSED};
                    break;
            }

            char reportBuffer[sizeof(KeyboardInput)];
            inputData.serialize(reportBuffer);
            int bytesSent = sender.send(reportBuffer, sizeof(reportBuffer));

            std::cout << "Sent keyboard input bytes: " << bytesSent << std::endl;
                  

            lock.lock();
        }
    }
}