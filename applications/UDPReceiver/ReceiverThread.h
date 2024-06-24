#pragma once

#include <iostream>
#include <thread>
#include <atomic>

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

#include "ThreadSafeQueue.h"
#include "FrameData.h"
#include "ReportData.h"
#include "Decoder.h"
#include "UDPReceive.h"
#include "UDPSend.h"
#include "IMGUtility.h"

class ReceiverThread 
{
	private:
        std::thread receiverThread;
        ThreadSafeQueue<FrameData>& frameQueue;

        double currentFramerate = 0.0;

        std::atomic<bool> running;

        void worker();
        void onFrameDecodedCallback(const AVFrame* frame);
    public:
        ReceiverThread(ThreadSafeQueue<FrameData>& queue);
        ~ReceiverThread();
        void start();
        void stop();
};