#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "CameraGpuComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "GraphicsConstants.h"
#include "ParticleEmitterComponent.h"
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

    static constexpr float Pi = 3.14159265358979323846f;

    std::vector<ExtractedParticle> alphaParticles;
    std::vector<ExtractedParticle> additiveParticles;

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

    static uint32_t nextRandom(uint32_t& state)
    {
        if (state == 0u)
            state = 1u;

        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    static float random01(uint32_t& state)
    {
        constexpr float invMax = 1.0f / static_cast<float>(std::numeric_limits<uint32_t>::max());
        return static_cast<float>(nextRandom(state)) * invMax;
    }

    static float randomRange(uint32_t& state, float minValue, float maxValue)
    {
        if (maxValue <= minValue)
            return minValue;
        return minValue + (maxValue - minValue) * random01(state);
    }

    static glm::vec3 randomUnitVector(uint32_t& state)
    {
        const float z = randomRange(state, -1.0f, 1.0f);
        const float a = randomRange(state, 0.0f, Pi * 2.0f);
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return {r * std::cos(a), z, r * std::sin(a)};
    }

    static glm::vec3 randomInDisc(uint32_t& state, float minRadius, float maxRadius)
    {
        const float radius = std::sqrt(random01(state))
            * std::max(0.0f, maxRadius - minRadius)
            + std::max(0.0f, minRadius);
        const float angle = randomRange(state, 0.0f, Pi * 2.0f);
        return {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
    }

    static glm::vec3 sampleShape(const ParticleEmitterComponent& emitter, uint32_t& rng)
    {
        switch (emitter.shape)
        {
            case ParticleEmitterShape::Sphere:
            {
                const float radius = std::cbrt(random01(rng)) * std::max(0.0f, emitter.sphereRadius);
                return randomUnitVector(rng) * radius;
            }
            case ParticleEmitterShape::Box:
                return {
                    randomRange(rng, -emitter.boxExtents.x, emitter.boxExtents.x),
                    randomRange(rng, -emitter.boxExtents.y, emitter.boxExtents.y),
                    randomRange(rng, -emitter.boxExtents.z, emitter.boxExtents.z)
                };
            case ParticleEmitterShape::Disc:
                return randomInDisc(rng, 0.0f, emitter.discRadius);
            case ParticleEmitterShape::Cylinder:
            {
                glm::vec3 p = randomInDisc(rng, 0.0f, emitter.cylinderRadius);
                p.y = randomRange(rng, -emitter.cylinderHeight * 0.5f, emitter.cylinderHeight * 0.5f);
                return p;
            }
            case ParticleEmitterShape::Ring:
                return randomInDisc(rng, emitter.ringInnerRadius, emitter.ringOuterRadius);
        }

        return {0.0f, 0.0f, 0.0f};
    }

    static glm::vec3 radialDirection(const glm::vec3& localPosition)
    {
        glm::vec3 flat = {localPosition.x, 0.0f, localPosition.z};
        if (glm::length(flat) > 0.0001f)
            return glm::normalize(flat);

        if (glm::length(localPosition) > 0.0001f)
            return glm::normalize(localPosition);

        return {0.0f, 1.0f, 0.0f};
    }

    static glm::vec3 tangentDirection(const glm::vec3& radial)
    {
        glm::vec3 tangent = {-radial.z, 0.0f, radial.x};
        if (glm::length(tangent) > 0.0001f)
            return glm::normalize(tangent);
        return {1.0f, 0.0f, 0.0f};
    }

    static glm::vec3 rgbToHsv(const glm::vec3& rgb)
    {
        const float cMax = std::max(rgb.r, std::max(rgb.g, rgb.b));
        const float cMin = std::min(rgb.r, std::min(rgb.g, rgb.b));
        const float delta = cMax - cMin;

        glm::vec3 hsv{0.0f, 0.0f, cMax};
        if (delta <= 0.00001f)
            return hsv;

        hsv.y = cMax <= 0.0f ? 0.0f : delta / cMax;
        if (cMax == rgb.r)
            hsv.x = std::fmod((rgb.g - rgb.b) / delta, 6.0f);
        else if (cMax == rgb.g)
            hsv.x = ((rgb.b - rgb.r) / delta) + 2.0f;
        else
            hsv.x = ((rgb.r - rgb.g) / delta) + 4.0f;

        hsv.x /= 6.0f;
        if (hsv.x < 0.0f)
            hsv.x += 1.0f;
        return hsv;
    }

    static glm::vec3 hsvToRgb(const glm::vec3& hsv)
    {
        const float h = hsv.x * 6.0f;
        const float c = hsv.z * hsv.y;
        const float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
        const float m = hsv.z - c;

        glm::vec3 rgb{0.0f, 0.0f, 0.0f};
        if (h < 1.0f)      rgb = {c, x, 0.0f};
        else if (h < 2.0f) rgb = {x, c, 0.0f};
        else if (h < 3.0f) rgb = {0.0f, c, x};
        else if (h < 4.0f) rgb = {0.0f, x, c};
        else if (h < 5.0f) rgb = {x, 0.0f, c};
        else               rgb = {c, 0.0f, x};
        return rgb + glm::vec3(m);
    }

    static glm::vec4 applyColorVariation(const glm::vec4& color,
                                         float hueVariation,
                                         float valueVariation,
                                         uint32_t& rng)
    {
        glm::vec3 hsv = rgbToHsv(glm::clamp(glm::vec3(color), glm::vec3(0.0f), glm::vec3(1.0f)));
        hsv.x += randomRange(rng, -hueVariation, hueVariation);
        hsv.x = hsv.x - std::floor(hsv.x);
        hsv.z = glm::clamp(hsv.z + randomRange(rng, -valueVariation, valueVariation), 0.0f, 1.0f);
        return {hsvToRgb(hsv), color.a};
    }

    static float evaluateFloatCurve(const ParticleFloatCurve& curve, float t, float fallback)
    {
        if (curve.empty())
            return fallback;

        if (t <= curve.front().t)
            return curve.front().value;

        for (size_t i = 1; i < curve.size(); ++i)
        {
            if (t > curve[i].t)
                continue;

            const ParticleFloatKey& a = curve[i - 1];
            const ParticleFloatKey& b = curve[i];
            const float span = std::max(0.0001f, b.t - a.t);
            const float localT = glm::clamp((t - a.t) / span, 0.0f, 1.0f);
            return glm::mix(a.value, b.value, localT);
        }

        return curve.back().value;
    }

    static glm::vec4 evaluateColorCurve(const ParticleColorCurve& curve, float t)
    {
        if (curve.empty())
            return {1.0f, 1.0f, 1.0f, 1.0f};

        if (t <= curve.front().t)
            return curve.front().color;

        for (size_t i = 1; i < curve.size(); ++i)
        {
            if (t > curve[i].t)
                continue;

            const ParticleColorKey& a = curve[i - 1];
            const ParticleColorKey& b = curve[i];
            const float span = std::max(0.0001f, b.t - a.t);
            const float localT = glm::clamp((t - a.t) / span, 0.0f, 1.0f);
            return glm::mix(a.color, b.color, localT);
        }

        return curve.back().color;
    }

    static void updateEmitter(const EcsControllerContext& ctx,
                              const ParticleEmitterComponent& emitter,
                              ParticleEmitterRuntimeComponent& runtime,
                              const TransformComponent& transform,
                              float dt)
    {
        bindTextureIfNeeded(ctx, emitter, runtime);
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
                        randomRange(runtime.rngState,
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
        glm::vec3 radial = radialDirection(localPosition);

        if (emitter.forces.radial != 0.0f)
            acceleration += radial * emitter.forces.radial;

        if (emitter.forces.vortex != 0.0f)
            acceleration += tangentDirection(radial) * emitter.forces.vortex;

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

    static void bindTextureIfNeeded(const EcsControllerContext& ctx,
                                    const ParticleEmitterComponent& emitter,
                                    ParticleEmitterRuntimeComponent& runtime)
    {
        if (ctx.resources == nullptr)
            return;

        const std::string texturePath = emitter.texturePath.empty()
            ? GraphicsConstants::ENGINE_RESOURCES + "/textures/engine_particle_fallback.png"
            : emitter.texturePath;

        if (runtime.textureID != 0 && runtime.boundTexturePath == texturePath)
            return;

        runtime.textureID = ctx.resources->requestTexture(texturePath);
        runtime.boundTexturePath = texturePath;
    }

    static void spawnParticle(const ParticleEmitterComponent& emitter,
                              ParticleEmitterRuntimeComponent& runtime,
                              const TransformComponent& transform)
    {
        ParticleState particle;
        const float minLifetime = std::max(0.01f, std::min(emitter.lifetimeMin, emitter.lifetimeMax));
        const float maxLifetime = std::max(minLifetime, std::max(emitter.lifetimeMin, emitter.lifetimeMax));
        particle.lifetime = randomRange(runtime.rngState, minLifetime, maxLifetime);

        const glm::vec3 localPosition = sampleShape(emitter, runtime.rngState);
        const glm::vec3 radial = radialDirection(localPosition);
        const glm::vec3 tangent = tangentDirection(radial);
        const float radialSpeed =
            randomRange(runtime.rngState, emitter.radialVelocityMin, emitter.radialVelocityMax);

        glm::vec3 velocity = emitter.initialVelocity;
        velocity += randomUnitVector(runtime.rngState) * emitter.velocitySpread;
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

        particle.tint = applyColorVariation(emitter.baseTint,
                                            emitter.hueVariation,
                                            emitter.valueVariation,
                                            runtime.rngState);
        particle.rotation = randomRange(runtime.rngState, 0.0f, Pi * 2.0f);
        particle.spin = randomRange(runtime.rngState, emitter.spinMin, emitter.spinMax);
        particle.sizeScale =
            randomRange(runtime.rngState, 1.0f - emitter.sizeRandomness, 1.0f + emitter.sizeRandomness);
        particle.frameOffset = emitter.flipbook.randomStart
            ? randomRange(runtime.rngState, 0.0f, static_cast<float>(std::max(1u, emitter.flipbook.frameCount)))
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
            glm::vec4 color = particle.tint * evaluateColorCurve(emitter.colorOverLifetime, normalizedAge);
            color.a *= evaluateFloatCurve(emitter.alphaOverLifetime, normalizedAge, 1.0f);
            color.a *= emitter.intensity;
            color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
            if (color.a <= 0.001f)
                continue;

            const glm::vec3 worldPosition = glm::vec3(model * glm::vec4(particle.position, 1.0f));
            const glm::vec4 viewPosition = camera.viewMatrix * glm::vec4(worldPosition, 1.0f);

            ParticleInstance instance;
            instance.worldPosition = worldPosition;
            instance.size =
                std::max(0.001f, evaluateFloatCurve(emitter.sizeOverLifetime, normalizedAge, 0.5f))
                * particle.sizeScale;
            instance.color = color;
            instance.uvRect = computeFlipbookUv(emitter, particle, normalizedAge);
            instance.rotation = particle.rotation;
            instance.depth = -viewPosition.z;
            instance.softness = std::max(0.0f, emitter.softness);

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

    static glm::vec4 computeFlipbookUv(const ParticleEmitterComponent& emitter,
                                       const ParticleState& particle,
                                       float normalizedAge)
    {
        const uint32_t columns = std::max(1u, emitter.flipbook.columns);
        const uint32_t rows = std::max(1u, emitter.flipbook.rows);
        const uint32_t maxFrames = columns * rows;
        const uint32_t frameCount = glm::clamp(emitter.flipbook.frameCount, 1u, maxFrames);
        uint32_t frame = 0;

        if (frameCount > 1)
        {
            if (emitter.flipbook.lifetimeDriven)
            {
                frame = static_cast<uint32_t>(
                    glm::clamp(normalizedAge, 0.0f, 0.9999f) * static_cast<float>(frameCount));
            }
            else
            {
                const float animated =
                    particle.age * std::max(0.0f, emitter.flipbook.frameRate) + particle.frameOffset;
                frame = static_cast<uint32_t>(animated) % frameCount;
            }
        }

        const uint32_t column = frame % columns;
        const uint32_t row = frame / columns;
        const float invColumns = 1.0f / static_cast<float>(columns);
        const float invRows = 1.0f / static_cast<float>(rows);
        const float u0 = static_cast<float>(column) * invColumns;
        const float v0 = static_cast<float>(row) * invRows;
        return {u0, v0, u0 + invColumns, v0 + invRows};
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

        frameData.instances.reserve(alphaParticles.size() + additiveParticles.size());
        frameData.drawCommands.reserve(alphaParticles.size() + additiveParticles.size());

        appendParticles(frameData, alphaParticles);
        appendParticles(frameData, additiveParticles);
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
};
