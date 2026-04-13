#pragma once

#include <chrono>
#include <limits>
#include <unordered_map>
#include <unordered_set>

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

    RenderExtractionSnapshot& build(ECSWorld& world)
    {
        const auto start = std::chrono::steady_clock::now();

        snapshot.cameraViewID = 0;
        snapshot.frustum.fill(glm::vec4(0.0f));
        snapshot.objectUploads.clear();
        staticActiveIDs.clear();
        dynamicActiveIDs.clear();
        staticActiveIDs.reserve(staticSnapshotMap.size());
        dynamicActiveIDs.reserve(dynamicSnapshotMap.size());
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

        world.forEach<RenderGpuComponent, MeshGpuComponent, MaterialGpuComponent>(
            [&](Entity e, RenderGpuComponent& rc, MeshGpuComponent& meshGpu, MaterialGpuComponent& matGpu)
        {
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                return;

            if (!rc.readyToRender)
                return;

            const RenderableID id = e.id;
            const bool isStatic = world.hasComponent<RenderStaticTag>(e);

            // Move entity to the correct store if its classification changed
            if (isStatic)
            {
                auto it = dynamicSnapshotMap.find(id);
                if (it != dynamicSnapshotMap.end())
                    dynamicSnapshotMap.erase(it);
            }
            else
            {
                auto it = staticSnapshotMap.find(id);
                if (it != staticSnapshotMap.end())
                    staticSnapshotMap.erase(it);
            }

            auto& targetMap = isStatic ? staticSnapshotMap : dynamicSnapshotMap;

            if (isStatic)
                staticActiveIDs.push_back(id);
            else
                dynamicActiveIDs.push_back(id);

            auto [it, inserted] = targetMap.try_emplace(id);
            RenderableSnapshot& renderable = it->second;

            bool transformDirty = true;
            bool materialDirty  = true;
            bool meshDirty      = true;
            if (world.hasComponent<RenderDirtyComponent>(e))
            {
                auto& dirty = world.getComponent<RenderDirtyComponent>(e);
                transformDirty = dirty.transformDirty;
                materialDirty  = dirty.materialDirty;
                meshDirty      = dirty.meshDirty;

                if (!inserted && !transformDirty && !materialDirty && !meshDirty)
                {
                    renderable.visible = true;
                    reusedCount += 1;
                }
                else
                {
                    dirtyUpdatedCount += 1;
                    if (isStatic) staticUpdatedCount  += 1;
                    else          dynamicUpdatedCount += 1;
                }

                dirty.transformDirty = false;
                dirty.materialDirty  = false;
                dirty.meshDirty      = false;
            }
            else
            {
                dirtyUpdatedCount += 1;
                if (isStatic) staticUpdatedCount  += 1;
                else          dynamicUpdatedCount += 1;
            }

            if (!inserted && !transformDirty && !materialDirty && !meshDirty)
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
        });

        // Prune stale entries from the static store
        {
            staticActiveIDSet.clear();
            staticActiveIDSet.reserve(staticActiveIDs.size());
            for (RenderableID id : staticActiveIDs)
                staticActiveIDSet.insert(id);

            for (auto it = staticSnapshotMap.begin(); it != staticSnapshotMap.end(); )
            {
                if (staticActiveIDSet.find(it->first) == staticActiveIDSet.end())
                    it = staticSnapshotMap.erase(it);
                else
                    ++it;
            }
        }

        // Prune stale entries from the dynamic store
        {
            dynamicActiveIDSet.clear();
            dynamicActiveIDSet.reserve(dynamicActiveIDs.size());
            for (RenderableID id : dynamicActiveIDs)
                dynamicActiveIDSet.insert(id);

            for (auto it = dynamicSnapshotMap.begin(); it != dynamicSnapshotMap.end(); )
            {
                if (dynamicActiveIDSet.find(it->first) == dynamicActiveIDSet.end())
                    it = dynamicSnapshotMap.erase(it);
                else
                    ++it;
            }
        }

        // Rebuild output: static renderables first, then dynamic
        snapshot.renderables.clear();
        snapshot.renderables.reserve(staticSnapshotMap.size() + dynamicSnapshotMap.size());
        for (RenderableID id : staticActiveIDs)
            snapshot.renderables.push_back(staticSnapshotMap.at(id));
        for (RenderableID id : dynamicActiveIDs)
            snapshot.renderables.push_back(dynamicSnapshotMap.at(id));

        const auto end = std::chrono::steady_clock::now();
        lastMetrics.buildCpuMs             = std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.renderableCount        = static_cast<uint32_t>(snapshot.renderables.size());
        lastMetrics.dirtyUpdatedCount      = dirtyUpdatedCount;
        lastMetrics.reusedCount            = reusedCount;
        lastMetrics.staticRenderableCount  = static_cast<uint32_t>(staticSnapshotMap.size());
        lastMetrics.dynamicRenderableCount = static_cast<uint32_t>(dynamicSnapshotMap.size());
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

private:
    RenderExtractionSnapshot snapshot;
    Metrics                  lastMetrics;

    std::unordered_map<RenderableID, RenderableSnapshot> staticSnapshotMap;
    std::unordered_map<RenderableID, RenderableSnapshot> dynamicSnapshotMap;

    std::vector<RenderableID>        staticActiveIDs;
    std::vector<RenderableID>        dynamicActiveIDs;
    std::unordered_set<RenderableID> staticActiveIDSet;
    std::unordered_set<RenderableID> dynamicActiveIDSet;

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
