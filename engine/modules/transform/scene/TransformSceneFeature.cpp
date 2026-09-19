#include "SceneExecutionPolicy.h"
#include "BuiltinExecutionGroups.h"
#include "TransformSceneFeature.h"

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "HierarchyComponent.h"
#include "TransformHierarchyHelpers.h"
#include "TransformDirtyHelpers.h"
#include "TransformInvalidationLifecycle.h"
#include "TransformSystem.hpp"
#include "../runtime/detail/TransformInvalidationState.h"

namespace gts::transform
{
    void resetTransformSceneFeature(ECSWorld& world)
    {
        releaseTransformWorldState(world);
    }

    void installTransformRuntime(ECSWorld& world)
    {
        gts::execution::ensureExecutionPolicy(world);
        installTransformWorldState(world);
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

        world.forEachSnapshot<TransformComponent>(
            [&world](Entity entity, TransformComponent&)
            {
                // Existing transforms can be present when low-level worlds install
                // the feature after creating entities.
                queueTransformDirty(world, entity);
            });
    }

    void installTransformResolver(ECSWorld& world)
    {
        gts::execution::ensureExecutionPolicy(world);
        installTransformWorldState(world);
        world.addControllerSystem<TransformSystem>(gts::execution::groups::RenderPrep);
    }

    void installTransformFeature(ECSWorld& world)
    {
        installTransformRuntime(world);
        installTransformResolver(world);
    }

    void installTransformRuntime(GtsScene& scene)
    {
        if (!scene.markSceneFeatureInstalled("transform-runtime"))
            return;

        installTransformRuntime(scene.getWorld());
    }

    void installTransformResolver(GtsScene& scene)
    {
        if (!scene.markSceneFeatureInstalled("transform-resolver"))
            return;

        installTransformResolver(scene.getWorld());
    }

    void installTransformFeature(GtsScene& scene)
    {
        installTransformRuntime(scene);
        installTransformResolver(scene);
    }
} // namespace gts::transform
