#pragma once

#include <string>

// configuration settings for the OutputWindow wrapper object
struct OutputWindowConfig 
{
    int width;
    int height;
    std::string title;
    
    bool enableValidationLayers;
};