#pragma once

#include <unordered_map>
#include <unordered_set>

#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "MaterialComponent.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "QuadMeshComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "RenderInvalidationLifecycle.h"
#include "StaticMeshComponent.h"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"

namespace gts::rendering
{
    struct GeometryBindingLifecycleState
    {
        std::unordered_set<entity_id_type> staticMeshRefreshEntities;
        std::unordered_set<entity_id_type> quadMeshRefreshEntities;
        std::unordered_set<entity_id_type> dynamicMeshRefreshEntities;
        std::unordered_set<entity_id_type> materialRefreshEntities;
        std::unordered_set<entity_id_type> renderObjectRefreshEntities;
        std::unordered_set<entity_id_type> cleanupEntities;
    };

    inline auto& geometryBindingLifecycleRegistry()
    {
        static std::unordered_map<ECSWorld*, GeometryBindingLifecycleState> registry;
        return registry;
    }

    inline GeometryBindingLifecycleState& geometryBindingLifecycleState(ECSWorld& world)
    {
        return geometryBindingLifecycleRegistry()[&world];
    }

    inline void resetGeometryBindingLifecycleState(ECSWorld& world)
    {
        geometryBindingLifecycleRegistry().erase(&world);
        resetRenderInvalidationState(world);
    }

    inline std::unordered_set<entity_id_type> takeStaticMeshRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.staticMeshRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeQuadMeshRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.quadMeshRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeDynamicMeshRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.dynamicMeshRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeMaterialRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.materialRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeRenderObjectRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.renderObjectRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeRenderableCleanupEntities(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.cleanupEntities);
        return pending;
    }

    inline void queueMaterialRefresh(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).materialRefreshEntities.insert(entity.id);
    }

    inline void queueRenderObjectRefresh(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).renderObjectRefreshEntities.insert(entity.id);
    }

    inline void queueStaticMeshRefresh(ECSWorld& world, Entity entity)
    {
        auto& state = geometryBindingLifecycleState(world);
        state.staticMeshRefreshEntities.insert(entity.id);
        state.materialRefreshEntities.insert(entity.id);
        state.renderObjectRefreshEntities.insert(entity.id);
    }

    inline void queueQuadMeshRefresh(ECSWorld& world, Entity entity)
    {
        auto& state = geometryBindingLifecycleState(world);
        state.quadMeshRefreshEntities.insert(entity.id);
        state.materialRefreshEntities.insert(entity.id);
        state.renderObjectRefreshEntities.insert(entity.id);
    }

    inline void queueDynamicMeshRefresh(ECSWorld& world, Entity entity)
    {
        auto& state = geometryBindingLifecycleState(world);
        state.dynamicMeshRefreshEntities.insert(entity.id);
        state.materialRefreshEntities.insert(entity.id);
        state.renderObjectRefreshEntities.insert(entity.id);
    }

    inline void queueRenderableCleanup(ECSWorld& world, Entity entity)
    {
        auto& state = geometryBindingLifecycleState(world);
        state.cleanupEntities.insert(entity.id);
        state.renderObjectRefreshEntities.insert(entity.id);
    }

    inline void markMeshRepresentationDirty(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<RenderDirtyComponent>(entity))
            world.getComponent<RenderDirtyComponent>(entity).meshDirty = true;
        queueRenderSnapshotDirty(world, entity);
    }

    inline void markMaterialRepresentationDirty(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<RenderDirtyComponent>(entity))
        {
            RenderDirtyComponent& dirty = world.getComponent<RenderDirtyComponent>(entity);
            dirty.materialDirty = true;
            dirty.objectDataDirty = true;
        }
        queueRenderSnapshotDirty(world, entity);
    }

    inline void markObjectDataDirty(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<RenderDirtyComponent>(entity))
            world.getComponent<RenderDirtyComponent>(entity).objectDataDirty = true;
        queueRenderSnapshotDirty(world, entity);
    }

    inline void markTransformRepresentationDirty(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<RenderDirtyComponent>(entity))
            world.getComponent<RenderDirtyComponent>(entity).transformDirty = true;
        queueRenderSnapshotDirty(world, entity);
    }

    inline bool hasAnyGeometryDescriptor(ECSWorld& world, Entity entity)
    {
        return world.hasComponent<StaticMeshComponent>(entity)
            || world.hasComponent<QuadMeshComponent>(entity)
            || world.hasComponent<DynamicMeshComponent>(entity)
            || world.hasComponent<WorldTextComponent>(entity);
    }

    inline bool hasRenderableGeometryDescriptor(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<StaticMeshComponent>(entity))
            return true;

        if (world.hasComponent<QuadMeshComponent>(entity))
            return true;

        if (world.hasComponent<DynamicMeshComponent>(entity))
        {
            const DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
            return !mesh.vertices.empty() && !mesh.indices.empty();
        }

        if (world.hasComponent<WorldTextComponent>(entity))
        {
            const WorldTextComponent& text = world.getComponent<WorldTextComponent>(entity);
            return !text.fontPath.empty() && !text.text.empty();
        }

        return false;
    }

    inline bool legacyMaterialDescriptorUsable(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<MaterialComponent>(entity))
            return false;

        const MaterialComponent& material = world.getComponent<MaterialComponent>(entity);
        return !material.texturePath.empty() || world.hasComponent<StaticMeshComponent>(entity);
    }

    inline bool worldTextMaterialDescriptorUsable(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<WorldTextComponent>(entity))
            return false;

        const WorldTextComponent& text = world.getComponent<WorldTextComponent>(entity);
        return !text.fontPath.empty() && !text.text.empty();
    }

    inline bool hasAnyMaterialSource(ECSWorld& world, Entity entity)
    {
        return world.hasComponent<MaterialComponent>(entity)
            || world.hasComponent<MaterialReferenceComponent>(entity)
            || world.hasComponent<WorldTextComponent>(entity);
    }

    inline bool hasRenderableMaterialDescriptor(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<MaterialReferenceComponent>(entity))
            return true;

        return legacyMaterialDescriptorUsable(world, entity)
            || worldTextMaterialDescriptorUsable(world, entity);
    }

    inline bool hasRenderableDescriptor(ECSWorld& world, Entity entity)
    {
        return hasRenderableGeometryDescriptor(world, entity)
            && hasRenderableMaterialDescriptor(world, entity);
    }

    inline bool meshGpuReady(ECSWorld& world, Entity entity)
    {
        return world.hasComponent<MeshGpuComponent>(entity)
            && world.getComponent<MeshGpuComponent>(entity).meshID != 0;
    }

    inline bool materialGpuReady(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<MaterialReferenceComponent>(entity))
            return false;

        const MaterialReferenceComponent& reference = world.getComponent<MaterialReferenceComponent>(entity);
        const MaterialGpuState* state = materialRuntime(world).getGpuState(reference.material);
        return state != nullptr && state->baseColorTextureID != 0;
    }

    inline void scheduleMeshGpuCleanup(ECSWorld& world,
                                       ECSWorld::EntityCommandBuffer& commands,
                                       Entity entity)
    {
        if (world.hasComponent<MeshGpuComponent>(entity))
            commands.removeComponent<MeshGpuComponent>(entity);
    }

    inline void scheduleRenderObjectCleanup(ECSWorld& world,
                                            ECSWorld::EntityCommandBuffer& commands,
                                            Entity entity)
    {
        if (world.hasComponent<RenderDirtyComponent>(entity))
            commands.removeComponent<RenderDirtyComponent>(entity);
        if (world.hasComponent<RenderGpuComponent>(entity))
            commands.removeComponent<RenderGpuComponent>(entity);
    }

    inline void scheduleRenderableCleanup(ECSWorld& world,
                                          ECSWorld::EntityCommandBuffer& commands,
                                          Entity entity)
    {
        scheduleMeshGpuCleanup(world, commands, entity);
        scheduleRenderObjectCleanup(world, commands, entity);
    }
}
