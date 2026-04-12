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

class RenderExtractionSnapshotBuilder
{
public:
    struct Metrics
    {
        float    buildCpuMs       = 0.0f;
        uint32_t renderableCount  = 0;
        uint32_t dirtyUpdatedCount = 0;
        uint32_t reusedCount      = 0;
    };

    RenderExtractionSnapshot& build(ECSWorld& world)
    {
        const auto start = std::chrono::steady_clock::now();

        snapshot.cameraViewID = 0;
        snapshot.frustum.fill(glm::vec4(0.0f));
        activeIDs.clear();
        activeIDs.reserve(snapshotMap.size());
        uint32_t dirtyUpdatedCount = 0;
        uint32_t reusedCount = 0;

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
            activeIDs.push_back(id);

            auto [it, inserted] = snapshotMap.try_emplace(id);
            RenderableSnapshot& renderable = it->second;

            bool transformDirty = true;
            bool materialDirty = true;
            bool meshDirty = true;
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
                }

                dirty.transformDirty = false;
                dirty.materialDirty  = false;
                dirty.meshDirty      = false;
            }
            else
            {
                dirtyUpdatedCount += 1;
            }

            if (!inserted && !transformDirty && !materialDirty && !meshDirty)
                return;

            renderable.id = id;
            renderable.entity = e;
            renderable.objectSSBOSlot = rc.objectSSBOSlot;
            renderable.visible = true;

            if (inserted || transformDirty)
            {
                renderable.modelMatrix = rc.modelMatrix;
                updateBounds(world, e, renderable);
            }

            bool sortKeyNeedsUpdate = inserted;
            if (inserted || meshDirty)
            {
                renderable.meshID = meshGpu.meshID;
                sortKeyNeedsUpdate = true;
            }

            if (inserted || materialDirty)
            {
                renderable.textureID = matGpu.textureID;
                renderable.alpha = matGpu.alpha;
                renderable.doubleSided = matGpu.doubleSided;
                sortKeyNeedsUpdate = true;
            }

            if (sortKeyNeedsUpdate)
                renderable.sortKey = makeSortKey(renderable);
        });

        activeIDSet.clear();
        activeIDSet.reserve(activeIDs.size());
        for (RenderableID id : activeIDs)
            activeIDSet.insert(id);

        for (auto it = snapshotMap.begin(); it != snapshotMap.end(); )
        {
            if (activeIDSet.find(it->first) == activeIDSet.end())
            {
                it = snapshotMap.erase(it);
                continue;
            }

            ++it;
        }

        snapshot.renderables.clear();
        snapshot.renderables.reserve(snapshotMap.size());
        for (RenderableID id : activeIDs)
            snapshot.renderables.push_back(snapshotMap.at(id));

        const auto end = std::chrono::steady_clock::now();
        lastMetrics.buildCpuMs =
            std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.renderableCount = static_cast<uint32_t>(snapshot.renderables.size());
        lastMetrics.dirtyUpdatedCount = dirtyUpdatedCount;
        lastMetrics.reusedCount = reusedCount;
        return snapshot;
    }

    Metrics getLastMetrics() const
    {
        return lastMetrics;
    }

private:
    RenderExtractionSnapshot snapshot;
    Metrics                  lastMetrics;
    std::unordered_map<RenderableID, RenderableSnapshot> snapshotMap;
    std::vector<RenderableID> activeIDs;
    std::unordered_set<RenderableID> activeIDSet;

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
