#include "ProcessingThread.h"

ProcessingThread::ProcessingThread(ThreadSafeQueue<FrameData>& queue) : frameQueue(queue)
{
    this->running = false;
}

ProcessingThread::~ProcessingThread()
{
    if(this->running)
    {
        this->stop();
    }
}

void ProcessingThread::start()
{
    if (this->running) 
    {
        std::cerr << "Processing thread is already running" << std::endl;
        return;
    }

    std::cout << "Starting Processing Thread!" << std::endl;
    this->running = true;
    processingThread = std::thread(&ProcessingThread::worker, this);
}

void ProcessingThread::stop()
{
    if (!this->running) 
    {
        std::cerr << "Processing thread is already stopped" << std::endl;
        return;
    }

    std::cout << "Stopping Processing Thread!" << std::endl;
    this->running = false;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (this->processingThread.joinable())
    {
        this->processingThread.join();
    }
}

void ProcessingThread::worker()
{
    int SCREEN_WIDTH = 800;
    int SCREEN_HEIGHT = 600;

    SDL_Event event;

    IMGUtility utility;

    //this currently peeks the queue at about 30 frames per second, statically, which of course does not scale with the sender amping up
    //its framerate when no packets are lost, leading to a "slower" display of sorts.
    //right now this is intentional, but for this task i will probably not fix it, since accessing the framerate here is a bit annoying
    std::chrono::milliseconds frameInterval(1000 / 30);

    SDLWindow window = SDLWindow("Output", SCREEN_WIDTH, SCREEN_HEIGHT);
    uint32_t* pixels = new uint32_t[SCREEN_WIDTH * SCREEN_HEIGHT];

    //start out the window with a glaring red eyeburner
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) 
    {
         pixels[i] = 0xFF0000FF;
    }

    while(running)
    {
        window.update(pixels);
        //std::cout << "Frames in Queue: " << this->frameQueue.size() << std::endl;

        if(!this->frameQueue.empty())
        {
            FrameData frame;
            this->frameQueue.pop(frame);
            //saving a frame to an image takes ages, which slows down the frame presentation 
            //utility.saveRGBToPNG("frame.png", frame.data, frame.width, frame.height);
            pixels = frame.data;
            int roundedRate = static_cast<int>(std::ceil(frame.framerate));
            std::cout << "Rounded FrameRate: " << roundedRate << std::endl;

            if(roundedRate != 0)
            {
                //i guess we try polling about 15 frames above the rounded rate?
                //this definetly needs to be fine tuned in the future, but for now it will suffice
                std::chrono::milliseconds newinterval(1000 / (roundedRate + 15));
                frameInterval = newinterval;
            }
        }

        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) 
            {
                //this executes whenever we press the x button on the window, need to refactor some code here to actually be able to exit the program and stop
                //both threads here, but thats work for another day
                //commenting out the stop here of course since thats a race condition and we really dont want that, id rather just have the program die naturally
                //window.~SDLWindow();
                //delete[] pixels;
                //this->stop();
            }
        }

        std::this_thread::sleep_for(frameInterval);
    }

    delete[] pixels;
    //window.~SDLWindow();
}
