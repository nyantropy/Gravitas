#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <utility>

#include "BoundsComponent.h"
#include "CameraGpuComponent.h"
#include "CullFlagsComponent.h"
#include "ECSWorld.hpp"
#include "FrustumCuller.h"
#include "GraphicsConstants.h"
#include "LightExtraction.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderExtractionSnapshot.h"
#include "RenderGpuComponent.h"
#include "RenderInvalidationLifecycle.h"
#include "RenderStaticTag.h"
#include "TransformMatrixHelpers.h"
#include "WorldTransformComponent.h"

class RenderExtractionSnapshotBuilder
{
    public:
    struct Metrics
    {
        float    buildCpuMs             = 0.0f;
        uint32_t renderableCount        = 0;
        uint32_t dirtyUpdatedCount      = 0;
        uint32_t reusedCount            = 0;
        uint32_t staticRenderableCount  = 0;
        uint32_t dynamicRenderableCount = 0;
        uint32_t staticUpdatedCount     = 0;
        uint32_t dynamicUpdatedCount    = 0;
        uint32_t dirtyEntries           = 0;
        uint32_t updateWorkItems        = 0;
        uint32_t updateBatches          = 0;
        uint32_t refreshedEntries       = 0;
        uint32_t fullRebuilds           = 0;
        uint32_t storageReallocations   = 0;
        uint32_t maxBatchSize           = 0;
        float    averageBatchSize       = 0.0f;
        float    dirtyCollectionCpuMs   = 0.0f;
        float    inputGatherCpuMs       = 0.0f;
        float    entryRefreshCpuMs      = 0.0f;
        float    aggregateMergeCpuMs    = 0.0f;
    };

    RenderExtractionSnapshotBuilder() : slotToIndex(GraphicsConstants::MAX_RENDERABLE_OBJECTS, InvalidIndex)
    {
        snapshot.renderables.reserve(1024);
        snapshot.occupiedSlots.reserve(1024);
        entryMetadata.reserve(1024);
    }

    ~RenderExtractionSnapshotBuilder()
    {
        unregisterFromWorld();
    }

