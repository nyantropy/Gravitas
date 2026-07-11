#pragma once

#include "DebugDrawSceneFeature.h"
#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "PhysicsDebugRenderer.h"

namespace gts::debugdraw
{
    inline void installPhysicsDebugFeature(GtsScene& scene, const EcsControllerContext& ctx)
    {
        if (!scene.markSceneFeatureInstalled("physics_debug"))
            return;

        installDebugDrawFeature(scene, ctx);
        scene.getWorld().addControllerSystem<PhysicsDebugRenderer>(EcsSystemGroup::Tools);
    }
}
