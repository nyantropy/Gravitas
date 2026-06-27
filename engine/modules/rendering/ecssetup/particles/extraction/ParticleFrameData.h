#pragma once

#include <cstdint>
#include <vector>

#include "GlmConfig.h"
#include "ParticleTypes.h"
#include "Types.h"

struct ParticleInstance
{
    glm::vec3 worldPosition = {0.0f, 0.0f, 0.0f};
    glm::vec2 size          = {1.0f, 1.0f};
    glm::vec4 color         = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 uvRect        = {0.0f, 0.0f, 1.0f, 1.0f};
    float     rotation      = 0.0f;
    float     depth         = 0.0f;
    float     softness      = 0.0f;
    float     spriteEdgeSoftness = 1.0f;
    float     importance    = 1.0f;
    ParticleSpriteShape spriteShape = ParticleSpriteShape::SoftCircle;
};

struct ParticleDrawCommand
{
    texture_id_type    textureID      = 0;
    ParticleBlendMode  blendMode      = ParticleBlendMode::Alpha;
    uint32_t           instanceOffset = 0;
    uint32_t           instanceCount  = 0;
};

struct ParticleMeshInstance
{
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec4 color       = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
    float     depth       = 0.0f;
    float     importance  = 1.0f;
};

struct ParticleMeshDrawCommand
{
    mesh_id_type       meshID         = 0;
    texture_id_type    textureID      = 0;
    ParticleBlendMode  blendMode      = ParticleBlendMode::Alpha;
    uint32_t           instanceOffset = 0;
    uint32_t           instanceCount  = 0;
};

struct ParticleFrameData
{
    view_id_type cameraViewID = 0;
    uint32_t     emitterCount = 0;
    uint32_t     visibleEmitterCount = 0;
    uint32_t     culledEmitterCount = 0;
    uint32_t     simulatedParticleCount = 0;
    uint32_t     renderedParticleCount = 0;
    uint32_t     budgetClippedParticleCount = 0;
    uint32_t     collisionEventCount = 0;
    uint32_t     deathEventCount = 0;
    uint32_t     eventSpawnedParticleCount = 0;
    uint32_t     renderBudget = 0;

    std::vector<ParticleInstance>       instances;
    std::vector<ParticleDrawCommand>    drawCommands;
    std::vector<ParticleMeshInstance>   meshInstances;
    std::vector<ParticleMeshDrawCommand> meshDrawCommands;

    bool empty() const
    {
        return drawCommands.empty() && meshDrawCommands.empty();
    }

    void clear()
    {
        cameraViewID = 0;
        emitterCount = 0;
        visibleEmitterCount = 0;
        culledEmitterCount = 0;
        simulatedParticleCount = 0;
        renderedParticleCount = 0;
        budgetClippedParticleCount = 0;
        collisionEventCount = 0;
        deathEventCount = 0;
        eventSpawnedParticleCount = 0;
        renderBudget = 0;
        instances.clear();
        drawCommands.clear();
        meshInstances.clear();
        meshDrawCommands.clear();
    }
};

struct ParticleFrameDataComponent
{
    ParticleFrameData frameData;
};