    RenderExtractionSnapshot& build(ECSWorld& world)
    {
        const auto start = std::chrono::steady_clock::now();
        registerWithWorld(world);

        m_snapshotDirty        = m_pendingSnapshotDirty;
        m_pendingSnapshotDirty = false;
        snapshot.cameraViewID  = 0;
        snapshot.frustum.fill(glm::vec4(0.0f));
        snapshot.cameraViewMatrix = glm::mat4(1.0f);
        snapshot.cameraWorldPosition = {0.0f, 0.0f, 0.0f};
        glm::mat4 cameraProjectionMatrix = glm::mat4(1.0f);
        snapshot.objectUploads.clear();
        snapshot.cameraUploads.clear();
        snapshot.materialFrameData.clear();
        uint32_t                                 dirtyUpdatedCount   = 0;
        uint32_t                                 reusedCount         = 0;
        uint32_t                                 staticUpdatedCount  = 0;
        uint32_t                                 dynamicUpdatedCount = 0;
        gts::rendering::RenderInvalidationState& invalidation        = gts::rendering::renderInvalidationState(world);
        const uint32_t                           dirtyEntryCount =
            static_cast<uint32_t>(invalidation.snapshotDirtyEntities.size());
        snapshot.objectUploads.reserve(invalidation.snapshotDirtyEntities.size());

        world.forEach<CameraGpuComponent>(
            [&](Entity entity, CameraGpuComponent& gpu)
            {
                if (snapshot.cameraViewID != 0)
                    return;

                if (!gpu.active)
                    return;

                snapshot.cameraViewID = gpu.viewID;
                snapshot.cameraViewMatrix = gpu.viewMatrix;
                cameraProjectionMatrix = gpu.projMatrix;
                snapshot.cameraWorldPosition = cameraWorldPosition(world, entity, gpu.viewMatrix);
                snapshot.frustum      = FrustumCuller::extractPlanesFromMatrix(gpu.projMatrix * gpu.viewMatrix);
            });
        snapshot.lighting = gts::rendering::extractLightingFrameData(world, snapshot.cameraWorldPosition);
        if (snapshot.cameraViewID != 0)
        {
            snapshot.cameraUploads.push_back({
                snapshot.cameraViewID,
                snapshot.cameraViewMatrix,
                cameraProjectionMatrix,
                snapshot.cameraWorldPosition,
                snapshot.lighting
            });
        }
        const bool cameraChanged = updateCameraVersion();

        const auto dirtyCollectionStart = std::chrono::steady_clock::now();
        const size_t renderableCapacityBefore = snapshot.renderables.capacity();
        const size_t occupiedSlotCapacityBefore = snapshot.occupiedSlots.capacity();
        const size_t metadataCapacityBefore = entryMetadata.capacity();
        snapshotUpdateItems.clear();
        snapshotUpdateItems.reserve(invalidation.snapshotDirtyEntities.size());
        snapshotBatchRanges.clear();
        const auto dirtyCollectionEnd = std::chrono::steady_clock::now();

        const auto inputGatherStart = std::chrono::steady_clock::now();
        for (entity_id_type entityId : invalidation.snapshotDirtyEntities)
        {
            Entity e{entityId};
            if (!world.hasComponent<RenderDirtyComponent>(e) || !world.hasComponent<RenderGpuComponent>(e) ||
                !world.hasComponent<MeshGpuComponent>(e) || !world.hasComponent<MaterialReferenceComponent>(e))
            {
                continue;
            }

            RenderDirtyComponent& dirty   = world.getComponent<RenderDirtyComponent>(e);
            RenderGpuComponent&   rc      = world.getComponent<RenderGpuComponent>(e);
            MeshGpuComponent&     meshGpu = world.getComponent<MeshGpuComponent>(e);
            MaterialReferenceComponent& materialRef = world.getComponent<MaterialReferenceComponent>(e);
            gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(world);
            const MaterialGpuState* materialGpu = materials.getGpuState(materialRef.material);
            if (materialGpu == nullptr)
                continue;

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                continue;

            if (!rc.readyToRender)
                continue;

            const RenderableID id              = e.id;
            const bool         isStatic        = world.hasComponent<RenderStaticTag>(e);
            const bool         transformDirty  = dirty.transformDirty;
            const bool         materialDirty   = dirty.materialDirty;
            const bool         meshDirty       = dirty.meshDirty;
            const bool         objectDataDirty = dirty.objectDataDirty;
            const bool         anyDirty        = transformDirty || materialDirty || meshDirty || objectDataDirty;
            const uint32_t     existingIndex   = slotToIndex[rc.objectSSBOSlot];
            const bool         inserted        = existingIndex == InvalidIndex;

            std::vector<SubmeshMaterialRuntimeBinding> submeshMaterials;
            submeshMaterials.reserve(meshGpu.submeshMaterials.size());
            for (MaterialInstanceHandle submeshMaterial : meshGpu.submeshMaterials)
            {
                if (!submeshMaterial.valid())
                {
                    submeshMaterials.push_back({});
                    continue;
                }

                const MaterialGpuState* submeshMaterialGpu = materials.getGpuState(submeshMaterial);
                if (submeshMaterialGpu == nullptr)
                {
                    submeshMaterials.push_back({});
                    continue;
                }

                submeshMaterials.push_back({
                    submeshMaterialGpu->instance,
                    submeshMaterialGpu->gpuHandle,
                    submeshMaterialGpu->variantKey,
                    renderQueueForAlphaMode(submeshMaterialGpu->renderState.alphaMode)
                });
            }

            if (!inserted && !anyDirty)
            {
                snapshot.renderables[existingIndex].visible = true;
                reusedCount += 1;
                continue;
            }

            uint32_t index = existingIndex;
            if (inserted)
            {
                index           = appendRenderable(id, e, rc.objectSSBOSlot, isStatic);
                m_snapshotDirty = true;
            }
            else if (entryMetadata[index].isStatic != isStatic)
            {
                if (entryMetadata[index].isStatic)
                {
                    if (staticRenderableCount > 0)
                        staticRenderableCount -= 1;
                    dynamicRenderableCount += 1;
                }
                else
                {
                    if (dynamicRenderableCount > 0)
                        dynamicRenderableCount -= 1;
                    staticRenderableCount += 1;
                }
                entryMetadata[index].isStatic = isStatic;
            }

            if (anyDirty)
            {
                m_snapshotDirty = true;
                dirtyUpdatedCount += 1;
                if (isStatic)
                    staticUpdatedCount += 1;
                else
                    dynamicUpdatedCount += 1;
            }

            snapshotUpdateItems.push_back({
                e,
                id,
                index,
                rc.objectSSBOSlot,
                inserted,
                isStatic,
                transformDirty,
                materialDirty,
                meshDirty,
                objectDataDirty,
                rc.modelMatrix,
                rc.uvTransform,
                meshGpu.meshID,
                materialGpu->instance,
                materialGpu->gpuHandle,
                materialGpu->variantKey,
                renderQueueForAlphaMode(materialGpu->renderState.alphaMode),
                std::move(submeshMaterials)
            });
        }
        const auto inputGatherEnd = std::chrono::steady_clock::now();

        for (size_t begin = 0; begin < snapshotUpdateItems.size(); begin += TargetBatchSize)
        {
            const size_t end = std::min(begin + TargetBatchSize, snapshotUpdateItems.size());
            snapshotBatchRanges.push_back({
                static_cast<uint32_t>(begin),
                static_cast<uint32_t>(end)
            });
        }

        const auto entryRefreshStart = std::chrono::steady_clock::now();
        uint32_t refreshedEntries = 0;
        uint32_t maxBatchSize = 0;
        for (const SnapshotBatchRange& range : snapshotBatchRanges)
        {
            maxBatchSize = std::max(maxBatchSize, range.end - range.begin);
            for (uint32_t itemIndex = range.begin; itemIndex < range.end; ++itemIndex)
            {
                const SnapshotUpdateItem& item = snapshotUpdateItems[itemIndex];
                RenderableSnapshot& renderable = snapshot.renderables[item.index];

                renderable.id             = item.id;
                renderable.entity         = item.entity;
                renderable.objectSSBOSlot = item.objectSlot;
                renderable.visible        = true;

                if (item.inserted || item.transformDirty)
                {
                    renderable.modelMatrix = item.modelMatrix;
                    updateBounds(world, item.entity, renderable);
                    updateCameraDepth(renderable);
                }

                if (item.inserted || item.objectDataDirty)
                    renderable.uvTransform = item.uvTransform;

                bool sortKeyNeedsUpdate = item.inserted;
                if (item.inserted || item.meshDirty)
                {
                    renderable.meshID = item.meshID;
                    sortKeyNeedsUpdate = true;
                }

                if (item.inserted || item.materialDirty)
                {
                    renderable.material    = item.material;
                    renderable.materialGpu = item.materialGpu;
                    renderable.variantKey  = item.variantKey;
                    renderable.renderQueue = item.renderQueue;
                    sortKeyNeedsUpdate     = true;
                }

                if (item.inserted || item.meshDirty || item.materialDirty)
                    renderable.submeshMaterials = item.submeshMaterials;

                if (item.inserted || item.transformDirty || item.objectDataDirty)
                {
                    snapshot.objectUploads.push_back(
                        {item.objectSlot, renderable.modelMatrix, renderable.uvTransform});
                }

                if (sortKeyNeedsUpdate)
                    renderable.sortKey = makeSortKey(renderable);

                if (world.hasComponent<RenderDirtyComponent>(item.entity))
                {
                    RenderDirtyComponent& dirty = world.getComponent<RenderDirtyComponent>(item.entity);
                    dirty.transformDirty  = false;
                    dirty.materialDirty   = false;
                    dirty.meshDirty       = false;
                    dirty.objectDataDirty = false;
                }
                refreshedEntries += 1;
            }
        }
        const auto entryRefreshEnd = std::chrono::steady_clock::now();

        const auto aggregateStart = std::chrono::steady_clock::now();
        if (cameraChanged)
        {
            for (RenderableSnapshot& renderable : snapshot.renderables)
                updateCameraDepth(renderable);
        }
        populateMaterialFrameData(world);
        gts::rendering::clearSnapshotDirtyQueue(invalidation);
        if (m_snapshotDirty)
            snapshot.contentVersion = ++contentVersion;
        const auto aggregateEnd = std::chrono::steady_clock::now();

        const auto end                     = std::chrono::steady_clock::now();
        lastMetrics.buildCpuMs             = std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.renderableCount        = static_cast<uint32_t>(snapshot.renderables.size());
        lastMetrics.dirtyUpdatedCount      = dirtyUpdatedCount;
        lastMetrics.reusedCount            = static_cast<uint32_t>(snapshot.renderables.size() >= dirtyUpdatedCount
                                                                       ? snapshot.renderables.size() - dirtyUpdatedCount
                                                                       : reusedCount);
        lastMetrics.staticRenderableCount  = staticRenderableCount;
        lastMetrics.dynamicRenderableCount = dynamicRenderableCount;
        lastMetrics.staticUpdatedCount     = staticUpdatedCount;
        lastMetrics.dynamicUpdatedCount    = dynamicUpdatedCount;
        lastMetrics.dirtyEntries           = dirtyEntryCount;
        lastMetrics.updateWorkItems        = static_cast<uint32_t>(snapshotUpdateItems.size());
        lastMetrics.updateBatches          = static_cast<uint32_t>(snapshotBatchRanges.size());
        lastMetrics.refreshedEntries       = refreshedEntries;
        lastMetrics.fullRebuilds           = 0;
        lastMetrics.storageReallocations   =
            (snapshot.renderables.capacity() != renderableCapacityBefore ? 1u : 0u) +
            (snapshot.occupiedSlots.capacity() != occupiedSlotCapacityBefore ? 1u : 0u) +
            (entryMetadata.capacity() != metadataCapacityBefore ? 1u : 0u);
        lastMetrics.maxBatchSize           = maxBatchSize;
        lastMetrics.averageBatchSize       = snapshotBatchRanges.empty()
            ? 0.0f
            : static_cast<float>(snapshotUpdateItems.size()) /
                static_cast<float>(snapshotBatchRanges.size());
        lastMetrics.dirtyCollectionCpuMs   =
            std::chrono::duration<float, std::milli>(
                dirtyCollectionEnd - dirtyCollectionStart).count();
        lastMetrics.inputGatherCpuMs       =
            std::chrono::duration<float, std::milli>(
                inputGatherEnd - inputGatherStart).count();
        lastMetrics.entryRefreshCpuMs      =
            std::chrono::duration<float, std::milli>(
                entryRefreshEnd - entryRefreshStart).count();
        lastMetrics.aggregateMergeCpuMs    =
            std::chrono::duration<float, std::milli>(
                aggregateEnd - aggregateStart).count();
        return snapshot;
    }

