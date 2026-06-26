#pragma once

#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "PhysicsDebugRenderer.h"
#include "PhysicsSystem.h"
#include "PhysicsWorld.h"

namespace gts::physics
{
    inline void installPhysicsFeature(GtsScene& scene, EcsControllerContext& ctx, bool enableDebugRenderer = false)
    {
        if (!scene.markSceneFeatureInstalled("physics"))
            return;

        PhysicsWorld& physicsWorld = scene.createSceneResource<PhysicsWorld>(&scene.getWorld());
        scene.setPhysicsModule(&physicsWorld);
        ctx.physics = &physicsWorld;

        scene.getWorld().addSimulationSystem<PhysicsSystem>(EcsSystemGroup::Physics, &physicsWorld);
        if (enableDebugRenderer)
            scene.getWorld().addControllerSystem<PhysicsDebugRenderer>(EcsSystemGroup::Tools);
    }
}
