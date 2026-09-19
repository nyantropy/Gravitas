#pragma once

#include "RendererExecutionInputs.h"

#include "ECSWorld.hpp"
#include "ParticleEffectHotReloadSystem.hpp"
#include "ParticleEmitterSystem.hpp"

namespace gts::rendering
{
    inline void installRendererParticleSceneFeature(ECSWorld& world, const RendererExecutionInputs& execution)
    {
        if (!world.hasConfiguredDefaultExecutionSelection())
            world.configureDefaultExecutionSelection(execution.defaultSelection);
        world.addControllerSystem<ParticleEffectHotReloadSystem>(execution.particles);
        world.addControllerSystem<ParticleEmitterSystem>(execution.particles);
    }
}