    Metrics getLastMetrics() const
    {
        return lastMetrics;
    }

    const RenderExtractionSnapshot& getLatestSnapshot() const
    {
        return snapshot;
    }

    void resetSceneState()
    {
        unregisterFromWorld();
        resetPersistentState();
        snapshot.contentVersion = 0;
        snapshot.cameraVersion  = 0;
        contentVersion          = 0;
        cameraVersion           = 0;
        lastFrustum.fill(glm::vec4(0.0f));
        lastCameraViewID = 0;
        lastCameraPosition = {0.0f, 0.0f, 0.0f};
        lastMetrics      = {};
        m_snapshotDirty  = true;
    }

    bool isSnapshotDirty() const
    {
        return m_snapshotDirty;
    }

    static void notifyRenderableRemoved(ECSWorld& world, Entity entity, const RenderGpuComponent& renderGpu)
    {
        auto& builders = builderRegistry();
        auto  it       = builders.find(&world);
        if (it == builders.end() || it->second == nullptr)
            return;

        it->second->removeRenderable(entity, renderGpu.objectSSBOSlot);
    }

    private:
    struct SnapshotEntryMetadata
    {
        RenderableID id       = 0;
        bool         isStatic = false;
    };

    static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();
    static constexpr uint32_t TargetBatchSize = 1024;

