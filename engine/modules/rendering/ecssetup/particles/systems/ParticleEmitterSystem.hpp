#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "CameraGpuComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "FrustumCuller.h"
#include "GraphicsConstants.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterMath.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ParticleFrameData.h"
#include "RenderCameraSelectionComponent.h"
#include "TransformMatrixHelpers.h"
#include "WorldTransformComponent.h"

class ParticleEmitterSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        ParticleFrameDataComponent& frameComponent = ensureFrameData(ctx.world);
        ParticleBudgetComponent& budgetComponent = ensureBudget(ctx.world);
        ParticleBudgetState& budget = budgetComponent.state;
        budget.resetFrameStats();

        ParticleFrameData& frameData = frameComponent.frameData;
        frameData.clear();

        removeOrphanRuntimes(ctx.world);
        ensureRuntimes(ctx.world);

        const float dt = resolveDeltaTime(ctx);
        const CameraInfo camera = resolveActiveCamera(ctx.world);
        const BudgetFrame budgetFrame = buildBudgetFrame(ctx.world, budget);
        frameData.cameraViewID = camera.viewID;
        frameData.renderBudget = budget.maxRenderedParticles;

        alphaParticles.clear();
        additiveParticles.clear();
        alphaMeshParticles.clear();
        additiveMeshParticles.clear();

        ctx.world.forEach<ParticleEmitterComponent,
                          ParticleEmitterRuntimeComponent,
                          WorldTransformComponent>(
            [&](Entity,
                ParticleEmitterComponent& emitter,
                ParticleEmitterRuntimeComponent& runtime,
                WorldTransformComponent& worldTransform)
            {
                const EmitterTransform emitterTransform = makeEmitterTransform(worldTransform);
                frameData.emitterCount += 1;
                resetRuntimeFrameStats(runtime);
                const EmitterFramePolicy policy =
                    resolveEmitterFramePolicy(emitter, runtime, emitterTransform, camera, budgetFrame);
                const float emitterDt =
                    runtime.playbackPaused ? 0.0f : dt * std::max(0.0f, runtime.playbackTimeScale);
                updateEmitter(ctx, emitter, runtime, emitterTransform, emitterDt, policy);
                refreshRuntimeBounds(emitter, runtime, emitterTransform);
                applyVisibilityPolicy(emitter, runtime, emitterTransform, camera);

                frameData.simulatedParticleCount += static_cast<uint32_t>(runtime.particles.size());
                frameData.collisionEventCount += runtime.collisionEventsThisFrame;
                frameData.deathEventCount += runtime.diedThisFrame;
                frameData.eventSpawnedParticleCount += runtime.eventSpawnsThisFrame;
                budget.activeParticles += static_cast<uint32_t>(runtime.particles.size());
                budget.spawnedParticles += runtime.spawnedThisFrame;
                budget.collisionEvents += runtime.collisionEventsThisFrame;
                budget.deathEvents += runtime.diedThisFrame;
                budget.eventSpawnedParticles += runtime.eventSpawnsThisFrame;
                if (runtime.budgetSkippedSpawnsThisFrame > 0u)
                    budget.budgetClippedEmitters += 1u;

                if (camera.viewID == 0 || runtime.textureID == 0)
                    return;
                if (!runtime.visible)
                {
                    frameData.culledEmitterCount += 1;
                    budget.culledEmitters += 1u;
                    return;
                }

                frameData.visibleEmitterCount += 1;
                extractEmitter(emitter, runtime, emitterTransform, camera);
            });

        buildFrameData(frameData, budget);
    }

