#pragma once

#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "PhysicsSystem.h"
#include "PhysicsWorld.h"
#include "TransformSceneFeature.h"

namespace gts::physics
{
    inline void installPhysicsFeature(GtsScene& scene, EcsControllerContext& ctx)
    {
        if (!scene.markSceneFeatureInstalled("physics"))
            return;

        gts::transform::installTransformRuntime(scene);

        PhysicsWorld& physicsWorld = scene.createSceneResource<PhysicsWorld>(&scene.getWorld());
        scene.setPhysicsModule(&physicsWorld);
        ctx.physics = &physicsWorld;

        scene.getWorld().addSimulationSystem<PhysicsSystem>(EcsSystemGroup::Physics, &physicsWorld);
    }
}
