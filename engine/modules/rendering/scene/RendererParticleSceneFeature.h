#pragma once

#include "ECSWorld.hpp"
#include "ParticleEffectHotReloadSystem.hpp"
#include "ParticleEmitterSystem.hpp"

namespace gts::rendering
{
    inline void installRendererParticleSceneFeature(ECSWorld& world)
    {
        world.addControllerSystem<ParticleEffectHotReloadSystem>(EcsSystemGroup::Particles);
        world.addControllerSystem<ParticleEmitterSystem>(EcsSystemGroup::Particles);
    }
}
