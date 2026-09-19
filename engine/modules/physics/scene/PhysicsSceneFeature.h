#pragma once


#include "PhysicsControllerContext.h"
#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "PhysicsSystem.h"
#include "PhysicsWorld.h"
#include "ScenePhysics.h"
#include "TransformSceneFeature.h"

namespace gts::physics
{
    inline void installPhysicsFeature(GtsScene&                    scene,
                                      EcsControllerContext&        ctx,
                                      const EcsExecutionSelection& defaultSelection,
                                      EcsSystemGroup               simulationGroup)
    {
        if (!scene.markSceneFeatureInstalled("physics"))
            return;

        gts::transform::installTransformRuntime(scene, defaultSelection);

        PhysicsWorld& physicsWorld = scene.createSceneResource<PhysicsWorld>(&scene.getWorld());
        scene.createSceneResource<detail::ScenePhysicsBinding>(physicsWorld);
        gts::physics::controllerContext(ctx).physics = &physicsWorld;

        scene.getWorld().addSimulationSystem<PhysicsSystem>(simulationGroup, &physicsWorld);
    }
}
