#pragma once

#include <chrono>

#include "ECSControllerSystem.hpp"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "RenderInvalidationLifecycle.h"
#include "WorldTransformComponent.h"

// Controller system that keeps render GPU object data synchronized with resolved
// scene state. TransformSystem owns hierarchy traversal and writes
// WorldTransformComponent; rendering only consumes its versioned matrix.
class RenderGpuSystem : public ECSControllerSystem
{
public:
    struct Metrics
    {
        uint32_t totalRenderables;
        uint32_t updatedRenderables;
        uint32_t queuedTransformDirty;
        uint32_t processedTransformDirty;
        float    cpuTimeMs;
    };

    static Metrics getLastMetrics()
    {
        return lastMetrics;
    }

    void update(const EcsControllerContext& ctx) override
    {
        const auto startTime = std::chrono::steady_clock::now();
        uint32_t totalRenderables = 0;
        uint32_t updatedRenderables = 0;

        ctx.world.forEach<WorldTransformComponent, RenderGpuComponent, RenderDirtyComponent>(
            [&](Entity entity,
                WorldTransformComponent& worldTransform,
                RenderGpuComponent& renderGpu,
                RenderDirtyComponent& dirty)
        {
            if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                return;

            totalRenderables += 1;
            if (renderGpu.readyToRender
                && renderGpu.uploadedWorldTransformVersion == worldTransform.version)
            {
                return;
            }

            renderGpu.modelMatrix = worldTransform.matrix;
            renderGpu.uploadedWorldTransformVersion = worldTransform.version;
            renderGpu.readyToRender = true;
            dirty.transformDirty = true;
            gts::rendering::queueRenderSnapshotDirty(ctx.world, entity);
            updatedRenderables += 1;
        });

        const auto endTime = std::chrono::steady_clock::now();
        lastMetrics.totalRenderables = totalRenderables;
        lastMetrics.updatedRenderables = updatedRenderables;
        lastMetrics.queuedTransformDirty = 0;
        lastMetrics.processedTransformDirty = totalRenderables;
        lastMetrics.cpuTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }

private:
    static inline Metrics lastMetrics{};
};
