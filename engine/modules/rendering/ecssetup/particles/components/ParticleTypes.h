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

struct ParticleBurst
{
    float    time           = 0.0f;
    uint32_t countMin       = 0;
    uint32_t countMax       = 0;
    float    repeatInterval = 0.0f;
    uint32_t repeatCount    = 0;
};

struct ParticleFlipbook
{
    uint32_t columns        = 1;
    uint32_t rows           = 1;
    uint32_t frameCount     = 1;
    float    frameRate      = 12.0f;
    bool     lifetimeDriven = true;
    bool     randomStart    = false;
};

struct ParticleForceModule
{
    glm::vec3 acceleration  = {0.0f, 0.0f, 0.0f};
    glm::vec3 wind          = {0.0f, 0.0f, 0.0f};
    float     vortex        = 0.0f;
    float     radial        = 0.0f;
    float     noiseStrength = 0.0f;
    float     noiseScale    = 1.0f;
};

using ParticleColorCurve = std::vector<ParticleColorKey>;
using ParticleFloatCurve = std::vector<ParticleFloatKey>;
