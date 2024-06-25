#ifndef GTS_ENCODER_HPP
#define GTS_ENCODER_HPP

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

#include <string.h>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "UDPSend.h"
#include "UDPReceive.h"
#include "ReportData.h"
#include "GtsFrameGrabber.hpp"

struct FrameConverter 
{
    uint8_t* dst_data[4];
    int dst_linesize[4];

    FrameConverter(uint8_t* dataImage, int width, int height) 
	{
        uint8_t* src_data[4] = { dataImage, nullptr, nullptr, nullptr };
        int src_linesize[4] = { width * 4, 0, 0, 0 };

        av_image_alloc(dst_data, dst_linesize, width, height, AV_PIX_FMT_YUV420P, 1);

        struct SwsContext* sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGBA,
                                                     width, height, AV_PIX_FMT_YUV420P,
                                                     0, nullptr, nullptr, nullptr);

        sws_scale(sws_ctx, src_data, src_linesize, 0, height, dst_data, dst_linesize);

        sws_freeContext(sws_ctx);
    }

    ~FrameConverter() 
	{
        av_freep(&dst_data[0]);
    }
};

//adapted from task6, but optimized a bit, at least in terms of performance, but not in terms of overall code cleanliness
//one could say its a bit hacky indeed, but so is most of this codebase 
class GtsEncoder
{
    private:
        const char* path = "output.mpg";
        AVCodecContext *codecContext = nullptr;
        AVFormatContext *formatContext = nullptr;
        GtsFrameGrabber *framegrabber;
        float FPS = 30.0;
        double frameInterval = 1.0 / FPS;
        double lastFrameTime = 0.0;
        int frameCounter = 0;
        AVFrame *frame;
        AVPacket *pkt;
        UDPSend sender;
        std::atomic<bool> reportListenerRunning = true;
        std::thread reportListenerThread;
        std::mutex sendMutex;
        std::condition_variable sendCV;
        std::deque<AVPacket*> packetQueue;

        void allocateAVFrame(uint32_t width, uint32_t height)
        {
            this->frame = av_frame_alloc();
            if (!this->frame)
            {
                std::cerr << "Failed to allocate AVFrame\n";
                return;
            }

            this->frame->pts = this->frameCounter;
            frame->format = AV_PIX_FMT_YUV420P;
            frame->width = width;
            frame->height = height;
            av_frame_get_buffer(frame, 0);
        }
        
        void encodeAndWriteFrame(uint8_t *dataImage, uint32_t width, uint32_t height)
        {
            FrameConverter image = FrameConverter(dataImage, width, height);
            this->allocateAVFrame(width, height);
            av_image_copy(frame->data, frame->linesize, image.dst_data, image.dst_linesize, AV_PIX_FMT_YUV420P, width, height);
            this->pkt = av_packet_alloc();

            int ret = avcodec_send_frame(codecContext, frame);
            if (ret < 0) 
            {
                std::cerr << "Error sending frame for encoding\n";
                av_frame_free(&frame);
                return;
            }

            while (ret >= 0) 
            {
                ret = avcodec_receive_packet(codecContext, this->pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }		
                else if (ret < 0) 
                {
                    std::cerr << "Error during encoding\n";
                    av_frame_free(&frame);
                    return;
                }

                av_packet_rescale_ts(this->pkt, codecContext->time_base, formatContext->streams[0]->time_base);
                this->pkt->stream_index = formatContext->streams[0]->index;

                std::lock_guard<std::mutex> lock(sendMutex);
                packetQueue.push_back(av_packet_clone(this->pkt));
                sendCV.notify_one();
            }
        }

        void initUDPSend()
        {
            char ip[] = "127.0.0.1";
            sender.init(ip, 2000);
        }

