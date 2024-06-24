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
		//ffmpeg is weird, since it does not use all the array entries
		//we multiply by 4 since we assume the image is RGBA, meaning we store 4 bytes per pixel
        uint8_t* src_data[4] = { dataImage, nullptr, nullptr, nullptr };
		//that essentially means that linesize refers to the number of bytes occupied by each row of image data
        int src_linesize[4] = { width * 4, 0, 0, 0 };

		//the buffers still need to be allocated for all of this to work though, and it does need height as a parameter as well
        av_image_alloc(dst_data, dst_linesize, width, height, AV_PIX_FMT_YUV420P, 1);

		//now we just use sws to convert the image from RGBA image space to YUV420P image space and we are good to go :)
        struct SwsContext* sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGBA,
                                                     width, height, AV_PIX_FMT_YUV420P,
                                                     0, nullptr, nullptr, nullptr);

        sws_scale(sws_ctx, src_data, src_linesize, 0, height, dst_data, dst_linesize);

        sws_freeContext(sws_ctx);
    }

    ~FrameConverter() 
	{
		//no sigsegv today, always free up memory, in this case we only need to free the first entry of the array, since the other entries are all nullptr
        av_freep(&dst_data[0]);
    }
};

class GtsEncoder
{
    private:
        const char* path = "output.mpg";
        AVCodecContext *codecContext = nullptr;
        AVFormatContext *formatContext = nullptr;

        GtsFrameGrabber *framegrabber;

        float FPS = 30.0;

        //we want to sync the frame recording with the fps of our video, so we dont have too many or too little frames making up a second
        //if we dont gatekeep with this mechanism, our video will either pass time too fast if there are excess frames, or too slow if there are too little frames
        //EDIT: it looks like this bug i experienced was somewhat random? i could not replicate it spontaneously, which is quite weird considering i did not change anything lol
        //however, manually changing fps still breaks the program without this measure, which is expected by logic, so maybe my application just happened to run with about 60fps per default
        double frameInterval = 1.0 / FPS;
        double lastFrameTime = 0.0;
        int frameCounter = 0;

        AVFrame *frame;
        AVPacket *pkt;

        UDPSend sender;

        std::atomic<bool> reportListenerRunning = true;
        std::thread reportListenerThread;

        void allocateAVFrame(uint32_t width, uint32_t height)
        {
            this->frame = av_frame_alloc();
            if (!this->frame)
            {
                std::cerr << "Failed to allocate AVFrame\n";
                return;
            }

            //this is the holy line, it is needed for timestamps to work correctly with the codec we chose
            this->frame->pts = this->frameCounter;
            frame->format = AV_PIX_FMT_YUV420P;
            frame->width = width;
            frame->height = height;

            //allocate frame buffers
            av_frame_get_buffer(frame, 0);
        }

        void allocateAVPacket()
        {
            this->pkt = av_packet_alloc();
        }

        void freePacketAndFrame()
        {
            av_packet_unref(this->pkt);
            av_frame_free(&frame);
        }