    struct SnapshotUpdateItem
    {
        Entity                entity = INVALID_ENTITY;
        RenderableID          id = 0;
        uint32_t              index = InvalidIndex;
        ssbo_id_type          objectSlot = RENDERABLE_SLOT_UNALLOCATED;
        bool                  inserted = false;
        bool                  isStatic = false;
        bool                  transformDirty = false;
        bool                  materialDirty = false;
        bool                  meshDirty = false;
        bool                  objectDataDirty = false;
        glm::mat4             modelMatrix = glm::mat4(1.0f);
        glm::vec4             uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
        mesh_id_type          meshID = 0;
        MaterialInstanceHandle material;
        MaterialGpuHandle     materialGpu;
        MaterialVariantKey    variantKey{};
        RenderQueue           renderQueue = RenderQueue::Opaque;
        std::vector<SubmeshMaterialRuntimeBinding> submeshMaterials;
    };

    struct SnapshotBatchRange
    {
        uint32_t begin = 0;
        uint32_t end = 0;
    };

    RenderExtractionSnapshot snapshot;
    Metrics                  lastMetrics;
    // Persistent dense snapshot storage.
    // Indexing contract:
    //   - snapshot.renderables[i], snapshot.occupiedSlots[i], and entryMetadata[i]
    //     describe the same live renderable.
    //   - slotToIndex provides O(1) lookup from SSBO slot to dense index.
    //   - entityToIndex mirrors that lookup for removal safety during teardown.
    // Incremental update contract:
    //   - build() only rewrites entries for renderables whose RenderDirtyComponent
    //     flags are set or that do not yet exist in the persistent vectors.
    //   - removals are authoritative via the RenderGpuComponent removal callback,
    //     which swap-removes the dense entry and marks m_snapshotDirty.
    //   - m_snapshotDirty is raised on add/remove/update so downstream stages can
    //     skip command regeneration when the extraction snapshot is unchanged.
    std::vector<SnapshotEntryMetadata>         entryMetadata;
    std::vector<SnapshotUpdateItem>            snapshotUpdateItems;
    std::vector<SnapshotBatchRange>            snapshotBatchRanges;
    std::vector<uint32_t>                      slotToIndex;
    std::unordered_map<RenderableID, uint32_t> entityToIndex;
    ECSWorld*                                  registeredWorld = nullptr;
    uint64_t                                   contentVersion  = 0;
    uint64_t                                   cameraVersion   = 0;
    FrustumPlanes                              lastFrustum{};
    view_id_type                               lastCameraViewID       = 0;
    glm::vec3                                  lastCameraPosition     = {0.0f, 0.0f, 0.0f};
    bool                                       cameraCacheValid       = false;
    uint32_t                                   staticRenderableCount  = 0;
    uint32_t                                   dynamicRenderableCount = 0;
    bool                                       m_pendingSnapshotDirty = true;
    bool                                       m_snapshotDirty        = true;

