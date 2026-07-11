#pragma once

#include "DynamicMeshBindingSystem.hpp"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MaterialBindingSystem.hpp"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "QuadMeshBindingSystem.hpp"
#include "QuadMeshComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "RenderGpuComponent.h"
#include "RenderGpuSystem.hpp"
#include "RenderObjectLifecycleSystem.hpp"
#include "RenderableCleanupSystem.hpp"
#include "StaticMeshBindingSystem.hpp"
#include "StaticMeshComponent.h"
#include "TextureAnimationComponent.h"
#include "TextureAnimationRuntimeComponent.h"
#include "TextureAnimationSystem.hpp"
#include "WorldTextBindingSystem.hpp"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"
#include "WorldTransformComponent.h"

namespace gts::rendering
{
    inline void resetRendererGeometrySceneFeature(ECSWorld& world)
    {
        resetGeometryBindingLifecycleState(world);
    }

    inline void installRendererGeometrySceneFeature(ECSWorld& world, IResourceProvider* resources)
    {
        world.registerRemoveCallback<RenderGpuComponent>(
            [resources](ECSWorld& world, Entity entity, RenderGpuComponent& renderGpu)
            {
                RenderExtractionSnapshotBuilder::notifyRenderableRemoved(world, entity, renderGpu);

                if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED || resources == nullptr)
                    return;

                resources->releaseObjectSlot(renderGpu.objectSSBOSlot);
                renderGpu.objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
            });
        world.registerRemoveCallback<MeshGpuComponent>(
            [resources](ECSWorld& world, Entity entity, MeshGpuComponent& meshGpu)
            {
                queueRenderObjectRefresh(world, entity);

                if (!meshGpu.ownsProceduralMeshResource || meshGpu.meshID == 0 || resources == nullptr)
                    return;

                resources->releaseProceduralMesh(meshGpu.meshID);
                meshGpu.meshID                     = 0;
                meshGpu.ownsProceduralMeshResource = false;
            });
        world.registerAddCallback<StaticMeshComponent>(
            [](ECSWorld& world, Entity entity, StaticMeshComponent&)
            {
                queueStaticMeshRefresh(world, entity);
            });
        world.registerRemoveCallback<StaticMeshComponent>(
            [](ECSWorld& world, Entity entity, StaticMeshComponent&)
            {
                queueMaterialRefresh(world, entity);
                queueRenderableCleanup(world, entity);
            });
        world.registerAddCallback<QuadMeshComponent>(
            [](ECSWorld& world, Entity entity, QuadMeshComponent&)
            {
                queueQuadMeshRefresh(world, entity);
            });
        world.registerRemoveCallback<QuadMeshComponent>(
            [](ECSWorld& world, Entity entity, QuadMeshComponent&)
            {
                queueMaterialRefresh(world, entity);
                queueRenderableCleanup(world, entity);
            });
        world.registerAddCallback<DynamicMeshComponent>(
            [](ECSWorld& world, Entity entity, DynamicMeshComponent&)
            {
                queueDynamicMeshRefresh(world, entity);
            });
        world.registerRemoveCallback<DynamicMeshComponent>(
            [](ECSWorld& world, Entity entity, DynamicMeshComponent&)
            {
                queueMaterialRefresh(world, entity);
                queueRenderableCleanup(world, entity);
            });
        world.registerAddCallback<MaterialComponent>(
            [](ECSWorld& world, Entity entity, MaterialComponent&)
            {
                queueMaterialRefresh(world, entity);
                queueRenderObjectRefresh(world, entity);
            });
        world.registerRemoveCallback<MaterialComponent>(
            [](ECSWorld& world, Entity entity, MaterialComponent&)
            {
                queueRenderableCleanup(world, entity);
            });
        world.registerAddCallback<TextureAnimationComponent>(
            [](ECSWorld& world, Entity entity, TextureAnimationComponent&)
            {
                if (!world.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    world.commands().addComponent<TextureAnimationRuntimeComponent>(
                        entity, TextureAnimationRuntimeComponent{});

                if (!world.hasComponent<RenderGpuComponent>(entity) ||
                    !world.hasComponent<RenderDirtyComponent>(entity))
                {
                    return;
                }

                auto& renderGpu       = world.getComponent<RenderGpuComponent>(entity);
                renderGpu.uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
                markObjectDataDirty(world, entity);
            });
        world.registerRemoveCallback<TextureAnimationComponent>(
            [](ECSWorld& world, Entity entity, TextureAnimationComponent&)
            {
                if (world.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    world.commands().removeComponent<TextureAnimationRuntimeComponent>(entity);

                if (!world.hasComponent<RenderGpuComponent>(entity) ||
                    !world.hasComponent<RenderDirtyComponent>(entity))
                {
                    return;
                }

                auto& renderGpu       = world.getComponent<RenderGpuComponent>(entity);
                renderGpu.uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
                markObjectDataDirty(world, entity);
            });
        world.registerAddCallback<WorldTextComponent>(
            [](ECSWorld& world, Entity entity, WorldTextComponent&)
            {
                if (!world.hasComponent<WorldTextRuntimeComponent>(entity))
                    world.commands().addComponent<WorldTextRuntimeComponent>(entity, WorldTextRuntimeComponent{});
                queueMaterialRefresh(world, entity);
                queueRenderObjectRefresh(world, entity);
            });
        world.registerRemoveCallback<WorldTextComponent>(
            [](ECSWorld& world, Entity entity, WorldTextComponent&)
            {
                if (world.hasComponent<WorldTextRuntimeComponent>(entity))
                    world.commands().removeComponent<WorldTextRuntimeComponent>(entity);

                queueRenderableCleanup(world, entity);
            });
        world.registerAddCallback<MeshGpuComponent>(
            [](ECSWorld& world, Entity entity, MeshGpuComponent&)
            {
                queueRenderObjectRefresh(world, entity);
            });
        world.registerAddCallback<MaterialGpuComponent>(
            [](ECSWorld& world, Entity entity, MaterialGpuComponent&)
            {
                queueRenderObjectRefresh(world, entity);
            });
        world.registerRemoveCallback<MaterialGpuComponent>(
            [](ECSWorld& world, Entity entity, MaterialGpuComponent&)
            {
                queueRenderObjectRefresh(world, entity);
            });
        world.registerAddCallback<WorldTransformComponent>(
            [](ECSWorld& world, Entity entity, WorldTransformComponent&)
            {
                queueRenderObjectRefresh(world, entity);
            });

        world.addControllerSystem<StaticMeshBindingSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<QuadMeshBindingSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<DynamicMeshBindingSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<WorldTextBindingSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<MaterialBindingSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<RenderObjectLifecycleSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<RenderableCleanupSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<RenderGpuSystem>(EcsSystemGroup::RenderPrep);
        world.addControllerSystem<TextureAnimationSystem>(EcsSystemGroup::Animation);

        world.forEachSnapshot<StaticMeshComponent, MaterialComponent>(
            [&world](Entity entity, StaticMeshComponent&, MaterialComponent&)
            {
                queueStaticMeshRefresh(world, entity);
                queueMaterialRefresh(world, entity);
            });
        world.forEachSnapshot<QuadMeshComponent, MaterialComponent>(
            [&world](Entity entity, QuadMeshComponent&, MaterialComponent&)
            {
                queueQuadMeshRefresh(world, entity);
                queueMaterialRefresh(world, entity);
            });
        world.forEachSnapshot<DynamicMeshComponent, MaterialComponent>(
            [&world](Entity entity, DynamicMeshComponent&, MaterialComponent&)
            {
                queueDynamicMeshRefresh(world, entity);
                queueMaterialRefresh(world, entity);
            });
        world.forEachSnapshot<MaterialComponent>(
            [&world](Entity entity, MaterialComponent&)
            {
                queueMaterialRefresh(world, entity);
            });
        world.forEachSnapshot<TextureAnimationComponent>(
            [&world](Entity entity, TextureAnimationComponent&)
            {
                if (!world.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    world.commands().addComponent<TextureAnimationRuntimeComponent>(
                        entity, TextureAnimationRuntimeComponent{});
            });
        world.forEachSnapshot<WorldTextComponent>(
            [&world](Entity entity, WorldTextComponent&)
            {
                if (!world.hasComponent<WorldTextRuntimeComponent>(entity))
                    world.commands().addComponent<WorldTextRuntimeComponent>(entity, WorldTextRuntimeComponent{});
                queueMaterialRefresh(world, entity);
            });
    }
}
