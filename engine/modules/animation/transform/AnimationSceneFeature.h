#pragma once

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "TransformAnimationSystem.hpp"

namespace gts::animation
{
    inline void installAnimationFeature(ECSWorld& world)
    {
        world.addSimulationSystem<TransformAnimationSystem>(EcsSystemGroup::Animation);
    }

    inline void installAnimationFeature(GtsScene& scene)
    {
        if (!scene.markSceneFeatureInstalled("animation"))
        {
            return;
        }

        installAnimationFeature(scene.getWorld());
    }
} // namespace gts::animation