    static std::unordered_map<ECSWorld*, RenderExtractionSnapshotBuilder*>& builderRegistry()
    {
        static std::unordered_map<ECSWorld*, RenderExtractionSnapshotBuilder*> registry;
        return registry;
    }

    void registerWithWorld(ECSWorld& world)
    {
        if (registeredWorld == &world)
            return;

        unregisterFromWorld();
        resetPersistentState();
        builderRegistry()[&world] = this;
        registeredWorld           = &world;
        m_pendingSnapshotDirty    = true;
        m_snapshotDirty           = true;
    }

    void unregisterFromWorld()
    {
        if (registeredWorld == nullptr)
            return;

        auto& registry = builderRegistry();
        auto  it       = registry.find(registeredWorld);
        if (it != registry.end() && it->second == this)
            registry.erase(it);
        registeredWorld = nullptr;
    }

    void resetPersistentState()
    {
        snapshot.renderables.clear();
        snapshot.occupiedSlots.clear();
        snapshot.objectUploads.clear();
        snapshot.cameraUploads.clear();
        snapshot.materialFrameData.clear();
        snapshot.lighting = gts::rendering::defaultLightingFrameData();
        snapshot.cameraWorldPosition = {0.0f, 0.0f, 0.0f};
        entryMetadata.clear();
        entityToIndex.clear();
        std::fill(slotToIndex.begin(), slotToIndex.end(), InvalidIndex);
        staticRenderableCount      = 0;
        dynamicRenderableCount     = 0;
        snapshot.visibilityVersion = 0;
        cameraCacheValid           = false;
        m_pendingSnapshotDirty     = true;
    }

