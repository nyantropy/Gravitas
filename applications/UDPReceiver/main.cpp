#include <iostream>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <SDL2/SDL.h>

#include "UDPReceive.h"
#include "ReceiverThread.h"
#include "ProcessingThread.h"
#include "ThreadSafeQueue.h"
#include "FrameData.h"
#include "SDLWindow.h"

#include <png.h>

extern "C" {
    #ifdef HAVE_AV_CONFIG_H
    #undef HAVE_AV_CONFIG_H
    #endif

    #include <libavutil/imgutils.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavformat/avformat.h>
    #include <libavutil/opt.h>
    #include <libavutil/timestamp.h>
}

int main() 
{  
    ThreadSafeQueue<FrameData> frameQueue;

    ReceiverThread receiverThread = ReceiverThread(frameQueue);
    receiverThread.start();
    
    ProcessingThread processingThread = ProcessingThread(frameQueue);
    processingThread.start();

    //program runs for 30 seconds before it dies, need to rework this bit since i still dont know how to incorporate the sdl surface correctly, but oh well
    std::this_thread::sleep_for(std::chrono::seconds(30));

    receiverThread.stop();
    processingThread.stop();

    return 0;
}
