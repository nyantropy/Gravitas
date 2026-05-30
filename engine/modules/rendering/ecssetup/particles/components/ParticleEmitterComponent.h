#pragma once

#include <cstdint>
#include <string>

#include "GlmConfig.h"
#include "ParticleTypes.h"

// Gameplay-facing particle emitter descriptor. Pair with TransformComponent.
// Engine particle systems own runtime state and backend resources.
struct ParticleEmitterComponent
{
    bool enabled    = true;
    bool localSpace = true;

    ParticleEmitterShape shape = ParticleEmitterShape::Sphere;
    ParticleBlendMode    blend = ParticleBlendMode::Alpha;

    std::string texturePath;
    glm::vec4   baseTint = {1.0f, 1.0f, 1.0f, 1.0f};

    ParticleColorCurve colorOverLifetime = {
        {0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
        {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    ParticleFloatCurve alphaOverLifetime = {
        {0.0f, 0.0f},
        {0.2f, 1.0f},
        {0.8f, 1.0f},
        {1.0f, 0.0f},
    };
    ParticleFloatCurve sizeOverLifetime = {
        {0.0f, 0.35f},
        {1.0f, 0.75f},
    };

    float    emissionRate  = 32.0f;
    uint32_t maxParticles  = 128;
    float    lifetimeMin   = 1.0f;
    float    lifetimeMax   = 2.0f;
    float    intensity     = 1.0f;

    glm::vec3 initialVelocity = {0.0f, 0.25f, 0.0f};
    float     velocitySpread  = 0.2f;
    float     radialVelocityMin = 0.0f;
    float     radialVelocityMax = 0.0f;
    float     tangentVelocity   = 0.0f;
    float     drag              = 0.15f;

    float spinMin = -0.8f;
    float spinMax = 0.8f;
    float sizeRandomness = 0.15f;

    float hueVariation   = 0.0f;
    float valueVariation = 0.0f;

    // Shape controls. Extents are in local emitter units.
    float     sphereRadius   = 0.5f;
    glm::vec3 boxExtents     = {0.5f, 0.5f, 0.5f};
    float     discRadius     = 0.5f;
    float     ringInnerRadius = 0.25f;
    float     ringOuterRadius = 0.65f;
    float     cylinderRadius = 0.5f;
    float     cylinderHeight = 1.0f;

    uint32_t randomSeed = 1u;
};

