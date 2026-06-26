#pragma once

#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "PhysicsDebugRenderer.h"

namespace gts::debugdraw
{
    inline void installPhysicsDebugFeature(GtsScene& scene, const EcsControllerContext&)
    {
        if (!scene.markSceneFeatureInstalled("physics_debug"))
            return;

        scene.getWorld().addControllerSystem<PhysicsDebugRenderer>(EcsSystemGroup::Tools);
    }
}
