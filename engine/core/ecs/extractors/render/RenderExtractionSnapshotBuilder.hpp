#pragma once

#include <chrono>
#include <limits>

#include "BoundsComponent.h"
#include "CameraGpuComponent.h"
#include "CullFlagsComponent.h"
#include "EngineConfig.h"
#include "ECSWorld.hpp"
#include "FrustumCuller.h"
#include "MaterialGpuComponent.h"
#include "MeshGpuComponent.h"
#include "RenderExtractionSnapshot.h"
#include "RenderGpuComponent.h"

class RenderExtractionSnapshotBuilder
{
public:
    struct Metrics
    {
        float    buildCpuMs = 0.0f;
        uint32_t renderableCount = 0;
    };

    const RenderExtractionSnapshot& build(ECSWorld& world)
    {
        const auto start = std::chrono::steady_clock::now();

        snapshot.renderables.clear();
        snapshot.renderables.reserve(EngineConfig::MAX_RENDERABLE_OBJECTS);
        snapshot.cameraViewID = 0;
        snapshot.frustum.fill(glm::vec4(0.0f));

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

            RenderableSnapshot renderable{};
            renderable.id = e.id;
            renderable.entity = e;
            renderable.modelMatrix = rc.modelMatrix;
            renderable.meshID = meshGpu.meshID;
            renderable.textureID = matGpu.textureID;
            renderable.objectSSBOSlot = rc.objectSSBOSlot;
            renderable.alpha = matGpu.alpha;
            renderable.doubleSided = matGpu.doubleSided;
            renderable.sortKey = makeSortKey(renderable);

            bool shouldBuildBounds = world.hasComponent<BoundsComponent>(e);
            if (shouldBuildBounds && world.hasComponent<CullFlagsComponent>(e))
            {
                const auto& flags = world.getComponent<CullFlagsComponent>(e);
                if (flags.neverCull || !flags.cullEnabled)
                    shouldBuildBounds = false;
            }

            if (shouldBuildBounds)
            {
                const auto& bounds = world.getComponent<BoundsComponent>(e);
                renderable.worldBounds = transformBounds(bounds, renderable.modelMatrix);
                renderable.hasBounds = true;
            }

            snapshot.renderables.push_back(renderable);
        });

        const auto end = std::chrono::steady_clock::now();
        lastMetrics.buildCpuMs =
            std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.renderableCount = static_cast<uint32_t>(snapshot.renderables.size());
        return snapshot;
    }

    Metrics getLastMetrics() const
    {
        return lastMetrics;
    }

private:
    RenderExtractionSnapshot snapshot;
    Metrics                  lastMetrics;

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
};
