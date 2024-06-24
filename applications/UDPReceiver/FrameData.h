#pragma once

#include <cstdint>

struct FrameData 
{
    uint32_t* data;
    int width;
    int height;

    //idk just put it here for now
    double framerate;
};