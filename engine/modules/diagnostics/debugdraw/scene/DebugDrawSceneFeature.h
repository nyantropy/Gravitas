#pragma once

#include "BuiltinExecutionGroups.h"
#include "SceneExecutionPolicy.h"

#include "DebugDrawSystem.hpp"
#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"

namespace gts::debugdraw
{
    inline void installDebugDrawFeature(GtsScene& scene, const EcsControllerContext&)
    {
        if (!scene.markSceneFeatureInstalled("debugdraw"))
            return;

        gts::execution::ensureExecutionPolicy(scene.getWorld());
        scene.getWorld().addControllerSystem<DebugDrawSystem>(gts::execution::groups::Tools);
    }
}
