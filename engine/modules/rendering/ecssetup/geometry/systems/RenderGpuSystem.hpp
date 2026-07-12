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
        uint32_t skippedUnallocatedSlots;
        uint32_t readyVersionMatches;
        uint32_t snapshotDirtyEnqueues;
        uint32_t queuedTransformDirty;
        uint32_t processedTransformDirty;
        float    cpuTimeMs;
        float    scanAndCompareCpuMs;
        float    modelSyncCpuMs;
    };

    static Metrics getLastMetrics()
    {
        return lastMetrics;
    }

    static void setDetailedMetricsEnabled(bool enabled)
    {
        detailedMetricsEnabled = enabled;
    }

    void update(const EcsControllerContext& ctx) override
    {
        const auto startTime = std::chrono::steady_clock::now();
        uint32_t totalRenderables = 0;
        uint32_t updatedRenderables = 0;
        uint32_t skippedUnallocatedSlots = 0;
        uint32_t readyVersionMatches = 0;
        uint32_t snapshotDirtyEnqueues = 0;
        float scanAndCompareCpuMs = 0.0f;
        float modelSyncCpuMs = 0.0f;
        const bool detailed = detailedMetricsEnabled;

        ctx.world.forEach<WorldTransformComponent, RenderGpuComponent, RenderDirtyComponent>(
            [&](Entity entity,
                WorldTransformComponent& worldTransform,
                RenderGpuComponent& renderGpu,
                RenderDirtyComponent& dirty)
        {
            const auto scanStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                skippedUnallocatedSlots += 1;
                if (detailed)
                {
                    const auto scanEnd = std::chrono::steady_clock::now();
                    scanAndCompareCpuMs +=
                        std::chrono::duration<float, std::milli>(scanEnd - scanStart).count();
                }
                return;
            }

            totalRenderables += 1;
            if (renderGpu.readyToRender
                && renderGpu.uploadedWorldTransformVersion == worldTransform.version)
            {
                readyVersionMatches += 1;
                if (detailed)
                {
                    const auto scanEnd = std::chrono::steady_clock::now();
                    scanAndCompareCpuMs +=
                        std::chrono::duration<float, std::milli>(scanEnd - scanStart).count();
                }
                return;
            }
            if (detailed)
            {
                const auto scanEnd = std::chrono::steady_clock::now();
                scanAndCompareCpuMs +=
                    std::chrono::duration<float, std::milli>(scanEnd - scanStart).count();
            }

            const auto syncStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            renderGpu.modelMatrix = worldTransform.matrix;
            renderGpu.uploadedWorldTransformVersion = worldTransform.version;
            renderGpu.readyToRender = true;
            dirty.transformDirty = true;
            gts::rendering::queueRenderSnapshotDirty(ctx.world, entity);
            snapshotDirtyEnqueues += 1;
            updatedRenderables += 1;
            if (detailed)
            {
                const auto syncEnd = std::chrono::steady_clock::now();
                modelSyncCpuMs +=
                    std::chrono::duration<float, std::milli>(syncEnd - syncStart).count();
            }
        });

        const auto endTime = std::chrono::steady_clock::now();
        lastMetrics.totalRenderables = totalRenderables;
        lastMetrics.updatedRenderables = updatedRenderables;
        lastMetrics.skippedUnallocatedSlots = skippedUnallocatedSlots;
        lastMetrics.readyVersionMatches = readyVersionMatches;
        lastMetrics.snapshotDirtyEnqueues = snapshotDirtyEnqueues;
        lastMetrics.queuedTransformDirty = 0;
        lastMetrics.processedTransformDirty = totalRenderables;
        lastMetrics.cpuTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        lastMetrics.scanAndCompareCpuMs = scanAndCompareCpuMs;
        lastMetrics.modelSyncCpuMs = modelSyncCpuMs;
    }

private:
    using TimePoint = std::chrono::steady_clock::time_point;
    static inline Metrics lastMetrics{};
    static inline bool detailedMetricsEnabled = false;
};
