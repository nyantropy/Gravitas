#pragma once

#include <cstdint>
#include <string>


class GravitasEngineConstants
{
    public:
        static const uint32_t GLFW_DEFAULT_WIDTH = 800;
        static const uint32_t GLFW_DEFAULT_HEIGHT = 600;

        static const std::string V_SHADER_PATH;
        static const std::string F_SHADER_PATH;
};

const std::string GravitasEngineConstants::V_SHADER_PATH = "shaders/vert.spv";
const std::string GravitasEngineConstants::F_SHADER_PATH = "shaders/frag.spv";