#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GlmConfig.h"
#include "ParticleTypes.h"
#include "Types.h"

struct ParticleState
{
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
    glm::vec4 tint     = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 meshRotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 meshSpin     = {0.0f, 0.0f, 0.0f};

    float age       = 0.0f;
    float lifetime  = 1.0f;
    float rotation  = 0.0f;
    float spin      = 0.0f;
    float sizeScale = 1.0f;
    float aspectRatio = 1.0f;
    float frameOffset = 0.0f;
};

struct ParticleEmitterRuntimeComponent
{
    std::vector<ParticleState> particles;
    float spawnAccumulator = 0.0f;
    float emitterAge = 0.0f;
    bool playbackPaused = false;
    float playbackTimeScale = 1.0f;
    uint32_t rngState = 1u;
    std::vector<uint32_t> burstRepeatCounts;

    texture_id_type textureID = 0;
    mesh_id_type meshID = 0;
    std::string boundTexturePath;
    std::string boundMeshPath;
    uint64_t appliedEffectVersion = 0;

    bool hasBounds = false;
    bool visible = true;
    ParticleCullReason cullReason = ParticleCullReason::None;
    glm::vec3 boundsMin = {0.0f, 0.0f, 0.0f};
    glm::vec3 boundsMax = {0.0f, 0.0f, 0.0f};
    glm::vec3 boundsCenter = {0.0f, 0.0f, 0.0f};
    float boundsRadius = 0.0f;
    float distanceToCamera = 0.0f;
    float importanceScore = 1.0f;
    float lodSpawnScale = 1.0f;
    float lodRenderScale = 1.0f;
    uint32_t budgetedMaxParticles = 0;
    uint32_t budgetedSpawnPerFrame = 0;

    uint32_t spawnedThisFrame = 0;
    uint32_t diedThisFrame = 0;
    uint32_t collisionEventsThisFrame = 0;
    uint32_t eventSpawnsThisFrame = 0;
    uint32_t budgetSkippedSpawnsThisFrame = 0;
};

struct ParticleBudgetComponent
{
    ParticleBudgetState state;
};
