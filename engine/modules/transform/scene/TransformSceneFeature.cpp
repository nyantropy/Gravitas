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

    void installTransformRuntime(ECSWorld& world, const EcsExecutionSelection& defaultSelection)
    {
        if (!world.hasConfiguredDefaultExecutionSelection())
            world.configureDefaultExecutionSelection(defaultSelection);
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

    void installTransformResolver(ECSWorld&                    world,
                                  const EcsExecutionSelection& defaultSelection,
                                  EcsSystemGroup               resolverGroup)
    {
        if (!world.hasConfiguredDefaultExecutionSelection())
            world.configureDefaultExecutionSelection(defaultSelection);
        installTransformWorldState(world);
        world.addControllerSystem<TransformSystem>(resolverGroup);
    }

    void installTransformFeature(ECSWorld&                    world,
                                 const EcsExecutionSelection& defaultSelection,
                                 EcsSystemGroup               resolverGroup)
    {
        installTransformRuntime(world, defaultSelection);
        installTransformResolver(world, defaultSelection, resolverGroup);
    }

    void installTransformRuntime(GtsScene& scene, const EcsExecutionSelection& defaultSelection)
    {
        if (!scene.markSceneFeatureInstalled("transform-runtime"))
            return;

        installTransformRuntime(scene.getWorld(), defaultSelection);
    }

    void installTransformResolver(GtsScene&                    scene,
                                  const EcsExecutionSelection& defaultSelection,
                                  EcsSystemGroup               resolverGroup)
    {
        if (!scene.markSceneFeatureInstalled("transform-resolver"))
            return;

        installTransformResolver(scene.getWorld(), defaultSelection, resolverGroup);
    }

    void installTransformFeature(GtsScene&                    scene,
                                 const EcsExecutionSelection& defaultSelection,
                                 EcsSystemGroup               resolverGroup)
    {
        installTransformRuntime(scene, defaultSelection);
        installTransformResolver(scene, defaultSelection, resolverGroup);
    }
} // namespace gts::transform
