#pragma once

#include <filesystem>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"

class ParticleEffectHotReloadSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        ParticleEffectRegistryComponent& registry = ensureRegistry(ctx.world);

        ctx.world.forEach<ParticleEmitterComponent>(
            [&](Entity entity, ParticleEmitterComponent& emitter)
            {
                if (emitter.effectPath.empty() || !emitter.reloadFromEffect)
                    return;

                ParticleEffectAsset* asset = loadOrReload(registry, emitter.effectPath);
                if (asset == nullptr || !asset->loaded)
                    return;

                if (ctx.world.hasComponent<ParticleEmitterRuntimeComponent>(entity))
                {
                    ParticleEmitterRuntimeComponent& runtime =
                        ctx.world.getComponent<ParticleEmitterRuntimeComponent>(entity);
                    if (runtime.appliedEffectVersion == asset->version)
                        return;

                    applyEffect(emitter, *asset);
                    resetRuntime(runtime);
                    runtime.appliedEffectVersion = asset->version;
                    return;
                }

                applyEffect(emitter, *asset);
            });
    }

private:
    static ParticleEffectRegistryComponent& ensureRegistry(ECSWorld& world)
    {
        if (!world.hasAny<ParticleEffectRegistryComponent>())
            return world.createSingleton<ParticleEffectRegistryComponent>();
        return world.getSingleton<ParticleEffectRegistryComponent>();
    }

    static ParticleEffectAsset* loadOrReload(ParticleEffectRegistryComponent& registry,
                                             const std::string& path)
    {
        if (!std::filesystem::exists(path))
            return nullptr;

        const auto lastWrite = std::filesystem::last_write_time(path);
        ParticleEffectAsset& asset = registry.effects[path];
        if (asset.loaded && asset.lastWriteTime == lastWrite)
            return &asset;

        ParticleEmitterComponent loaded;
        if (!gts::particles::loadParticleEffect(path, loaded))
            return nullptr;

        loaded.effectPath = path;
        loaded.reloadFromEffect = true;
        asset.emitter = loaded;
        asset.lastWriteTime = lastWrite;
        asset.loaded = true;
        asset.version += 1;
        return &asset;
    }

    static void applyEffect(ParticleEmitterComponent& target, const ParticleEffectAsset& asset)
    {
        const std::string effectPath = target.effectPath;
        const uint32_t randomSeed = target.randomSeed;

        target = asset.emitter;
        target.effectPath = effectPath;
        target.randomSeed = randomSeed;
        target.reloadFromEffect = true;
    }

    static void resetRuntime(ParticleEmitterRuntimeComponent& runtime)
    {
        runtime.particles.clear();
        runtime.spawnAccumulator = 0.0f;
        runtime.emitterAge = 0.0f;
        runtime.burstRepeatCounts.clear();
        runtime.textureID = 0;
        runtime.boundTexturePath.clear();
    }
};
