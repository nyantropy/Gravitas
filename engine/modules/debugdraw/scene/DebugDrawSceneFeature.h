#pragma once

#include "DebugDrawSystem.hpp"
#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"

namespace gts::debugdraw
{
    inline void installDebugDrawFeature(GtsScene& scene, const EcsControllerContext&)
    {
        if (!scene.markSceneFeatureInstalled("debugdraw"))
            return;

        scene.getWorld().addControllerSystem<DebugDrawSystem>(EcsSystemGroup::Tools);
    }
}