        void reportListenerFunction()
        {
            UDPReceive receiver;
            receiver.init(10000);
            const int bufferSize = 3000000;
            char buffer[bufferSize];
            double packetTime;
            int receivedBytes;

            while(reportListenerRunning)
            {
                receivedBytes = receiver.receive(buffer, bufferSize, &packetTime);

                if (receivedBytes > 0) 
                {
                    std::cout << "Received payload size: " << receivedBytes << " bytes" << std::endl;
                    ReportData report;
                    report.deserialize(buffer);

                    std::cout << std::endl << "Received Report!" << std::endl;
                    std::cout << "ReceivedByteRate: " << report.receivedByteRate << std::endl;
                    std::cout << "PacketLossRate: " << report.packetLossRate << std::endl;
                    std::cout << "frameRate: " << report.frameRate << std::endl << std::endl;

                    if(report.packetLossRate > 3.0)
                    {
                        if(!(this->FPS <= 10.0))
                        {
                            this->FPS--;
                        }
                    }
                    else
                    {
                        if(!(this->FPS >= 40.0))
                        {
                            this->FPS++;
                        }
                    }

                    this->frameInterval = 1.0 / FPS;
                    std::cout << "Production FPS: " << this->FPS << std::endl;
                    std::cout << "Frame Interval: " << this->frameInterval << std:: endl << std::endl;
                }
            }
        }

        void sendPacketFunction()
        {
            while (reportListenerRunning)
            {
                std::unique_lock<std::mutex> lock(sendMutex);
                sendCV.wait(lock, [this] { return !packetQueue.empty() || !reportListenerRunning; });

                while (!packetQueue.empty())
                {
                    AVPacket* packet = packetQueue.front();
                    packetQueue.pop_front();

                    int bytesSent = this->sender.send(reinterpret_cast<char*>(packet->data), packet->size);
                    if (bytesSent == -1) 
                    {
                        std::cerr << "Failed to send message." << std::endl;
                    }
                    else
                    {
                        //std::cout << "Bytes sent: " << bytesSent << std::endl;
                    }

                    av_packet_free(&packet);
                }
            }
        }

    public:

        void onFrameEnded(int deltaTime, uint32_t imageIndex)
        {
            //std::cout << "OnFrame ended event" << std::endl;
            uint32_t imageSize = 600 * 800 * 4;

            uint8_t *dataImage = new uint8_t[imageSize];
            framegrabber->getCurrentFrame(imageIndex, dataImage);
            encodeAndWriteFrame(dataImage, 600, 800);
            delete[] dataImage;
        }

        GtsEncoder(GtsFrameGrabber* framegrabber)
        {
            this->framegrabber = framegrabber;
            const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);

            this->codecContext = avcodec_alloc_context3(codec);
            codecContext->width = 600;
            codecContext->height = 800;
            codecContext->bit_rate = 200000;
            codecContext->time_base = {1, (int)FPS};
            codecContext->framerate = {(int)FPS, 1};
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
            codecContext->codec_type = AVMEDIA_TYPE_VIDEO;

            av_opt_set(codecContext->priv_data, "preset", "ultrafast", 0);
            av_opt_set(codecContext->priv_data, "tune", "zerolatency", 0);

            if (avcodec_open2(codecContext, codec, NULL) < 0) {
                std::cerr << "Failed to open codec\n";
            }

            if (avformat_alloc_output_context2(&formatContext, NULL, NULL, path) < 0) {
                std::cerr << "Failed to allocate output format context\n";
            }

            AVStream *videoStream = avformat_new_stream(formatContext, codec);
            if (!videoStream) {
                std::cerr << "Failed to create new stream\n";
            }

            avcodec_parameters_from_context(videoStream->codecpar, codecContext);

            if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
                if (avio_open(&formatContext->pb, path, AVIO_FLAG_WRITE) < 0) {
                    std::cerr << "Failed to open output file\n";
                }
            }

            if (avformat_write_header(formatContext, NULL) < 0) {
                std::cerr << "Failed to write file header\n";
            }

            this->initUDPSend();
            reportListenerThread = std::thread(&GtsEncoder::reportListenerFunction, this);
            std::thread(&GtsEncoder::sendPacketFunction, this).detach();
        }

        ~GtsEncoder()
        {
            this->reportListenerRunning = false;
            this->sendCV.notify_all();
            this->reportListenerThread.join();
            this->sender.closeSock();

            if (codecContext) 
            {
                avcodec_free_context(&codecContext);
            }

            if (formatContext) 
            {
                av_write_trailer(formatContext);

                if (formatContext->pb) 
                {
                    avio_close(formatContext->pb);
                }

                avformat_free_context(formatContext);
            }
        }
};

#endif // GTS_ENCODER_HPP
