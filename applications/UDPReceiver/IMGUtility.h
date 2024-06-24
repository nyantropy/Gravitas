#pragma once

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

class IMGUtility
{
    private:
       
    public:
        IMGUtility();
        ~IMGUtility();
        void saveFrameAsImage(const AVFrame* frame, const std::string& filename);
        void saveFrameAsImage(uint32_t* image, int width, int height, const std::string& filename);

        uint32_t* extractImageData(const AVFrame* frame, int* width, int* height);
        void saveRGBToPNG(const char* filename, uint32_t* rgb, int width, int height);
};