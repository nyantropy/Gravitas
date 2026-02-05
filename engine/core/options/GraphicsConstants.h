#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

// needed to make intellisense shut up, we get the engine root from cmake, since that seems like a cleaner way
#ifndef GRAVITAS_ENGINE_ROOT
#define GRAVITAS_ENGINE_ROOT
#endif

// same goes for the engines resources
#ifndef GRAVITAS_ENGINE_RESOURCES
#define GRAVITAS_ENGINE_RESOURCES
#endif


class GraphicsConstants
{
    public:
        static inline const int MAX_FRAMES_IN_FLIGHT = 2;

        static inline const char* V_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/vert.spv";
        static inline const char* F_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/frag.spv";

        static inline const std::string V_SHADER_PATH = V_SHADER_PATH_CSTR;
        static inline const std::string F_SHADER_PATH = F_SHADER_PATH_CSTR;

        static inline const char* ENGINE_RESOURCES_CSTR = GRAVITAS_ENGINE_RESOURCES "";
        static inline const std::string ENGINE_RESOURCES = ENGINE_RESOURCES_CSTR;
};

