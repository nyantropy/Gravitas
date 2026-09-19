#pragma once

#include "BuiltinExecutionGroups.h"
#include "SceneExecutionPolicy.h"

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "TransformAnimationSystem.hpp"

namespace gts::animation
{
    inline void installAnimationFeature(ECSWorld& world)
    {
        gts::execution::ensureExecutionPolicy(world);
        world.addSimulationSystem<TransformAnimationSystem>(gts::execution::groups::Animation);
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
