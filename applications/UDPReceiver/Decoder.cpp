#include <iostream>

#include "Decoder.h"

Decoder::Decoder() : codecContext(nullptr), frame(nullptr), pkt(nullptr)
{
}

//needs to be called with a codec id, or it wont function at all
Decoder::Decoder(AVCodecID id) : Decoder()
{
    this->setupDecoder(id);
}

Decoder::~Decoder()
{
    //std::cout << "Entering decoder destruction!" << std::endl;

    if (codecContext) 
    {
        avcodec_free_context(&codecContext);
    }

    //std::cout << "before frame destruction" << std::endl;
    if (frame) 
    {
        av_frame_free(&frame);
    }

    //std::cout << "Successfully destroyed decoder!" << std::endl;
}

void Decoder::setupDecoder(AVCodecID id)
{
    const AVCodec* codec = avcodec_find_decoder(id);
    if (!codec) 
    {
        std::cerr << "Codec not found" << std::endl;
    }

    this->codecContext = avcodec_alloc_context3(codec);
    if (!this->codecContext) 
    {
        std::cerr << "Failed to allocate codec context" << std::endl;
    }

    if (avcodec_open2(this->codecContext, codec, nullptr) < 0) 
    {
        std::cerr << "Failed to open codec" << std::endl;
    }

    this->frame = av_frame_alloc();
    if (!this->frame)
    {
        std::cerr << "Failed to allocate AVFrame" << std::endl;
        avcodec_free_context(&codecContext);
        return;
    }

    this->pkt = av_packet_alloc();
    if (!this->pkt)
    {
        std::cerr << "Failed to allocate AVPacket" << std::endl;
        av_frame_free(&frame);
        avcodec_free_context(&codecContext);
        return;
    }
}

void Decoder::decode(char* data, int receivedBytes)
{
    if (!codecContext || !frame || !pkt)
    {
        std::cerr << "Decoder not properly initialized" << std::endl;
        return;
    }

    if (av_packet_from_data(pkt, reinterpret_cast<uint8_t*>(data), receivedBytes) != 0)
    {
        std::cerr << "Could not construct packet from data" << std::endl;
        return;
    }

    int ret = avcodec_send_packet(codecContext, pkt);
    if (ret < 0) 
    {
        std::cerr << "Error sending packet for decoding" << std::endl;
        std::cerr << av_err2str(ret) << std::endl;
        return;
    }

    ret = avcodec_receive_frame(codecContext, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
    {
        return;
    } 
    else if (ret < 0) 
    {
        std::cerr << "Error receiving frame" << std::endl;
        return;
    }

    //trigger the on frame decoded event here
    this->onFrameDecoded.trigger(frame);
    i++;
}

void Decoder::addFrameDecodedListener(OnFrameDecodedEvent::Callback listener)
{
    onFrameDecoded.addListener(listener);
}