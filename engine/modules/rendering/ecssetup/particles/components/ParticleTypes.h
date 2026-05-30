#pragma once

#include <vector>

#include "GlmConfig.h"

enum class ParticleEmitterShape
{
    Sphere = 0,
    Box,
    Disc,
    Cylinder,
    Ring
};

enum class ParticleBlendMode
{
    Alpha = 0,
    Additive
};

struct ParticleColorKey
{
    float     t     = 0.0f;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ParticleFloatKey
{
    float t     = 0.0f;
    float value = 1.0f;
};

using ParticleColorCurve = std::vector<ParticleColorKey>;
using ParticleFloatCurve = std::vector<ParticleFloatKey>;

