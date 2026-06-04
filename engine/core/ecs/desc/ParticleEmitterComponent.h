#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GlmConfig.h"
#include "ParticleTypes.h"

// gameplay-facing particle emitter descriptor
// pair with TransformComponent
// engine particle systems own runtime state and backend resources
struct ParticleEmitterComponent
{
    // high-level emitter switches
    bool enabled    = true;
    bool localSpace = true;
    bool looping    = true;
    bool reloadFromEffect = true;

    // where particles spawn, and how they blend in the particle render stage
    ParticleEmitterShape shape = ParticleEmitterShape::Sphere;
    ParticleBlendMode    blend = ParticleBlendMode::Alpha;

    // optional json source plus particle texture
    std::string effectPath;
    std::string texturePath;

    // base color multiplier before lifetime/random color changes
    glm::vec4   baseTint = {1.0f, 1.0f, 1.0f, 1.0f};

    // lifetime curves are evaluated with normalized particle age from 0..1
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

    // emission timing and particle lifetime
    float    emissionRate  = 32.0f;
    uint32_t maxParticles  = 128;
    float    lifetimeMin   = 1.0f;
    float    lifetimeMax   = 2.0f;
    float    duration      = 0.0f;
    float    startDelay    = 0.0f;
    float    intensity     = 1.0f;

    // initial movement at spawn
    glm::vec3 initialVelocity = {0.0f, 0.25f, 0.0f};
    float     velocitySpread  = 0.2f;
    float     radialVelocityMin = 0.0f;
    float     radialVelocityMax = 0.0f;
    float     tangentVelocity   = 0.0f;
    float     drag              = 0.15f;

    // per-particle visual variation
    float spinMin = -0.8f;
    float spinMax = 0.8f;
    float sizeRandomness = 0.15f;
    float softness = 80.0f;

    float hueVariation   = 0.0f;
    float valueVariation = 0.0f;

    // shape controls, in local emitter units
    float     sphereRadius   = 0.5f;
    glm::vec3 boxExtents     = {0.5f, 0.5f, 0.5f};
    float     discRadius     = 0.5f;
    float     ringInnerRadius = 0.25f;
    float     ringOuterRadius = 0.65f;
    float     cylinderRadius = 0.5f;
    float     cylinderHeight = 1.0f;

    // optional one-shot/repeating bursts, sprite atlas animation, and forces
    std::vector<ParticleBurst> bursts;
    ParticleFlipbook           flipbook;
    ParticleForceModule        forces;

    // 0 means the runtime picks a stable seed from the entity id
    uint32_t randomSeed = 1u;
};
