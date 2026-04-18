#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>

#include "BoundsComponent.h"
#include "CameraGpuComponent.h"
#include "CullFlagsComponent.h"
#include "EngineConfig.h"
#include "ECSWorld.hpp"
#include "FrustumCuller.h"
#include "MaterialGpuComponent.h"
#include "MeshGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderExtractionSnapshot.h"
#include "RenderGpuComponent.h"
#include "RenderStaticTag.h"

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
    };

    RenderExtractionSnapshotBuilder()
        : slotToIndex(EngineConfig::MAX_RENDERABLE_OBJECTS, InvalidIndex)
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

        m_snapshotDirty = m_pendingSnapshotDirty;
        m_pendingSnapshotDirty = false;
        snapshot.cameraViewID = 0;
        snapshot.frustum.fill(glm::vec4(0.0f));
        snapshot.objectUploads.clear();
        uint32_t dirtyUpdatedCount  = 0;
        uint32_t reusedCount        = 0;
        uint32_t staticUpdatedCount = 0;
        uint32_t dynamicUpdatedCount = 0;

        world.forEach<CameraGpuComponent>([&](Entity, CameraGpuComponent& gpu)
        {
            if (snapshot.cameraViewID != 0)
                return;

            if (!gpu.active)
                return;

            snapshot.cameraViewID = gpu.viewID;
            snapshot.frustum = FrustumCuller::extractPlanesFromMatrix(gpu.projMatrix * gpu.viewMatrix);
        });

        world.forEach<RenderDirtyComponent, RenderGpuComponent, MeshGpuComponent, MaterialGpuComponent>(
            [&](Entity e,
                RenderDirtyComponent& dirty,
                RenderGpuComponent& rc,
                MeshGpuComponent& meshGpu,
                MaterialGpuComponent& matGpu)
        {
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                return;

            if (!rc.readyToRender)
                return;

            const RenderableID id = e.id;
            const bool isStatic = world.hasComponent<RenderStaticTag>(e);
            const bool transformDirty = dirty.transformDirty;
            const bool materialDirty  = dirty.materialDirty;
            const bool meshDirty      = dirty.meshDirty;
            const bool anyDirty = transformDirty || materialDirty || meshDirty;
            const uint32_t existingIndex = slotToIndex[rc.objectSSBOSlot];
            const bool inserted = existingIndex == InvalidIndex;

            if (!inserted && !anyDirty)
            {
                snapshot.renderables[existingIndex].visible = true;
                reusedCount += 1;
                return;
            }

            uint32_t index = existingIndex;
            if (inserted)
            {
                index = appendRenderable(id, e, rc.objectSSBOSlot, isStatic);
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
                if (isStatic) staticUpdatedCount  += 1;
                else          dynamicUpdatedCount += 1;
            }

            RenderableSnapshot& renderable = snapshot.renderables[index];
            if (!inserted && !anyDirty)
                return;

            renderable.id             = id;
            renderable.entity         = e;
            renderable.objectSSBOSlot = rc.objectSSBOSlot;
            renderable.visible        = true;

            if (inserted || transformDirty)
            {
                renderable.modelMatrix = rc.modelMatrix;
                updateBounds(world, e, renderable);
                snapshot.objectUploads.push_back({rc.objectSSBOSlot, rc.modelMatrix});
            }

            bool sortKeyNeedsUpdate = inserted;
            if (inserted || meshDirty)
            {
                renderable.meshID  = meshGpu.meshID;
                sortKeyNeedsUpdate = true;
            }

            if (inserted || materialDirty)
            {
                renderable.textureID   = matGpu.textureID;
                renderable.alpha       = matGpu.alpha;
                renderable.doubleSided = matGpu.doubleSided;
                sortKeyNeedsUpdate     = true;
            }

            if (sortKeyNeedsUpdate)
                renderable.sortKey = makeSortKey(renderable);

            dirty.transformDirty = false;
            dirty.materialDirty  = false;
            dirty.meshDirty      = false;
        });

        const auto end = std::chrono::steady_clock::now();
        lastMetrics.buildCpuMs             = std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.renderableCount        = static_cast<uint32_t>(snapshot.renderables.size());
        lastMetrics.dirtyUpdatedCount      = dirtyUpdatedCount;
        lastMetrics.reusedCount            = reusedCount;
        lastMetrics.staticRenderableCount  = staticRenderableCount;
        lastMetrics.dynamicRenderableCount = dynamicRenderableCount;
        lastMetrics.staticUpdatedCount     = staticUpdatedCount;
        lastMetrics.dynamicUpdatedCount    = dynamicUpdatedCount;
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

    bool isSnapshotDirty() const
    {
        return m_snapshotDirty;
    }

    static void notifyRenderableRemoved(ECSWorld& world, Entity entity, const RenderGpuComponent& renderGpu)
    {
        auto& builders = builderRegistry();
        auto it = builders.find(&world);
        if (it == builders.end() || it->second == nullptr)
            return;

        it->second->removeRenderable(entity, renderGpu.objectSSBOSlot);
    }

