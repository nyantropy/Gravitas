#pragma once

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "HierarchyComponent.h"
#include "TransformHierarchyHelpers.h"
#include "TransformDirtyHelpers.h"
#include "TransformInvalidationLifecycle.h"
#include "TransformSystem.hpp"

namespace gts::transform
{
    inline void resetTransformSceneFeature(ECSWorld& world)
    {
        resetTransformInvalidationState(world);
    }

    inline void installTransformFeature(ECSWorld& world)
    {
        world.registerAddCallback<TransformComponent>(
            [](ECSWorld& world, Entity entity, TransformComponent&)
            {
                markDirty(world, entity);
            });
        world.registerAddCallback<HierarchyComponent>(
            [](ECSWorld& world, Entity entity, HierarchyComponent&)
            {
                queueTransformDirty(world, entity);
            });
        world.registerRemoveCallback<HierarchyComponent>(
            [](ECSWorld& world, Entity entity, HierarchyComponent& hierarchy)
            {
                detachHierarchyForRemoval(world, entity, hierarchy);
            });

        world.addControllerSystem<TransformSystem>(EcsSystemGroup::RenderPrep);
        world.forEachSnapshot<TransformComponent>(
            [&world](Entity entity, TransformComponent&)
            {
                // Existing transforms can be present when low-level worlds install
                // the feature after creating entities.
                queueTransformDirty(world, entity);
            });
    }

    inline void installTransformFeature(GtsScene& scene)
    {
        if (!scene.markSceneFeatureInstalled("transform"))
            return;

        scene.registerSceneResetHook(
            [](ECSWorld& world)
            {
                resetTransformSceneFeature(world);
            });

        installTransformFeature(scene.getWorld());
    }
}