    uint32_t appendRenderable(RenderableID id, Entity entity, ssbo_id_type slot, bool isStatic)
    {
        const uint32_t index = static_cast<uint32_t>(snapshot.renderables.size());
        snapshot.renderables.emplace_back();
        snapshot.occupiedSlots.push_back(slot);
        entryMetadata.push_back(SnapshotEntryMetadata{id, isStatic});
        entityToIndex[id] = index;
        slotToIndex[slot] = index;

        if (isStatic)
            staticRenderableCount += 1;
        else
            dynamicRenderableCount += 1;

        auto& renderable          = snapshot.renderables.back();
        renderable.id             = id;
        renderable.entity         = entity;
        renderable.objectSSBOSlot = slot;
        renderable.visible        = true;
        return index;
    }

    void removeRenderable(Entity entity, ssbo_id_type slot)
    {
        if (snapshot.renderables.empty())
            return;

        uint32_t index = InvalidIndex;
        if (slot != RENDERABLE_SLOT_UNALLOCATED && slot < slotToIndex.size())
            index = slotToIndex[slot];

        if (index == InvalidIndex)
        {
            auto it = entityToIndex.find(entity.id);
            if (it == entityToIndex.end())
                return;
            index = it->second;
        }

        const uint32_t              lastIndex   = static_cast<uint32_t>(snapshot.renderables.size() - 1);
        const SnapshotEntryMetadata removedMeta = entryMetadata[index];
        const ssbo_id_type          removedSlot = snapshot.occupiedSlots[index];

        if (removedMeta.isStatic)
        {
            if (staticRenderableCount > 0)
                staticRenderableCount -= 1;
        }
        else if (dynamicRenderableCount > 0)
        {
            dynamicRenderableCount -= 1;
        }

        if (index != lastIndex)
        {
            snapshot.renderables[index]   = std::move(snapshot.renderables.back());
            snapshot.occupiedSlots[index] = snapshot.occupiedSlots.back();
            entryMetadata[index]          = entryMetadata.back();

            const RenderableID movedID   = entryMetadata[index].id;
            const ssbo_id_type movedSlot = snapshot.occupiedSlots[index];
            entityToIndex[movedID]       = index;
            slotToIndex[movedSlot]       = index;
        }

        snapshot.renderables.pop_back();
        snapshot.occupiedSlots.pop_back();
        entryMetadata.pop_back();
        entityToIndex.erase(removedMeta.id);
        if (removedSlot != RENDERABLE_SLOT_UNALLOCATED && removedSlot < slotToIndex.size())
            slotToIndex[removedSlot] = InvalidIndex;

        m_pendingSnapshotDirty = true;
        m_snapshotDirty        = true;
    }

