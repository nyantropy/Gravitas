#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "CameraGpuComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "GraphicsConstants.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterMath.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ParticleFrameData.h"
#include "TransformComponent.h"

class ParticleEmitterSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        ParticleFrameDataComponent& frameComponent = ensureFrameData(ctx.world);
        ParticleFrameData& frameData = frameComponent.frameData;
        frameData.clear();

        removeOrphanRuntimes(ctx.world);
        ensureRuntimes(ctx.world);

        const float dt = resolveDeltaTime(ctx);
        const CameraInfo camera = resolveActiveCamera(ctx.world);
        frameData.cameraViewID = camera.viewID;

        alphaParticles.clear();
        additiveParticles.clear();
        alphaMeshParticles.clear();
        additiveMeshParticles.clear();

        ctx.world.forEach<ParticleEmitterComponent,
                          ParticleEmitterRuntimeComponent,
                          TransformComponent>(
            [&](Entity,
                ParticleEmitterComponent& emitter,
                ParticleEmitterRuntimeComponent& runtime,
                TransformComponent& transform)
            {
                frameData.emitterCount += 1;
                updateEmitter(ctx, emitter, runtime, transform, dt);

                if (camera.viewID == 0 || runtime.textureID == 0)
                    return;

                extractEmitter(emitter, runtime, transform, camera);
            });

        buildFrameData(frameData);
    }

