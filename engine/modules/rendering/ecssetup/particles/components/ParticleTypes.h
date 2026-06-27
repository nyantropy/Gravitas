#pragma once

#include <cstdint>
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

enum class ParticlePrimitive
{
    Billboard = 0,
    Mesh
};

enum class ParticleSpriteShape
{
    SoftCircle = 0,
    Square,
    Diamond,
    Petal,
    Streak
};

enum class ParticleCollisionMode
{
    None = 0,
    GroundPlane
};

enum class ParticleCullReason : uint32_t
{
    None = 0,
    Frustum = 1,
    Distance = 2,
    Budget = 3
};

struct ParticleColorKey
{
    // t is normalized particle lifetime from 0..1
    float     t     = 0.0f;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ParticleFloatKey
{
    // t is normalized particle lifetime from 0..1
    float t     = 0.0f;
    float value = 1.0f;
};

struct ParticleBurst
{
    // extra particles spawned at a specific emitter age
    float    time           = 0.0f;
    uint32_t countMin       = 0;
    uint32_t countMax       = 0;

    // repeatInterval/repeatCount make the burst fire more than once
    float    repeatInterval = 0.0f;
    uint32_t repeatCount    = 0;
};

struct ParticleFlipbook
{
    // texture atlas layout, selected row-major from top-left
    uint32_t columns        = 1;
    uint32_t rows           = 1;
    uint32_t frameCount     = 1;
    float    frameRate      = 12.0f;

    // lifetimeDriven maps particle age to frame; otherwise frameRate advances it
    bool     lifetimeDriven = true;
    bool     randomStart    = false;
};

struct ParticleForceModule
{
    // constant world/local acceleration added every frame
    glm::vec3 acceleration  = {0.0f, 0.0f, 0.0f};
    glm::vec3 wind          = {0.0f, 0.0f, 0.0f};

    // simple procedural forces around/away from the emitter
    float     vortex        = 0.0f;
    float     radial        = 0.0f;

    // random-looking drift; noiseScale controls how quickly the field changes
    float     noiseStrength = 0.0f;
    float     noiseScale    = 1.0f;
};

struct ParticleRuntimePolicy
{
    // Effect-wide scaling for artist-authored variants and gameplay intensity.
    float effectScale = 1.0f;

    // Higher importance keeps more simulation/render budget when global limits apply.
    float    importance  = 1.0f;
    uint32_t budgetWeight = 1;

    // 0 means unlimited; otherwise clamps particles spawned by this emitter per frame.
    uint32_t maxSpawnPerFrame = 0;

    // Culling controls. Frustum culling is render-only by default; simulation keeps running.
    bool  frustumCulling = true;
    bool  distanceCulling = false;
    bool  simulateWhenCulled = true;
    float cullPadding = 0.5f;
    float maxDrawDistance = 0.0f;

    // Distance LOD scales spawn rate and rendered particle size between near and far.
    float lodNearDistance = 12.0f;
    float lodFarDistance = 42.0f;
    float lodMinSpawnScale = 0.35f;
    float lodMinRenderScale = 0.50f;

    // Billboard velocity stretching. 0 disables stretching.
    float velocityStretch = 0.0f;
    float velocityStretchMax = 3.0f;

    // Forward-looking material/lighting/soft-particle knobs consumed by current/future backends.
    float lightingInfluence = 0.0f;
    float meshSoftness = 0.0f;
};

struct ParticleCollisionPolicy
{
    ParticleCollisionMode mode = ParticleCollisionMode::None;
    float groundY = 0.0f;
    float bounce = 0.35f;
    float damping = 0.70f;
    bool  killOnCollision = false;

    // First event/sub-emitter backend: spawn additional particles from this emitter.
    uint32_t spawnOnDeathCount = 0;
    uint32_t spawnOnCollisionCount = 0;
    uint32_t maxEventSpawnsPerFrame = 32;
};

struct ParticleBudgetState
{
    uint32_t maxSimulatedParticles = 0;
    uint32_t maxRenderedParticles = 0;
    uint32_t maxSpawnedPerFrame = 0;

    uint32_t requestedSimulatedParticles = 0;
    uint32_t activeParticles = 0;
    uint32_t spawnedParticles = 0;
    uint32_t renderedParticles = 0;
    uint32_t culledEmitters = 0;
    uint32_t budgetClippedEmitters = 0;
    uint32_t collisionEvents = 0;
    uint32_t deathEvents = 0;
    uint32_t eventSpawnedParticles = 0;

    void resetFrameStats()
    {
        requestedSimulatedParticles = 0;
        activeParticles = 0;
        spawnedParticles = 0;
        renderedParticles = 0;
        culledEmitters = 0;
        budgetClippedEmitters = 0;
        collisionEvents = 0;
        deathEvents = 0;
        eventSpawnedParticles = 0;
    }
};

using ParticleColorCurve = std::vector<ParticleColorKey>;
using ParticleFloatCurve = std::vector<ParticleFloatKey>;
