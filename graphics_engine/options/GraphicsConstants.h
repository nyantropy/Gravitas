#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

class GraphicsConstants
{
public:
    static const int MAX_FRAMES_IN_FLIGHT = 2;

    static inline const std::string V_SHADER_PATH = "shaders/vert.spv";
    static inline const std::string F_SHADER_PATH = "shaders/frag.spv";

    static inline const char* ENGINE_NAME = "Gravitas";
    static inline const uint32_t ENGINE_VERSION = VK_MAKE_API_VERSION(1, 0, 0, 0);
};