private:
    struct CameraInfo
    {
        view_id_type viewID = 0;
        glm::mat4    viewMatrix = glm::mat4(1.0f);
    };

    struct ExtractedParticle
    {
        ParticleInstance instance;
        texture_id_type  textureID = 0;
        ParticleBlendMode blendMode = ParticleBlendMode::Alpha;
    };

    struct ExtractedMeshParticle
    {
        ParticleMeshInstance instance;
        mesh_id_type     meshID = 0;
        texture_id_type  textureID = 0;
        ParticleBlendMode blendMode = ParticleBlendMode::Alpha;
    };

    std::vector<ExtractedParticle> alphaParticles;
    std::vector<ExtractedParticle> additiveParticles;
    std::vector<ExtractedMeshParticle> alphaMeshParticles;
    std::vector<ExtractedMeshParticle> additiveMeshParticles;

    static ParticleFrameDataComponent& ensureFrameData(ECSWorld& world)
    {
        if (!world.hasAny<ParticleFrameDataComponent>())
            return world.createSingleton<ParticleFrameDataComponent>();
        return world.getSingleton<ParticleFrameDataComponent>();
    }

    static void removeOrphanRuntimes(ECSWorld& world)
    {
        auto& commands = world.commands();
        world.forEachSnapshot<ParticleEmitterRuntimeComponent>(
            [&](Entity entity, ParticleEmitterRuntimeComponent&)
            {
                if (!world.hasComponent<ParticleEmitterComponent>(entity))
                    commands.removeComponent<ParticleEmitterRuntimeComponent>(entity);
            });
    }

    static void ensureRuntimes(ECSWorld& world)
    {
        auto& commands = world.commands();
        world.forEachSnapshot<ParticleEmitterComponent>(
            [&](Entity entity, ParticleEmitterComponent& emitter)
            {
                if (world.hasComponent<ParticleEmitterRuntimeComponent>(entity))
                    return;

                ParticleEmitterRuntimeComponent runtime;
                runtime.rngState = emitter.randomSeed == 0u ? entity.id + 1u : emitter.randomSeed;
                runtime.particles.reserve(emitter.maxParticles);
                commands.addComponent<ParticleEmitterRuntimeComponent>(entity, runtime);
            });
    }

    static float resolveDeltaTime(const EcsControllerContext& ctx)
    {
        if (ctx.time == nullptr)
            return 1.0f / 60.0f;

        const float dt = ctx.time->unscaledDeltaTime > 0.0f
            ? ctx.time->unscaledDeltaTime
            : ctx.time->deltaTime;
        return glm::clamp(dt, 0.0f, 0.1f);
    }

    static CameraInfo resolveActiveCamera(ECSWorld& world)
    {
        CameraInfo info;
        world.forEach<CameraGpuComponent>(
            [&](Entity, CameraGpuComponent& camera)
            {
                if (info.viewID != 0 || !camera.active)
                    return;

                info.viewID = camera.viewID;
                info.viewMatrix = camera.viewMatrix;
            });
        return info;
    }

    static void updateEmitter(const EcsControllerContext& ctx,
                              const ParticleEmitterComponent& emitter,
                              ParticleEmitterRuntimeComponent& runtime,
                              const TransformComponent& transform,
                              float dt)
    {
        bindResourcesIfNeeded(ctx, emitter, runtime);
        const float previousEmitterAge = runtime.emitterAge;
        runtime.emitterAge += dt;

        bool loopWrapped = false;
        float previousLocalAge = std::max(0.0f, previousEmitterAge - emitter.startDelay);
        float localAge = std::max(0.0f, runtime.emitterAge - emitter.startDelay);
        if (emitter.looping && emitter.duration > 0.0f)
        {
            previousLocalAge = std::fmod(previousLocalAge, emitter.duration);
            localAge = std::fmod(localAge, emitter.duration);
            loopWrapped = localAge < previousLocalAge;
            if (loopWrapped)
                runtime.burstRepeatCounts.clear();
        }

        const uint32_t maxParticles = std::max(1u, emitter.maxParticles);
        if (runtime.particles.capacity() < maxParticles)
            runtime.particles.reserve(maxParticles);
        if (runtime.particles.size() > maxParticles)
            runtime.particles.resize(maxParticles);
        if (runtime.burstRepeatCounts.size() != emitter.bursts.size())
            runtime.burstRepeatCounts.assign(emitter.bursts.size(), 0u);

        for (size_t i = 0; i < runtime.particles.size();)
        {
            ParticleState& particle = runtime.particles[i];
            particle.age += dt;
            if (particle.age >= particle.lifetime)
            {
                particle = runtime.particles.back();
                runtime.particles.pop_back();
                continue;
            }

            applyForces(emitter, transform, particle, dt);
            const float dragFactor = std::max(0.0f, 1.0f - emitter.drag * dt);
            particle.velocity *= dragFactor;
            particle.position += particle.velocity * dt;
            particle.rotation += particle.spin * dt;
            particle.meshRotation += particle.meshSpin * dt;
            ++i;
        }

        if (!emitter.enabled || emitter.intensity <= 0.0f)
            return;
        if (runtime.emitterAge < emitter.startDelay)
            return;
        if (!emitter.looping && emitter.duration > 0.0f && localAge > emitter.duration)
            return;

        if (emitter.emissionRate > 0.0f)
        {
            runtime.spawnAccumulator += emitter.emissionRate * emitter.intensity * dt;
            uint32_t spawnCount = static_cast<uint32_t>(runtime.spawnAccumulator);
            runtime.spawnAccumulator -= static_cast<float>(spawnCount);
            spawnParticles(emitter, runtime, transform, spawnCount, maxParticles);
        }

        spawnBursts(emitter,
                    runtime,
                    transform,
                    previousLocalAge,
                    localAge,
                    loopWrapped,
                    maxParticles);
    }

    static void spawnParticles(const ParticleEmitterComponent& emitter,
                               ParticleEmitterRuntimeComponent& runtime,
                               const TransformComponent& transform,
                               uint32_t count,
                               uint32_t maxParticles)
    {
        const uint32_t available = maxParticles - static_cast<uint32_t>(runtime.particles.size());
        count = std::min(count, available);

        for (uint32_t i = 0; i < count; ++i)
            spawnParticle(emitter, runtime, transform);
    }

    static void spawnBursts(const ParticleEmitterComponent& emitter,
                            ParticleEmitterRuntimeComponent& runtime,
                            const TransformComponent& transform,
                            float previousLocalAge,
                            float localAge,
                            bool loopWrapped,
                            uint32_t maxParticles)
    {
        for (size_t i = 0; i < emitter.bursts.size(); ++i)
        {
            const ParticleBurst& burst = emitter.bursts[i];
            uint32_t& repeatIndex = runtime.burstRepeatCounts[i];
            const uint32_t maxRepeats = std::max(1u, burst.repeatCount + 1u);

            while (repeatIndex < maxRepeats)
            {
                const float triggerTime = burst.time
                    + static_cast<float>(repeatIndex) * std::max(0.0f, burst.repeatInterval);
                const bool firstBurstWindow = repeatIndex == 0u && previousLocalAge <= 0.0f;
                const bool triggered = loopWrapped
                    ? (triggerTime > previousLocalAge || triggerTime <= localAge)
                    : ((triggerTime > previousLocalAge && triggerTime <= localAge)
                        || (firstBurstWindow && triggerTime <= localAge));
                if (!triggered)
                    break;

                const uint32_t count = burst.countMax > burst.countMin
                    ? static_cast<uint32_t>(
                        ParticleEmitterMath::randomRange(runtime.rngState,
                                                         static_cast<float>(burst.countMin),
                                                         static_cast<float>(burst.countMax + 1u)))
                    : burst.countMin;
                spawnParticles(emitter, runtime, transform, count, maxParticles);
                repeatIndex += 1;

                if (burst.repeatInterval <= 0.0f)
                    break;
            }
        }
    }

    static void applyForces(const ParticleEmitterComponent& emitter,
                            const TransformComponent& transform,
                            ParticleState& particle,
                            float dt)
    {
        glm::vec3 acceleration = emitter.forces.acceleration + emitter.forces.wind;
        const glm::vec3 localPosition = emitter.localSpace
            ? particle.position
            : glm::vec3(glm::inverse(transform.getModelMatrix()) * glm::vec4(particle.position, 1.0f));
        glm::vec3 radial = ParticleEmitterMath::radialDirection(localPosition);

        if (emitter.forces.radial != 0.0f)
            acceleration += radial * emitter.forces.radial;

        if (emitter.forces.vortex != 0.0f)
            acceleration += ParticleEmitterMath::tangentDirection(radial) * emitter.forces.vortex;

        if (emitter.forces.noiseStrength != 0.0f)
        {
            const float scale = std::max(0.0001f, emitter.forces.noiseScale);
            const float n1 = std::sin((localPosition.x + particle.age) * 12.9898f * scale);
            const float n2 = std::sin((localPosition.y + particle.age) * 78.2330f * scale);
            const float n3 = std::sin((localPosition.z + particle.age) * 37.7190f * scale);
            acceleration += glm::normalize(glm::vec3(n1, n2, n3) + glm::vec3(0.001f))
                * emitter.forces.noiseStrength;
        }

        particle.velocity += acceleration * dt;
    }

    static void bindResourcesIfNeeded(const EcsControllerContext& ctx,
                                      const ParticleEmitterComponent& emitter,
                                      ParticleEmitterRuntimeComponent& runtime)
    {
        if (ctx.resources == nullptr)
            return;

        const std::string texturePath = emitter.texturePath.empty()
            ? GraphicsConstants::ENGINE_RESOURCES + "/textures/engine_particle_fallback.png"
            : emitter.texturePath;

        if (runtime.textureID == 0 || runtime.boundTexturePath != texturePath)
        {
            runtime.textureID = ctx.resources->requestTexture(texturePath);
            runtime.boundTexturePath = texturePath;
        }

        if (emitter.primitive != ParticlePrimitive::Mesh)
        {
            runtime.meshID = 0;
            runtime.boundMeshPath.clear();
            return;
        }

        if (emitter.meshPath.empty())
        {
            runtime.meshID = 0;
            runtime.boundMeshPath.clear();
            return;
        }

        if (runtime.meshID != 0 && runtime.boundMeshPath == emitter.meshPath)
            return;

        runtime.meshID = ctx.resources->requestMesh(emitter.meshPath);
        runtime.boundMeshPath = emitter.meshPath;
    }

    static void spawnParticle(const ParticleEmitterComponent& emitter,
                              ParticleEmitterRuntimeComponent& runtime,
                              const TransformComponent& transform)
    {
        ParticleState particle;
        const float minLifetime = std::max(0.01f, std::min(emitter.lifetimeMin, emitter.lifetimeMax));
        const float maxLifetime = std::max(minLifetime, std::max(emitter.lifetimeMin, emitter.lifetimeMax));
        particle.lifetime = ParticleEmitterMath::randomRange(runtime.rngState, minLifetime, maxLifetime);

        const glm::vec3 localPosition = ParticleEmitterMath::sampleShape(emitter, runtime.rngState);
        const glm::vec3 radial = ParticleEmitterMath::radialDirection(localPosition);
        const glm::vec3 tangent = ParticleEmitterMath::tangentDirection(radial);
        const float radialSpeed =
            ParticleEmitterMath::randomRange(runtime.rngState, emitter.radialVelocityMin, emitter.radialVelocityMax);

        glm::vec3 velocity = emitter.initialVelocity;
        velocity += ParticleEmitterMath::randomUnitVector(runtime.rngState) * emitter.velocitySpread;
        velocity += radial * radialSpeed;
        velocity += tangent * emitter.tangentVelocity;

        if (emitter.localSpace)
        {
            particle.position = localPosition;
            particle.velocity = velocity;
        }
        else
        {
            const glm::mat4 model = transform.getModelMatrix();
            particle.position = glm::vec3(model * glm::vec4(localPosition, 1.0f));
            particle.velocity = glm::mat3(model) * velocity;
        }

        particle.tint = ParticleEmitterMath::applyColorVariation(emitter.baseTint,
                                                                 emitter.hueVariation,
                                                                 emitter.valueVariation,
                                                                 runtime.rngState);
        particle.rotation = ParticleEmitterMath::randomRange(
            runtime.rngState, 0.0f, ParticleEmitterMath::Pi * 2.0f);
        particle.spin = ParticleEmitterMath::randomRange(runtime.rngState, emitter.spinMin, emitter.spinMax);
        particle.sizeScale =
            ParticleEmitterMath::randomRange(
                runtime.rngState, 1.0f - emitter.sizeRandomness, 1.0f + emitter.sizeRandomness);
        const float minAspect = std::max(0.01f, std::min(emitter.aspectRatioMin, emitter.aspectRatioMax));
        const float maxAspect = std::max(minAspect, std::max(emitter.aspectRatioMin, emitter.aspectRatioMax));
        particle.aspectRatio = ParticleEmitterMath::randomRange(runtime.rngState, minAspect, maxAspect);
        particle.meshRotation = emitter.randomMeshRotation
            ? glm::vec3{
                ParticleEmitterMath::randomRange(runtime.rngState, 0.0f, ParticleEmitterMath::Pi * 2.0f),
                ParticleEmitterMath::randomRange(runtime.rngState, 0.0f, ParticleEmitterMath::Pi * 2.0f),
                ParticleEmitterMath::randomRange(runtime.rngState, 0.0f, ParticleEmitterMath::Pi * 2.0f)}
            : glm::vec3{0.0f, 0.0f, 0.0f};
        particle.meshSpin = {
            ParticleEmitterMath::randomRange(
                runtime.rngState, emitter.meshAngularVelocityMin.x, emitter.meshAngularVelocityMax.x),
            ParticleEmitterMath::randomRange(
                runtime.rngState, emitter.meshAngularVelocityMin.y, emitter.meshAngularVelocityMax.y),
            ParticleEmitterMath::randomRange(
                runtime.rngState, emitter.meshAngularVelocityMin.z, emitter.meshAngularVelocityMax.z)
        };
        particle.frameOffset = emitter.flipbook.randomStart
            ? ParticleEmitterMath::randomRange(
                runtime.rngState, 0.0f, static_cast<float>(std::max(1u, emitter.flipbook.frameCount)))
            : 0.0f;

        runtime.particles.push_back(particle);
    }

    void extractEmitter(const ParticleEmitterComponent& emitter,
                        const ParticleEmitterRuntimeComponent& runtime,
                        const TransformComponent& transform,
                        const CameraInfo& camera)
    {
        const glm::mat4 model = emitter.localSpace ? transform.getModelMatrix() : glm::mat4(1.0f);

        for (const ParticleState& particle : runtime.particles)
        {
            const float normalizedAge = glm::clamp(particle.age / std::max(0.01f, particle.lifetime), 0.0f, 1.0f);
            glm::vec4 color =
                particle.tint * ParticleEmitterMath::evaluateColorCurve(emitter.colorOverLifetime, normalizedAge);
            color.a *= ParticleEmitterMath::evaluateFloatCurve(emitter.alphaOverLifetime, normalizedAge, 1.0f);
            color.a *= emitter.intensity;
            color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
            if (color.a <= 0.001f)
                continue;

            const glm::vec3 worldPosition = glm::vec3(model * glm::vec4(particle.position, 1.0f));
            const glm::vec4 viewPosition = camera.viewMatrix * glm::vec4(worldPosition, 1.0f);
            const float particleSize =
                std::max(0.001f,
                         ParticleEmitterMath::evaluateFloatCurve(emitter.sizeOverLifetime, normalizedAge, 0.5f))
                * particle.sizeScale;

            if (emitter.primitive == ParticlePrimitive::Mesh)
            {
                if (runtime.meshID == 0)
                    continue;

                glm::mat4 particleModel = emitter.localSpace ? transform.getModelMatrix() : glm::mat4(1.0f);
                particleModel = glm::translate(particleModel, particle.position);
                particleModel = glm::rotate(particleModel, particle.meshRotation.x, {1.0f, 0.0f, 0.0f});
                particleModel = glm::rotate(particleModel, particle.meshRotation.y, {0.0f, 1.0f, 0.0f});
                particleModel = glm::rotate(particleModel, particle.meshRotation.z, {0.0f, 0.0f, 1.0f});
                particleModel = glm::scale(particleModel, glm::max(emitter.meshScale * particleSize,
                                                                    glm::vec3(0.001f)));

                ParticleMeshInstance instance;
                instance.modelMatrix = particleModel;
                instance.color = color;
                instance.depth = -viewPosition.z;

                ExtractedMeshParticle extracted;
                extracted.instance = instance;
                extracted.meshID = runtime.meshID;
                extracted.textureID = runtime.textureID;
                extracted.blendMode = emitter.blend;

                if (emitter.blend == ParticleBlendMode::Additive)
                    additiveMeshParticles.push_back(extracted);
                else
                    alphaMeshParticles.push_back(extracted);
                continue;
            }

            ParticleInstance instance;
            instance.worldPosition = worldPosition;
            instance.size = {particleSize * particle.aspectRatio, particleSize};
            instance.color = color;
            instance.uvRect = ParticleEmitterMath::computeFlipbookUv(emitter, particle, normalizedAge);
            instance.rotation = particle.rotation;
            instance.depth = -viewPosition.z;
            instance.softness = std::max(0.0f, emitter.softness);
            instance.spriteEdgeSoftness = glm::clamp(emitter.spriteEdgeSoftness, 0.0f, 1.0f);
            instance.spriteShape = emitter.spriteShape;

            ExtractedParticle extracted;
            extracted.instance = instance;
            extracted.textureID = runtime.textureID;
            extracted.blendMode = emitter.blend;

            if (emitter.blend == ParticleBlendMode::Additive)
                additiveParticles.push_back(extracted);
            else
                alphaParticles.push_back(extracted);
        }
    }

    void buildFrameData(ParticleFrameData& frameData)
    {
        std::sort(alphaParticles.begin(), alphaParticles.end(),
            [](const ExtractedParticle& lhs, const ExtractedParticle& rhs)
            {
                return lhs.instance.depth > rhs.instance.depth;
            });

        std::sort(additiveParticles.begin(), additiveParticles.end(),
            [](const ExtractedParticle& lhs, const ExtractedParticle& rhs)
            {
                if (lhs.textureID != rhs.textureID)
                    return lhs.textureID < rhs.textureID;
                return lhs.instance.depth > rhs.instance.depth;
            });

        std::sort(alphaMeshParticles.begin(), alphaMeshParticles.end(),
            [](const ExtractedMeshParticle& lhs, const ExtractedMeshParticle& rhs)
            {
                return lhs.instance.depth > rhs.instance.depth;
            });

        std::sort(additiveMeshParticles.begin(), additiveMeshParticles.end(),
            [](const ExtractedMeshParticle& lhs, const ExtractedMeshParticle& rhs)
            {
                if (lhs.meshID != rhs.meshID)
                    return lhs.meshID < rhs.meshID;
                if (lhs.textureID != rhs.textureID)
                    return lhs.textureID < rhs.textureID;
                return lhs.instance.depth > rhs.instance.depth;
            });

        frameData.instances.reserve(alphaParticles.size() + additiveParticles.size());
        frameData.drawCommands.reserve(alphaParticles.size() + additiveParticles.size());
        frameData.meshInstances.reserve(alphaMeshParticles.size() + additiveMeshParticles.size());
        frameData.meshDrawCommands.reserve(alphaMeshParticles.size() + additiveMeshParticles.size());

        appendParticles(frameData, alphaParticles);
        appendParticles(frameData, additiveParticles);
        appendMeshParticles(frameData, alphaMeshParticles);
        appendMeshParticles(frameData, additiveMeshParticles);
    }

    static void appendParticles(ParticleFrameData& frameData,
                                const std::vector<ExtractedParticle>& particles)
    {
        for (const ExtractedParticle& particle : particles)
        {
            const uint32_t instanceIndex = static_cast<uint32_t>(frameData.instances.size());
            frameData.instances.push_back(particle.instance);

            if (!frameData.drawCommands.empty())
            {
                ParticleDrawCommand& last = frameData.drawCommands.back();
                if (last.textureID == particle.textureID && last.blendMode == particle.blendMode)
                {
                    last.instanceCount += 1;
                    continue;
                }
            }

            frameData.drawCommands.push_back({
                particle.textureID,
                particle.blendMode,
                instanceIndex,
                1u
            });
        }
    }

    static void appendMeshParticles(ParticleFrameData& frameData,
                                    const std::vector<ExtractedMeshParticle>& particles)
    {
        for (const ExtractedMeshParticle& particle : particles)
        {
            const uint32_t instanceIndex = static_cast<uint32_t>(frameData.meshInstances.size());
            frameData.meshInstances.push_back(particle.instance);

            if (!frameData.meshDrawCommands.empty())
            {
                ParticleMeshDrawCommand& last = frameData.meshDrawCommands.back();
                if (last.meshID == particle.meshID
                    && last.textureID == particle.textureID
                    && last.blendMode == particle.blendMode)
                {
                    last.instanceCount += 1;
                    continue;
                }
            }

            frameData.meshDrawCommands.push_back({
                particle.meshID,
                particle.textureID,
                particle.blendMode,
                instanceIndex,
                1u
            });
        }
    }
};
