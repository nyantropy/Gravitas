#pragma once

#include "BuiltinExecutionGroups.h"
#include "SceneExecutionPolicy.h"

#include "ECSWorld.hpp"
#include "ParticleEffectHotReloadSystem.hpp"
#include "ParticleEmitterSystem.hpp"

namespace gts::rendering
{
    inline void installRendererParticleSceneFeature(ECSWorld& world)
    {
        gts::execution::ensureExecutionPolicy(world);
        world.addControllerSystem<ParticleEffectHotReloadSystem>(gts::execution::groups::Particles);
        world.addControllerSystem<ParticleEmitterSystem>(gts::execution::groups::Particles);
    }
}