private:
    struct SnapshotEntryMetadata
    {
        RenderableID id = 0;
        bool         isStatic = false;
    };

    static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

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
    std::vector<SnapshotEntryMetadata>   entryMetadata;
    std::vector<uint32_t>                slotToIndex;
    std::unordered_map<RenderableID, uint32_t> entityToIndex;
    ECSWorld*                            registeredWorld = nullptr;
    uint32_t                             staticRenderableCount = 0;
    uint32_t                             dynamicRenderableCount = 0;
    bool                                 m_pendingSnapshotDirty = true;
    bool                                 m_snapshotDirty = true;

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
        registeredWorld = &world;
        m_pendingSnapshotDirty = true;
        m_snapshotDirty = true;
    }

    void unregisterFromWorld()
    {
        if (registeredWorld == nullptr)
            return;

        auto& registry = builderRegistry();
        auto it = registry.find(registeredWorld);
        if (it != registry.end() && it->second == this)
            registry.erase(it);
        registeredWorld = nullptr;
    }

    void resetPersistentState()
    {
        snapshot.renderables.clear();
        snapshot.occupiedSlots.clear();
        snapshot.objectUploads.clear();
        entryMetadata.clear();
        entityToIndex.clear();
        std::fill(slotToIndex.begin(), slotToIndex.end(), InvalidIndex);
        staticRenderableCount = 0;
        dynamicRenderableCount = 0;
        m_pendingSnapshotDirty = true;
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

        auto& renderable = snapshot.renderables.back();
        renderable.id = id;
        renderable.entity = entity;
        renderable.objectSSBOSlot = slot;
        renderable.visible = true;
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

        const uint32_t lastIndex = static_cast<uint32_t>(snapshot.renderables.size() - 1);
        const SnapshotEntryMetadata removedMeta = entryMetadata[index];
        const ssbo_id_type removedSlot = snapshot.occupiedSlots[index];

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
            snapshot.renderables[index] = std::move(snapshot.renderables.back());
            snapshot.occupiedSlots[index] = snapshot.occupiedSlots.back();
            entryMetadata[index] = entryMetadata.back();

            const RenderableID movedID = entryMetadata[index].id;
            const ssbo_id_type movedSlot = snapshot.occupiedSlots[index];
            entityToIndex[movedID] = index;
            slotToIndex[movedSlot] = index;
        }

        snapshot.renderables.pop_back();
        snapshot.occupiedSlots.pop_back();
        entryMetadata.pop_back();
        entityToIndex.erase(removedMeta.id);
        if (removedSlot != RENDERABLE_SLOT_UNALLOCATED && removedSlot < slotToIndex.size())
            slotToIndex[removedSlot] = InvalidIndex;

        m_pendingSnapshotDirty = true;
        m_snapshotDirty = true;
    }

    static AABB transformBounds(const BoundsComponent& bounds, const glm::mat4& modelMatrix)
    {
        glm::vec3 corners[8] = {
            { bounds.min.x, bounds.min.y, bounds.min.z },
            { bounds.max.x, bounds.min.y, bounds.min.z },
            { bounds.min.x, bounds.max.y, bounds.min.z },
            { bounds.max.x, bounds.max.y, bounds.min.z },
            { bounds.min.x, bounds.min.y, bounds.max.z },
            { bounds.max.x, bounds.min.y, bounds.max.z },
            { bounds.min.x, bounds.max.y, bounds.max.z },
            { bounds.max.x, bounds.max.y, bounds.max.z }
        };

        glm::vec3 worldMin{std::numeric_limits<float>::max()};
        glm::vec3 worldMax{std::numeric_limits<float>::lowest()};

        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 worldCorner = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }

        return {worldMin, worldMax};
    }

    static uint64_t makeSortKey(const RenderableSnapshot& renderable)
    {
        const uint64_t sidedness = renderable.doubleSided ? 1ull : 0ull;
        return (sidedness << 63)
            | (static_cast<uint64_t>(renderable.meshID) << 32)
            | static_cast<uint64_t>(renderable.textureID);
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

        const auto& bounds = world.getComponent<BoundsComponent>(e);
        renderable.worldBounds = transformBounds(bounds, renderable.modelMatrix);
        renderable.hasBounds = true;
    }
};