private:
    struct CameraInfo
    {
        view_id_type viewID = 0;
        glm::mat4    viewMatrix = glm::mat4(1.0f);
        glm::mat4    projMatrix = glm::mat4(1.0f);
        FrustumPlanes frustum{};
        glm::vec3    position = {0.0f, 0.0f, 0.0f};
    };

    struct EmitterTransform
    {
        glm::mat4 worldMatrix = glm::mat4(1.0f);
        glm::mat4 inverseWorldMatrix = glm::mat4(1.0f);
        glm::mat3 worldLinear = glm::mat3(1.0f);
        glm::vec3 origin = {0.0f, 0.0f, 0.0f};
    };

    struct BudgetFrame
    {
        uint32_t maxSimulatedParticles = 0;
        uint32_t maxSpawnedPerFrame = 0;
        float    totalWeight = 0.0f;
    };

    struct EmitterFramePolicy
    {
        uint32_t maxParticles = 1;
        uint32_t maxSpawnPerFrame = std::numeric_limits<uint32_t>::max();
        bool     simulate = true;
        float    spawnScale = 1.0f;
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

    static EmitterTransform makeEmitterTransform(const WorldTransformComponent& worldTransform)
    {
        EmitterTransform transform;
        transform.worldMatrix = worldTransform.matrix;
        transform.inverseWorldMatrix = glm::inverse(worldTransform.matrix);
        transform.worldLinear = glm::mat3(worldTransform.matrix);
        transform.origin = gts::transform::worldPositionFromMatrix(worldTransform.matrix);
        return transform;
    }

    static ParticleFrameDataComponent& ensureFrameData(ECSWorld& world)
    {
        if (!world.hasAny<ParticleFrameDataComponent>())
            return world.createSingleton<ParticleFrameDataComponent>();
        return world.getSingleton<ParticleFrameDataComponent>();
    }

    static ParticleBudgetComponent& ensureBudget(ECSWorld& world)
    {
        if (!world.hasAny<ParticleBudgetComponent>())
            return world.createSingleton<ParticleBudgetComponent>();
        return world.getSingleton<ParticleBudgetComponent>();
    }

    static void resetRuntimeFrameStats(ParticleEmitterRuntimeComponent& runtime)
    {
        runtime.spawnedThisFrame = 0;
        runtime.diedThisFrame = 0;
        runtime.collisionEventsThisFrame = 0;
        runtime.eventSpawnsThisFrame = 0;
        runtime.budgetSkippedSpawnsThisFrame = 0;
        runtime.cullReason = ParticleCullReason::None;
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
        const auto useCamera =
            [&](CameraGpuComponent& camera)
            {
                if (!camera.active || camera.viewID == 0)
                    return false;

                info.viewID = camera.viewID;
                info.viewMatrix = camera.viewMatrix;
                info.projMatrix = camera.projMatrix;
                info.frustum = FrustumCuller::extractPlanesFromMatrix(camera.projMatrix * camera.viewMatrix);
                const glm::mat4 inverseView = glm::inverse(camera.viewMatrix);
                info.position = glm::vec3(inverseView[3]);
                return true;
            };

        if (world.hasAny<RenderCameraSelectionComponent>())
        {
            const Entity preferred = world.getSingleton<RenderCameraSelectionComponent>().preferredCamera;
            if (preferred.id != std::numeric_limits<entity_id_type>::max()
                && world.hasComponent<CameraGpuComponent>(preferred))
            {
                CameraGpuComponent& camera = world.getComponent<CameraGpuComponent>(preferred);
                useCamera(camera);
            }
        }

        world.forEach<CameraGpuComponent>(
            [&](Entity, CameraGpuComponent& camera)
            {
                if (info.viewID != 0)
                    return;

                useCamera(camera);
            });
        return info;
    }

    static BudgetFrame buildBudgetFrame(ECSWorld& world, ParticleBudgetState& budget)
    {
        BudgetFrame frame;
        frame.maxSimulatedParticles = budget.maxSimulatedParticles;
        frame.maxSpawnedPerFrame = budget.maxSpawnedPerFrame;

        world.forEach<ParticleEmitterComponent>(
            [&](Entity, ParticleEmitterComponent& emitter)
            {
                const float weight = emitterBudgetWeight(emitter);
                frame.totalWeight += weight;
                budget.requestedSimulatedParticles += emitter.maxParticles;
            });

        return frame;
    }

    static float emitterBudgetWeight(const ParticleEmitterComponent& emitter)
    {
        const float importance = std::max(0.01f, emitter.runtime.importance);
        const float weight = static_cast<float>(std::max(1u, emitter.runtime.budgetWeight));
        return importance * weight * static_cast<float>(std::max(1u, emitter.maxParticles));
    }

    static uint32_t budgetShare(uint32_t globalBudget,
                                float totalWeight,
                                const ParticleEmitterComponent& emitter,
                                uint32_t requested)
    {
        if (globalBudget == 0u)
            return requested;
        if (totalWeight <= 0.0f)
            return std::min(requested, globalBudget);

        const float share = static_cast<float>(globalBudget) * emitterBudgetWeight(emitter) / totalWeight;
        return std::min(requested, std::max(1u, static_cast<uint32_t>(std::floor(share))));
    }

    static float distanceLodScale(float distance,
                                  float nearDistance,
                                  float farDistance,
                                  float minScale)
    {
        const float nearValue = std::max(0.0f, nearDistance);
        const float farValue = std::max(nearValue + 0.001f, farDistance);
        const float t = glm::clamp((distance - nearValue) / (farValue - nearValue), 0.0f, 1.0f);
        return glm::mix(1.0f, glm::clamp(minScale, 0.0f, 1.0f), t);
    }

    static EmitterFramePolicy resolveEmitterFramePolicy(const ParticleEmitterComponent& emitter,
                                                        ParticleEmitterRuntimeComponent& runtime,
                                                        const EmitterTransform& transform,
                                                        const CameraInfo& camera,
                                                        const BudgetFrame& budgetFrame)
    {
        refreshRuntimeBounds(emitter, runtime, transform);
        applyVisibilityPolicy(emitter, runtime, transform, camera);

        EmitterFramePolicy policy;
        policy.maxParticles = budgetShare(budgetFrame.maxSimulatedParticles,
                                          budgetFrame.totalWeight,
                                          emitter,
                                          std::max(1u, emitter.maxParticles));
        runtime.budgetedMaxParticles = policy.maxParticles;

        const uint32_t localSpawnBudget = emitter.runtime.maxSpawnPerFrame == 0u
            ? std::numeric_limits<uint32_t>::max()
            : emitter.runtime.maxSpawnPerFrame;
        const uint32_t globalSpawnShare = budgetShare(budgetFrame.maxSpawnedPerFrame,
                                                      budgetFrame.totalWeight,
                                                      emitter,
                                                      localSpawnBudget);
        policy.maxSpawnPerFrame = std::min(localSpawnBudget, globalSpawnShare);
        runtime.budgetedSpawnPerFrame = policy.maxSpawnPerFrame == std::numeric_limits<uint32_t>::max()
            ? 0u
            : policy.maxSpawnPerFrame;

        const float scale = std::max(0.0f, emitter.runtime.effectScale);
        policy.spawnScale = scale * runtime.lodSpawnScale;
        policy.simulate = runtime.visible || emitter.runtime.simulateWhenCulled;
        return policy;
    }

    static glm::vec3 transformPoint(const glm::mat4& matrix, const glm::vec3& point)
    {
        return glm::vec3(matrix * glm::vec4(point, 1.0f));
    }

    static glm::vec3 transformOrigin(const EmitterTransform& transform)
    {
        return transform.origin;
    }

    static void refreshRuntimeBounds(const ParticleEmitterComponent& emitter,
                                     ParticleEmitterRuntimeComponent& runtime,
                                     const EmitterTransform& transform)
    {
        if (runtime.particles.empty())
        {
            runtime.hasBounds = false;
            runtime.boundsMin = transformOrigin(transform);
            runtime.boundsMax = runtime.boundsMin;
            runtime.boundsCenter = runtime.boundsMin;
            runtime.boundsRadius = 0.0f;
            return;
        }

        const glm::mat4 model = emitter.localSpace ? transform.worldMatrix : glm::mat4(1.0f);
        const float padding = std::max(0.0f, emitter.runtime.cullPadding) +
            std::max(0.001f, emitter.runtime.effectScale) * maxParticleSize(emitter);
        glm::vec3 minValue(std::numeric_limits<float>::max());
        glm::vec3 maxValue(-std::numeric_limits<float>::max());

        for (const ParticleState& particle : runtime.particles)
        {
            const glm::vec3 worldPosition = transformPoint(model, particle.position);
            minValue = glm::min(minValue, worldPosition - glm::vec3(padding));
            maxValue = glm::max(maxValue, worldPosition + glm::vec3(padding));
        }

        runtime.hasBounds = true;
        runtime.boundsMin = minValue;
        runtime.boundsMax = maxValue;
        runtime.boundsCenter = (minValue + maxValue) * 0.5f;
        runtime.boundsRadius = glm::length((maxValue - minValue) * 0.5f);
    }

    static float maxParticleSize(const ParticleEmitterComponent& emitter)
    {
        float value = 0.001f;
        for (const ParticleFloatKey& key : emitter.sizeOverLifetime)
            value = std::max(value, key.value);
        const float aspect = std::max(emitter.aspectRatioMin, emitter.aspectRatioMax);
        value *= std::max(1.0f, aspect);
        if (emitter.primitive == ParticlePrimitive::Mesh)
        {
            const glm::vec3 meshScale = glm::max(emitter.meshScale, glm::vec3(0.001f));
            value *= std::max(meshScale.x, std::max(meshScale.y, meshScale.z));
        }
        return value;
    }

    static void applyVisibilityPolicy(const ParticleEmitterComponent& emitter,
                                      ParticleEmitterRuntimeComponent& runtime,
                                      const EmitterTransform& transform,
                                      const CameraInfo& camera)
    {
        const glm::vec3 center = runtime.hasBounds ? runtime.boundsCenter : transformOrigin(transform);
        runtime.distanceToCamera = camera.viewID == 0 ? 0.0f : glm::length(center - camera.position);

        runtime.lodSpawnScale = distanceLodScale(runtime.distanceToCamera,
                                                 emitter.runtime.lodNearDistance,
                                                 emitter.runtime.lodFarDistance,
                                                 emitter.runtime.lodMinSpawnScale);
        runtime.lodRenderScale = distanceLodScale(runtime.distanceToCamera,
                                                  emitter.runtime.lodNearDistance,
                                                  emitter.runtime.lodFarDistance,
                                                  emitter.runtime.lodMinRenderScale);

        const float distanceWeight =
            emitter.runtime.maxDrawDistance > 0.0f
                ? glm::clamp(1.0f - runtime.distanceToCamera / emitter.runtime.maxDrawDistance, 0.05f, 1.0f)
                : 1.0f;
        runtime.importanceScore = std::max(0.0f, emitter.runtime.importance) * distanceWeight;

        runtime.visible = true;
        runtime.cullReason = ParticleCullReason::None;
        if (camera.viewID == 0 || !runtime.hasBounds)
            return;

        if (emitter.runtime.distanceCulling &&
            emitter.runtime.maxDrawDistance > 0.0f &&
            runtime.distanceToCamera - runtime.boundsRadius > emitter.runtime.maxDrawDistance)
        {
            runtime.visible = false;
            runtime.cullReason = ParticleCullReason::Distance;
            return;
        }

        if (emitter.runtime.frustumCulling &&
            !FrustumCuller::isVisible(camera.frustum, runtime.boundsMin, runtime.boundsMax))
        {
            runtime.visible = false;
            runtime.cullReason = ParticleCullReason::Frustum;
        }
    }

    static void updateEmitter(const EcsControllerContext& ctx,
                              const ParticleEmitterComponent& emitter,
                              ParticleEmitterRuntimeComponent& runtime,
                              const EmitterTransform& transform,
                              float dt,
                              const EmitterFramePolicy& policy)
    {
        bindResourcesIfNeeded(ctx, emitter, runtime);
        if (!policy.simulate)
            return;
        if (dt <= 0.0f)
            return;

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

        const uint32_t maxParticles = std::max(1u, policy.maxParticles);
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
                const glm::vec3 eventPosition = particle.position;
                runtime.diedThisFrame += 1u;
                removeParticleAt(runtime, i);
                spawnEventParticles(emitter,
                                    runtime,
                                    transform,
                                    eventPosition,
                                    emitter.collision.spawnOnDeathCount,
                                    maxParticles);
                continue;
            }

            applyForces(emitter, transform, particle, dt);
            const float dragFactor = std::max(0.0f, 1.0f - emitter.drag * dt);
            particle.velocity *= dragFactor;
            particle.position += particle.velocity * dt;
            particle.rotation += particle.spin * dt;
            particle.meshRotation += particle.meshSpin * dt;
            glm::vec3 collisionEventPosition = particle.position;
            bool killedByCollision = false;
            const bool collided = applyCollision(emitter, runtime, particle, collisionEventPosition, killedByCollision);
            if (killedByCollision)
            {
                removeParticleAt(runtime, i);
                spawnEventParticles(emitter,
                                    runtime,
                                    transform,
                                    collisionEventPosition,
                                    emitter.collision.spawnOnCollisionCount,
                                    maxParticles);
                continue;
            }
            if (collided && emitter.collision.spawnOnCollisionCount > 0u)
            {
                spawnEventParticles(emitter,
                                    runtime,
                                    transform,
                                    collisionEventPosition,
                                    emitter.collision.spawnOnCollisionCount,
                                    maxParticles);
            }
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
            runtime.spawnAccumulator += emitter.emissionRate * emitter.intensity * policy.spawnScale * dt;
            uint32_t spawnCount = static_cast<uint32_t>(runtime.spawnAccumulator);
            runtime.spawnAccumulator -= static_cast<float>(spawnCount);
            if (policy.maxSpawnPerFrame != std::numeric_limits<uint32_t>::max() &&
                spawnCount > policy.maxSpawnPerFrame)
            {
                runtime.budgetSkippedSpawnsThisFrame += spawnCount - policy.maxSpawnPerFrame;
                spawnCount = policy.maxSpawnPerFrame;
                runtime.spawnAccumulator = 0.0f;
            }
            spawnParticles(emitter, runtime, transform, spawnCount, maxParticles);
        }

        spawnBursts(emitter,
                    runtime,
                    transform,
                    previousLocalAge,
                    localAge,
                    loopWrapped,
                    maxParticles,
                    policy.maxSpawnPerFrame);
    }

    static void spawnParticles(const ParticleEmitterComponent& emitter,
                               ParticleEmitterRuntimeComponent& runtime,
                               const EmitterTransform& transform,
                               uint32_t count,
                               uint32_t maxParticles)
    {
        const uint32_t available = maxParticles - static_cast<uint32_t>(runtime.particles.size());
        if (count > available)
            runtime.budgetSkippedSpawnsThisFrame += count - available;
        count = std::min(count, available);

        for (uint32_t i = 0; i < count; ++i)
        {
            if (spawnParticle(emitter, runtime, transform, nullptr))
                runtime.spawnedThisFrame += 1u;
        }
    }

    static void spawnBursts(const ParticleEmitterComponent& emitter,
                            ParticleEmitterRuntimeComponent& runtime,
                            const EmitterTransform& transform,
                            float previousLocalAge,
                            float localAge,
                            bool loopWrapped,
                            uint32_t maxParticles,
                            uint32_t maxSpawnPerFrame)
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
                uint32_t burstCount = count;
                if (maxSpawnPerFrame != std::numeric_limits<uint32_t>::max())
                {
                    const uint32_t remainingFrameSpawns =
                        runtime.spawnedThisFrame >= maxSpawnPerFrame ? 0u : maxSpawnPerFrame - runtime.spawnedThisFrame;
                    if (burstCount > remainingFrameSpawns)
                    {
                        runtime.budgetSkippedSpawnsThisFrame += burstCount - remainingFrameSpawns;
                        burstCount = remainingFrameSpawns;
                    }
                }
                spawnParticles(emitter, runtime, transform, burstCount, maxParticles);
                repeatIndex += 1;

                if (burst.repeatInterval <= 0.0f)
                    break;
            }
        }
    }

    static void applyForces(const ParticleEmitterComponent& emitter,
                            const EmitterTransform& transform,
                            ParticleState& particle,
                            float dt)
    {
        glm::vec3 acceleration = emitter.forces.acceleration + emitter.forces.wind;
        const glm::vec3 localPosition = emitter.localSpace
            ? particle.position
            : glm::vec3(transform.inverseWorldMatrix * glm::vec4(particle.position, 1.0f));
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

    static void removeParticleAt(ParticleEmitterRuntimeComponent& runtime, size_t index)
    {
        if (index + 1u < runtime.particles.size())
            runtime.particles[index] = runtime.particles.back();
        runtime.particles.pop_back();
    }

    static bool applyCollision(const ParticleEmitterComponent& emitter,
                               ParticleEmitterRuntimeComponent& runtime,
                               ParticleState& particle,
                               glm::vec3& eventPosition,
                               bool& killed)
    {
        if (emitter.collision.mode != ParticleCollisionMode::GroundPlane)
            return false;

        const float groundY = emitter.collision.groundY;
        float particleY = particle.position.y;
        if (!emitter.localSpace)
            particleY = particle.position.y;

        if (particleY >= groundY)
            return false;

        eventPosition = particle.position;
        runtime.collisionEventsThisFrame += 1u;

        if (emitter.collision.killOnCollision)
        {
            runtime.diedThisFrame += 1u;
            killed = true;
            return true;
        }

        particle.position.y = groundY;
        particle.velocity.y = std::abs(particle.velocity.y) * std::max(0.0f, emitter.collision.bounce);
        particle.velocity.x *= glm::clamp(emitter.collision.damping, 0.0f, 1.0f);
        particle.velocity.z *= glm::clamp(emitter.collision.damping, 0.0f, 1.0f);
        killed = false;
        return true;
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

    static void spawnEventParticles(const ParticleEmitterComponent& emitter,
                                    ParticleEmitterRuntimeComponent& runtime,
                                    const EmitterTransform& transform,
                                    const glm::vec3& position,
                                    uint32_t count,
                                    uint32_t maxParticles)
    {
        if (count == 0u || emitter.collision.maxEventSpawnsPerFrame == 0u)
            return;

        const uint32_t remainingEventBudget =
            runtime.eventSpawnsThisFrame >= emitter.collision.maxEventSpawnsPerFrame
                ? 0u
                : emitter.collision.maxEventSpawnsPerFrame - runtime.eventSpawnsThisFrame;
        count = std::min(count, remainingEventBudget);

        const uint32_t available = maxParticles > runtime.particles.size()
            ? maxParticles - static_cast<uint32_t>(runtime.particles.size())
            : 0u;
        if (count > available)
            runtime.budgetSkippedSpawnsThisFrame += count - available;
        count = std::min(count, available);

        for (uint32_t i = 0; i < count; ++i)
        {
            if (spawnParticle(emitter, runtime, transform, &position))
            {
                runtime.spawnedThisFrame += 1u;
                runtime.eventSpawnsThisFrame += 1u;
            }
        }
    }

    static bool spawnParticle(const ParticleEmitterComponent& emitter,
                              ParticleEmitterRuntimeComponent& runtime,
                              const EmitterTransform& transform,
                              const glm::vec3* positionOverride)
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
            particle.position = positionOverride == nullptr ? localPosition : *positionOverride;
            particle.velocity = velocity;
        }
        else
        {
            particle.position = positionOverride == nullptr
                ? glm::vec3(transform.worldMatrix * glm::vec4(localPosition, 1.0f))
                : *positionOverride;
            particle.velocity = transform.worldLinear * velocity;
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
        return true;
    }

    void extractEmitter(const ParticleEmitterComponent& emitter,
                        const ParticleEmitterRuntimeComponent& runtime,
                        const EmitterTransform& transform,
                        const CameraInfo& camera)
    {
        const glm::mat4 model = emitter.localSpace ? transform.worldMatrix : glm::mat4(1.0f);

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
                * particle.sizeScale
                * std::max(0.0f, emitter.runtime.effectScale)
                * std::max(0.0f, runtime.lodRenderScale);

            if (emitter.primitive == ParticlePrimitive::Mesh)
            {
                if (runtime.meshID == 0)
                    continue;

                glm::mat4 particleModel = emitter.localSpace ? transform.worldMatrix : glm::mat4(1.0f);
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
                instance.importance = runtime.importanceScore;

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
            instance.importance = runtime.importanceScore;
            instance.spriteShape = emitter.spriteShape;
            applyVelocityStretch(emitter, particle, transform, camera, instance);

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

    static void applyVelocityStretch(const ParticleEmitterComponent& emitter,
                                     const ParticleState& particle,
                                     const EmitterTransform& transform,
                                     const CameraInfo& camera,
                                     ParticleInstance& instance)
    {
        if (emitter.runtime.velocityStretch <= 0.0f || camera.viewID == 0)
            return;

        const glm::mat3 modelRotation = emitter.localSpace ? transform.worldLinear : glm::mat3(1.0f);
        const glm::vec3 worldVelocity = modelRotation * particle.velocity;
        const glm::vec3 viewVelocity = glm::mat3(camera.viewMatrix) * worldVelocity;
        const glm::vec2 viewVelocity2D = {viewVelocity.x, viewVelocity.y};
        const float speed = glm::length(viewVelocity2D);
        if (speed <= 0.001f)
            return;

        const float stretch = std::min(std::max(0.0f, emitter.runtime.velocityStretchMax),
                                       speed * emitter.runtime.velocityStretch);
        instance.size.x *= 1.0f + stretch;
        instance.rotation = std::atan2(viewVelocity2D.y, viewVelocity2D.x);
    }

    void buildFrameData(ParticleFrameData& frameData, ParticleBudgetState& budget)
    {
        applyRenderBudget(frameData.renderBudget, frameData, budget);

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
        frameData.renderedParticleCount =
            static_cast<uint32_t>(frameData.instances.size() + frameData.meshInstances.size());
        budget.renderedParticles = frameData.renderedParticleCount;
    }

    void applyRenderBudget(uint32_t renderBudget, ParticleFrameData& frameData, ParticleBudgetState& budget)
    {
        const uint32_t total = static_cast<uint32_t>(alphaParticles.size() +
                                                     additiveParticles.size() +
                                                     alphaMeshParticles.size() +
                                                     additiveMeshParticles.size());
        if (renderBudget == 0u || total <= renderBudget)
            return;

        sortByRenderPriority(alphaParticles);
        sortByRenderPriority(additiveParticles);
        sortByRenderPriority(alphaMeshParticles);
        sortByRenderPriority(additiveMeshParticles);

        uint32_t remaining = renderBudget;
        clipParticleVector(alphaParticles, remaining);
        clipParticleVector(additiveParticles, remaining);
        clipParticleVector(alphaMeshParticles, remaining);
        clipParticleVector(additiveMeshParticles, remaining);

        const uint32_t kept = static_cast<uint32_t>(alphaParticles.size() +
                                                    additiveParticles.size() +
                                                    alphaMeshParticles.size() +
                                                    additiveMeshParticles.size());
        frameData.budgetClippedParticleCount = total - kept;
        budget.budgetClippedEmitters += frameData.budgetClippedParticleCount > 0u ? 1u : 0u;
    }

    template <typename ParticleVector>
    static void sortByRenderPriority(ParticleVector& particles)
    {
        std::sort(particles.begin(),
                  particles.end(),
                  [](const auto& lhs, const auto& rhs)
                  {
                      if (lhs.instance.importance != rhs.instance.importance)
                          return lhs.instance.importance > rhs.instance.importance;
                      return lhs.instance.depth > rhs.instance.depth;
                  });
    }

    template <typename ParticleVector>
    static void clipParticleVector(ParticleVector& particles, uint32_t& remaining)
    {
        if (remaining == 0u)
        {
            particles.clear();
            return;
        }
        if (particles.size() <= remaining)
        {
            remaining -= static_cast<uint32_t>(particles.size());
            return;
        }

        particles.resize(remaining);
        remaining = 0u;
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