    static AABB transformBounds(const BoundsComponent& bounds, const glm::mat4& modelMatrix)
    {
        const glm::vec3 localCenter  = (bounds.min + bounds.max) * 0.5f;
        const glm::vec3 localExtents = (bounds.max - bounds.min) * 0.5f;
        const glm::vec3 worldCenter  = glm::vec3(modelMatrix * glm::vec4(localCenter, 1.0f));

        const glm::mat3 linear(modelMatrix);
        const glm::mat3 absLinear{glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2])};
        const glm::vec3 worldExtents = absLinear * localExtents;

        return {worldCenter - worldExtents, worldCenter + worldExtents};
    }

    static bool sameFrustum(const FrustumPlanes& lhs, const FrustumPlanes& rhs)
    {
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            if (lhs[i] != rhs[i])
                return false;
        }
        return true;
    }

    bool updateCameraVersion()
    {
        if (cameraCacheValid
            && lastCameraViewID == snapshot.cameraViewID
            && lastCameraPosition == snapshot.cameraWorldPosition
            && sameFrustum(lastFrustum, snapshot.frustum))
        {
            return false;
        }

        lastCameraViewID       = snapshot.cameraViewID;
        lastCameraPosition     = snapshot.cameraWorldPosition;
        lastFrustum            = snapshot.frustum;
        cameraCacheValid       = true;
        snapshot.cameraVersion = ++cameraVersion;
        return true;
    }

    static glm::vec3 cameraWorldPosition(ECSWorld& world, Entity entity, const glm::mat4& viewMatrix)
    {
        if (world.hasComponent<WorldTransformComponent>(entity))
        {
            return gts::transform::worldPositionFromMatrix(
                world.getComponent<WorldTransformComponent>(entity).matrix);
        }

        return glm::vec3(glm::inverse(viewMatrix)[3]);
    }

    static uint64_t makeSortKey(const RenderableSnapshot& renderable)
    {
        const uint64_t queue = static_cast<uint64_t>(renderable.renderQueue) & 0x3ull;
        const uint64_t variant = renderable.variantKey.value & 0xFFFFull;
        const uint64_t material =
            ((static_cast<uint64_t>(renderable.materialGpu.id) & 0xFFFFull) << 8)
            | (static_cast<uint64_t>(renderable.materialGpu.generation) & 0xFFull);
        const uint64_t mesh = static_cast<uint64_t>(renderable.meshID) & 0xFFFFull;
        const uint64_t slot = static_cast<uint64_t>(renderable.objectSSBOSlot) & 0xFFull;

        return (queue << 62)
             | (variant << 46)
             | ((material & 0xFFFFFFull) << 22)
             | (mesh << 6)
             | slot;
    }

    void updateCameraDepth(RenderableSnapshot& renderable) const
    {
        glm::vec3 worldCenter = glm::vec3(renderable.modelMatrix[3]);
        if (renderable.hasBounds)
            worldCenter = (renderable.worldBounds.min + renderable.worldBounds.max) * 0.5f;

        const glm::vec4 viewPosition = snapshot.cameraViewMatrix * glm::vec4(worldCenter, 1.0f);
        renderable.cameraDepth = -viewPosition.z;
    }

    void populateMaterialFrameData(ECSWorld& world)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(world);
        snapshot.materialFrameData.reserve(snapshot.renderables.size());
        for (const RenderableSnapshot& renderable : snapshot.renderables)
        {
            const MaterialGpuState* materialGpu = materials.getGpuState(renderable.material);
            if (materialGpu != nullptr)
                snapshot.materialFrameData.upsert(makeMaterialFrameState(*materialGpu));

            for (const SubmeshMaterialRuntimeBinding& submeshMaterial : renderable.submeshMaterials)
            {
                if (!submeshMaterial.valid())
                    continue;

                const MaterialGpuState* submeshMaterialGpu =
                    materials.getGpuState(submeshMaterial.material);
                if (submeshMaterialGpu != nullptr)
                    snapshot.materialFrameData.upsert(makeMaterialFrameState(*submeshMaterialGpu));
            }
        }
    }

    static void updateBounds(ECSWorld& world, Entity e, RenderableSnapshot& renderable)
    {
        bool shouldBuildBounds = world.hasComponent<BoundsComponent>(e);
        if (shouldBuildBounds && world.hasComponent<CullFlagsComponent>(e))
        {
            const auto& flags = world.getComponent<CullFlagsComponent>(e);
            if (flags.neverCull || !flags.cullEnabled)
                shouldBuildBounds = false;
        }

        renderable.hasBounds = false;
        if (!shouldBuildBounds)
            return;

        const auto& bounds     = world.getComponent<BoundsComponent>(e);
        renderable.worldBounds = transformBounds(bounds, renderable.modelMatrix);
        renderable.hasBounds   = true;
    }
};
