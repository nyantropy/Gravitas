#pragma once

#include <cstdint>
#include <string>

#include "EnginePaths.h"

class GraphicsConstants
{
    public:
        static inline const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

        static inline const char* V_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/vert.spv";
        static inline const char* F_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/frag.spv";

        static inline const std::string V_SHADER_PATH = V_SHADER_PATH_CSTR;
        static inline const std::string F_SHADER_PATH = F_SHADER_PATH_CSTR;

        static inline const char* UI_V_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/ui_vert.spv";
        static inline const char* UI_F_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/ui_frag.spv";

        static inline const std::string UI_V_SHADER_PATH = UI_V_SHADER_PATH_CSTR;
        static inline const std::string UI_F_SHADER_PATH = UI_F_SHADER_PATH_CSTR;

        static inline const char* PARTICLE_V_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/particle_vert.spv";
        static inline const char* PARTICLE_F_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/particle_frag.spv";
        static inline const char* PARTICLE_MESH_V_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/particle_mesh_vert.spv";
        static inline const char* PARTICLE_MESH_F_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/particle_mesh_frag.spv";

        static inline const std::string PARTICLE_V_SHADER_PATH = PARTICLE_V_SHADER_PATH_CSTR;
        static inline const std::string PARTICLE_F_SHADER_PATH = PARTICLE_F_SHADER_PATH_CSTR;
        static inline const std::string PARTICLE_MESH_V_SHADER_PATH = PARTICLE_MESH_V_SHADER_PATH_CSTR;
        static inline const std::string PARTICLE_MESH_F_SHADER_PATH = PARTICLE_MESH_F_SHADER_PATH_CSTR;

        static inline const char* UPSCALE_V_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/upscale_vert.spv";
        static inline const char* UPSCALE_F_SHADER_PATH_CSTR = GRAVITAS_ENGINE_ROOT "/shaders/upscale_frag.spv";

        static inline const std::string UPSCALE_V_SHADER_PATH = UPSCALE_V_SHADER_PATH_CSTR;
        static inline const std::string UPSCALE_F_SHADER_PATH = UPSCALE_F_SHADER_PATH_CSTR;

        static inline const char* ENGINE_RESOURCES_CSTR = EnginePaths::ENGINE_RESOURCES_CSTR;
        static inline const std::string ENGINE_RESOURCES = EnginePaths::ENGINE_RESOURCES;
};
