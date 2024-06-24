#pragma once

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

#include <iostream>
#include <png.h>

#include "OnFrameDecodedEvent.h"

class Decoder
{
    private:
        AVCodecContext* codecContext;
        AVFrame* frame;
        AVPacket* pkt;

        int i = 1;

        void setupDecoder(AVCodecID id);
        void allocateResources(uint32_t width, uint32_t height, int i);
    public:
        Decoder();
        Decoder(AVCodecID id);
        ~Decoder();

        OnFrameDecodedEvent onFrameDecoded;
        void addFrameDecodedListener(OnFrameDecodedEvent::Callback listener);
        void decode(char* data, int receivedBytes);
};