        void encodeAndWriteFrame(uint8_t *dataImage, uint32_t width, uint32_t height)
        {
            //Step 1: Convert RGBA frame data to YUV420p format
            //this is the image that can be encoded, since we match the format of the video stream
            FrameConverter image = FrameConverter(dataImage, width, height);

            //Step 2: Allocate AVFrame and fill it with the converted data
            //messing with this can cause huge memory problems, caution is advised
            this->allocateAVFrame(width, height);

            //Step 3: Copy the image into the frame
            av_image_copy(frame->data, frame->linesize, image.dst_data, image.dst_linesize, AV_PIX_FMT_YUV420P, width, height);

            this->allocateAVPacket();

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

                //saveDataToFile(pkt->data, pkt->size, "../../media/pktdata/sender/frame" + std::to_string(frameCounter) + ".txt");

                //std::cout << "Packet " << this->frameCounter << std::endl;

                int bytesSent = this->sender.send(reinterpret_cast<char*>(this->pkt->data), this->pkt->size);
                if (bytesSent == -1) 
                {
                    std::cerr << "Failed to send message." << std::endl;
                }
                else
                {
                    std::cout << "Bytes sent: " << bytesSent << std::endl;
                }

                //Step 4: Write the encoded frame to the output file
                //finally, we can write the encoded frame to the video file
                av_packet_rescale_ts(this->pkt, codecContext->time_base, formatContext->streams[0]->time_base);
                this->pkt->stream_index = formatContext->streams[0]->index;
                av_interleaved_write_frame(formatContext, this->pkt);
            }
        }

        //helper method to check what kind of data is sent through the network
        void saveDataToFile(uint8_t* data, size_t dataSize, const std::string& filename)
        {
            std::ofstream outputFile(filename, std::ios::binary);
            if (!outputFile.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filename << std::endl;
                return;
            }

            for (size_t i = 0; i < dataSize; ++i)
            {
                outputFile << static_cast<int>(data[i]) << " ";
            }

            outputFile.close();
            std::cout << "Data saved to file: " << filename << std::endl;
        }

        //loopback address on port 2000
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

                    //adjust framerate bit by bit, we start with 30fps, but can increase it if we dont have packet loss
                    if(report.packetLossRate > 3.0)
                    {
                        //minimum fps
                        if(!(this->FPS <= 10.0))
                        {
                            this->FPS--;
                        }
                    }
                    else
                    {
                        //maximum fps
                        if(!(this->FPS >= 40.0))
                        {
                            this->FPS++;
                        }
                    }

                    //we need to adjust the frame Interval as well
                    //this does work fairly well, problem is we would need to adjust framerate on the receiver as well which is a bit yikes
                    this->frameInterval = 1.0 / FPS;
                    std::cout << "Production FPS: " << this->FPS << std::endl;
                    std::cout << "Frame Interval: " << this->frameInterval << std:: endl << std::endl;
                }
            }
        }

    public:

        void onFrameEnded(int deltaTime, uint32_t imageIndex)
        {
            std::cout << "OnFrame ended event" << std::endl;
            uint32_t imageSize = 600 * 800 * 4;

            uint8_t *dataImage = new uint8_t[imageSize];

            framegrabber->getCurrentFrame(imageIndex, dataImage);
    
            //rewritten to now use a sender instead
            encodeAndWriteFrame(dataImage, 600, 800);

            delete[] dataImage;
            //this->freePacketAndFrame();
        }

        GtsEncoder()
        {

        }

        GtsEncoder(GtsFrameGrabber* framegrabber)
        {
            this->framegrabber = framegrabber;
            //we are gonna be using h264 for this program
            const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);

            //initialize context variable
            this->codecContext = avcodec_alloc_context3(codec);

            //parameters depend on how big our frames are, in this case we got the values by simply looking at the dimensions the screenshots have
            codecContext->width = 600;
            codecContext->height = 800;
            codecContext->bit_rate = 200000;
            codecContext->time_base = {1, (int)FPS};
            codecContext->framerate = {(int)FPS, 1};
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
            codecContext->codec_type = AVMEDIA_TYPE_VIDEO;

            //open the codec
            if (avcodec_open2(codecContext, codec, NULL) < 0) {
                std::cerr << "Failed to open codec\n";
            }

            //setup the second variable we need, format context
            if (avformat_alloc_output_context2(&formatContext, NULL, NULL, path) < 0) {
                std::cerr << "Failed to allocate output format context\n";
            }

            //we want a video stream in that output file
            AVStream *videoStream = avformat_new_stream(formatContext, codec);
            if (!videoStream) {
                std::cerr << "Failed to create new stream\n";
            }

            avcodec_parameters_from_context(videoStream->codecpar, codecContext);

            //open output file
            if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
                if (avio_open(&formatContext->pb, path, AVIO_FLAG_WRITE) < 0) {
                    std::cerr << "Failed to open output file\n";
                }
            }

            //write file header
            if (avformat_write_header(formatContext, NULL) < 0) {
                std::cerr << "Failed to write file header\n";
            }

            this->initUDPSend();

            //quick and dirty, im tired :(
            reportListenerThread = std::thread(&GtsEncoder::reportListenerFunction, this);
        }

        ~GtsEncoder()
        {
            this->reportListenerRunning = false;
            this->reportListenerThread.join();
            this->freePacketAndFrame();
            this->sender.closeSock();

            //when we destruct the object, we want to cleanly close the video stream
            //as well, to avoid any errors
            //first we close the codec context
            if (codecContext) 
            {
                avcodec_free_context(&codecContext);
            }

            //and dont forget about the format context either
            if (formatContext) 
            {
                av_write_trailer(formatContext);

                if (formatContext->pb) 
                {
                    avio_close(formatContext->pb);
                }

                avformat_free_context(formatContext);
            }

            delete framegrabber;
        }
};

#endif // GTS_ENCODER_HPP