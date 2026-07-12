#pragma once

#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
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
        struct MaterialBindingMetrics
        {
            uint32_t queuedMaterials = 0;
            uint32_t synchronizedMaterials = 0;
            uint32_t userInvalidations = 0;
            uint32_t fallbackSubstitutions = 0;
            uint32_t referenceAdds = 0;
            uint32_t referenceRemoves = 0;
            uint32_t fullMaterialScans = 0;
        };

        struct MaterialUserIndex
        {
            std::unordered_map<MaterialInstanceHandle, std::unordered_set<entity_id_type>> usersByMaterial;
            std::unordered_map<entity_id_type, MaterialInstanceHandle> materialByEntity;
        };

        std::unordered_set<entity_id_type> staticMeshRefreshEntities;
        std::unordered_set<entity_id_type> quadMeshRefreshEntities;
        std::unordered_set<entity_id_type> dynamicMeshRefreshEntities;
        std::unordered_set<entity_id_type> materialRefreshEntities;
        std::unordered_set<entity_id_type> renderObjectRefreshEntities;
        std::unordered_set<entity_id_type> cleanupEntities;
        std::vector<entity_id_type> renderTransformSyncEntities;
        std::vector<uint8_t> renderTransformSyncFlags;
        uint32_t renderTransformSyncQueueAttempts = 0;
        uint32_t renderTransformSyncQueueDeduplications = 0;
        MaterialUserIndex materialUserIndex;
        MaterialBindingMetrics materialMetrics;
    };

    using MaterialBindingMetrics = GeometryBindingLifecycleState::MaterialBindingMetrics;

    struct RenderTransformSyncQueueStats
    {
        uint32_t queueAttempts = 0;
        uint32_t queueDeduplications = 0;
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

    inline void ensureRenderTransformSyncFlagCapacity(std::vector<uint8_t>& flags, entity_id_type id)
    {
        const size_t index = static_cast<size_t>(id);
        if (index >= flags.size())
            flags.resize(index + 1, 0);
    }

    inline bool queueRenderTransformSync(ECSWorld& world, Entity entity)
    {
        if (!validInvalidationEntity(entity))
            return false;

        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        state.renderTransformSyncQueueAttempts += 1;
        ensureRenderTransformSyncFlagCapacity(state.renderTransformSyncFlags, entity.id);
        uint8_t& queued = state.renderTransformSyncFlags[static_cast<size_t>(entity.id)];
        if (queued != 0)
        {
            state.renderTransformSyncQueueDeduplications += 1;
            return false;
        }

        queued = 1;
        state.renderTransformSyncEntities.push_back(entity.id);
        return true;
    }

    inline std::vector<entity_id_type> takeRenderTransformSyncs(ECSWorld& world)
    {
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        std::vector<entity_id_type> pending;
        pending.swap(state.renderTransformSyncEntities);
        for (entity_id_type id : pending)
        {
            const size_t index = static_cast<size_t>(id);
            if (index < state.renderTransformSyncFlags.size())
                state.renderTransformSyncFlags[index] = 0;
        }
        return pending;
    }

    inline RenderTransformSyncQueueStats takeRenderTransformSyncQueueStats(ECSWorld& world)
    {
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        RenderTransformSyncQueueStats stats{
            state.renderTransformSyncQueueAttempts,
            state.renderTransformSyncQueueDeduplications
        };
        state.renderTransformSyncQueueAttempts = 0;
        state.renderTransformSyncQueueDeduplications = 0;
        return stats;
    }

    inline void queueMaterialRefresh(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).materialRefreshEntities.insert(entity.id);
    }

    inline void removeIndexedMaterialUser(GeometryBindingLifecycleState& state,
                                          entity_id_type entityId,
                                          MaterialInstanceHandle material)
    {
        auto usersIt = state.materialUserIndex.usersByMaterial.find(material);
        if (usersIt == state.materialUserIndex.usersByMaterial.end())
            return;

        if (usersIt->second.erase(entityId) != 0)
            state.materialMetrics.referenceRemoves += 1;

        if (usersIt->second.empty())
            state.materialUserIndex.usersByMaterial.erase(usersIt);
    }

    inline MaterialInstanceHandle registerMaterialUser(ECSWorld& world,
                                                       Entity entity,
                                                       MaterialInstanceHandle requestedMaterial,
                                                       bool queueSync = true)
    {
        MaterialRuntime& materials = materialRuntime(world);
        const MaterialInstanceHandle material = materials.resolveInstanceHandle(requestedMaterial);
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        auto& byEntity = state.materialUserIndex.materialByEntity;

        auto existing = byEntity.find(entity.id);
        if (existing != byEntity.end() && existing->second == material)
            return material;

        if (existing != byEntity.end())
            removeIndexedMaterialUser(state, entity.id, existing->second);

        auto& users = state.materialUserIndex.usersByMaterial[material];
        if (users.insert(entity.id).second)
            state.materialMetrics.referenceAdds += 1;
        byEntity[entity.id] = material;

        if (queueSync)
            materials.queueMaterialSync(material);
        return material;
    }

    inline void unregisterMaterialUser(ECSWorld& world, Entity entity)
    {
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        auto& byEntity = state.materialUserIndex.materialByEntity;
        auto existing = byEntity.find(entity.id);
        if (existing == byEntity.end())
            return;

        removeIndexedMaterialUser(state, entity.id, existing->second);
        byEntity.erase(existing);
    }

    inline std::vector<Entity> materialUsers(ECSWorld& world, MaterialInstanceHandle material)
    {
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        std::vector<Entity> users;
        auto usersIt = state.materialUserIndex.usersByMaterial.find(material);
        if (usersIt == state.materialUserIndex.usersByMaterial.end())
            return users;

        users.reserve(usersIt->second.size());
        for (entity_id_type entityId : usersIt->second)
            users.push_back(Entity{entityId});
        return users;
    }

    inline size_t materialUserCount(ECSWorld& world, MaterialInstanceHandle material)
    {
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        auto usersIt = state.materialUserIndex.usersByMaterial.find(material);
        return usersIt == state.materialUserIndex.usersByMaterial.end() ? 0u : usersIt->second.size();
    }

    inline MaterialInstanceHandle indexedMaterialForEntity(ECSWorld& world, Entity entity)
    {
        GeometryBindingLifecycleState& state = geometryBindingLifecycleState(world);
        auto it = state.materialUserIndex.materialByEntity.find(entity.id);
        return it == state.materialUserIndex.materialByEntity.end() ? MaterialInstanceHandle{} : it->second;
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

    inline void queueDynamicMeshGeometryRefresh(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).dynamicMeshRefreshEntities.insert(entity.id);
    }

    inline uint64_t nextDynamicMeshGeometryVersion(uint64_t version)
    {
        return version == std::numeric_limits<uint64_t>::max() ? 1 : version + 1;
    }

    inline void markDynamicMeshChanged(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<DynamicMeshComponent>(entity))
        {
            DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
            mesh.geometryVersion = nextDynamicMeshGeometryVersion(mesh.geometryVersion);
        }
        queueDynamicMeshGeometryRefresh(world, entity);
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

    inline bool worldTextMaterialDescriptorUsable(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<WorldTextComponent>(entity))
            return false;

        const WorldTextComponent& text = world.getComponent<WorldTextComponent>(entity);
        return !text.fontPath.empty() && !text.text.empty();
    }

    inline bool hasAnyMaterialSource(ECSWorld& world, Entity entity)
    {
        return world.hasComponent<MaterialReferenceComponent>(entity)
            || world.hasComponent<WorldTextComponent>(entity);
    }

    inline bool hasRenderableMaterialDescriptor(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<MaterialReferenceComponent>(entity))
            return true;

        return worldTextMaterialDescriptorUsable(world, entity);
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
