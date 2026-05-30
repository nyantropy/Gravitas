#pragma once

#include <cstdint>
#include <vector>

#include "GlmConfig.h"
#include "ParticleTypes.h"
#include "Types.h"

struct ParticleInstance
{
    glm::vec3 worldPosition = {0.0f, 0.0f, 0.0f};
    float     size          = 1.0f;
    glm::vec4 color         = {1.0f, 1.0f, 1.0f, 1.0f};
    float     rotation      = 0.0f;
    float     depth         = 0.0f;
};

struct ParticleDrawCommand
{
    texture_id_type    textureID      = 0;
    ParticleBlendMode  blendMode      = ParticleBlendMode::Alpha;
    uint32_t           instanceOffset = 0;
    uint32_t           instanceCount  = 0;
};

struct ParticleFrameData
{
    view_id_type cameraViewID = 0;
    uint32_t     emitterCount = 0;

    std::vector<ParticleInstance>   instances;
    std::vector<ParticleDrawCommand> drawCommands;

    bool empty() const
    {
        return drawCommands.empty();
    }

    void clear()
    {
        cameraViewID = 0;
        emitterCount = 0;
        instances.clear();
        drawCommands.clear();
    }
};

struct ParticleFrameDataComponent
{
    ParticleFrameData frameData;
};

