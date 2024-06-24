#pragma once

#include <vector>
#include <functional>

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

class OnFrameDecodedEvent
{
    public:
        using Callback = std::function<void(const AVFrame*)>;
        void addListener(Callback callback)
        {
            listeners.push_back(callback);
        }

        void trigger(const AVFrame* frame)
        {
            for (auto& listener : listeners)
            {
                listener(frame);
            }
        }
    private:
        std::vector<Callback> listeners;
};