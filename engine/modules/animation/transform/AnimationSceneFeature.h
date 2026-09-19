#pragma once


#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "TransformAnimationSystem.hpp"

namespace gts::animation
{
    inline void installAnimationFeature(ECSWorld&                    world,
                                        const EcsExecutionSelection& defaultSelection,
                                        EcsSystemGroup               simulationGroup)
    {
        if (!world.hasConfiguredDefaultExecutionSelection())
            world.configureDefaultExecutionSelection(defaultSelection);
        world.addSimulationSystem<TransformAnimationSystem>(simulationGroup);
    }

    inline void installAnimationFeature(GtsScene&                    scene,
                                        const EcsExecutionSelection& defaultSelection,
                                        EcsSystemGroup               simulationGroup)
    {
        if (!scene.markSceneFeatureInstalled("animation"))
        {
            return;
        }

        installAnimationFeature(scene.getWorld(), defaultSelection, simulationGroup);
    }
} // namespace gts::animation
