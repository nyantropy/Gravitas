#pragma once

#include <filesystem>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ParticleProgramCompiler.h"

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

                ParticleEffectRegistryEntry* entry = loadOrReload(registry, emitter.effectPath);
                if (entry == nullptr || !entry->loaded)
                    return;

                if (ctx.world.hasComponent<ParticleEmitterRuntimeComponent>(entity))
                {
                    ParticleEmitterRuntimeComponent& runtime =
                        ctx.world.getComponent<ParticleEmitterRuntimeComponent>(entity);
                    if (runtime.appliedEffectVersion == entry->version)
                        return;

                    applyEffect(emitter, entry->asset);
                    resetRuntime(runtime);
                    runtime.appliedEffectVersion = entry->version;
                    return;
                }

                applyEffect(emitter, entry->asset);
            });
    }

private:
    static ParticleEffectRegistryComponent& ensureRegistry(ECSWorld& world)
    {
        if (!world.hasAny<ParticleEffectRegistryComponent>())
            return world.createSingleton<ParticleEffectRegistryComponent>();
        return world.getSingleton<ParticleEffectRegistryComponent>();
    }

    static ParticleEffectRegistryEntry* loadOrReload(ParticleEffectRegistryComponent& registry, const std::string& path)
    {
        if (!std::filesystem::exists(path))
            return nullptr;

        const auto                   lastWrite = std::filesystem::last_write_time(path);
        ParticleEffectRegistryEntry& entry     = registry.effects[path];
        if (entry.loaded && entry.lastWriteTime == lastWrite)
            return &entry;

        ParticleEffectAsset loaded;
        if (!gts::particles::loadParticleEffectAsset(path, loaded))
            return nullptr;

        entry.asset         = loaded;
        entry.lastWriteTime = lastWrite;
        entry.loaded        = true;
        entry.version += 1;
        return &entry;
    }

    static void applyEffect(ParticleEmitterComponent& target, const ParticleEffectAsset& asset)
    {
        const std::string effectPath      = target.effectPath;
        const std::string effectEmitterId = target.effectEmitterId;
        const uint32_t    randomSeed      = target.randomSeed;

        const ParticleEffectEmitter* selected = gts::particles::selectParticleEffectEmitter(asset, effectEmitterId);
        if (selected == nullptr)
            return;

        target                  = gts::particles::compiledParticleRuntimeDescriptor(*selected);
        target.effectPath       = effectPath;
        target.effectEmitterId  = effectEmitterId.empty() ? selected->stableId : effectEmitterId;
        target.randomSeed       = randomSeed;
        target.reloadFromEffect = true;
    }

    static void resetRuntime(ParticleEmitterRuntimeComponent& runtime)
    {
        runtime.particles.clear();
        runtime.spawnAccumulator = 0.0f;
        runtime.emitterAge       = 0.0f;
        runtime.burstRepeatCounts.clear();
        runtime.textureID = 0;
        runtime.meshID    = 0;
        runtime.boundTexturePath.clear();
        runtime.boundMeshPath.clear();
    }
};
