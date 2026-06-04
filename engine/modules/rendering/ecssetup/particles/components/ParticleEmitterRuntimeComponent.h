#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GlmConfig.h"
#include "Types.h"

struct ParticleState
{
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
    glm::vec4 tint     = {1.0f, 1.0f, 1.0f, 1.0f};

    float age       = 0.0f;
    float lifetime  = 1.0f;
    float rotation  = 0.0f;
    float spin      = 0.0f;
    float sizeScale = 1.0f;
    float frameOffset = 0.0f;
};

struct ParticleEmitterRuntimeComponent
{
    std::vector<ParticleState> particles;
    float spawnAccumulator = 0.0f;
    float emitterAge = 0.0f;
    uint32_t rngState = 1u;
    std::vector<uint32_t> burstRepeatCounts;

    texture_id_type textureID = 0;
    std::string boundTexturePath;
    uint64_t appliedEffectVersion = 0;
};
