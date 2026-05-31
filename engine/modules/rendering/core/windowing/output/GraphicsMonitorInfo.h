#pragma once

#include <string>

struct GraphicsMonitorInfo
{
    int index = 0;
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool primary = false;
};
