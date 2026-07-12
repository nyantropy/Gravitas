#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "WorldTransformComponent.h"

// Controller system that keeps render GPU object data synchronized with resolved
// scene state. TransformSystem owns hierarchy traversal and writes
// WorldTransformComponent; rendering consumes its versioned matrix through the
// scene-local render-transform sync queue.
class RenderGpuSystem : public ECSControllerSystem
{
public:
    struct Metrics
    {
        uint32_t totalRenderables = 0;
        uint32_t updatedRenderables = 0;
        uint32_t skippedUnallocatedSlots = 0;
        uint32_t readyVersionMatches = 0;
        uint32_t snapshotDirtyEnqueues = 0;
        uint32_t queuedTransformDirty = 0;
        uint32_t processedTransformDirty = 0;
        uint32_t fullScanRenderablesVisited = 0;
        uint32_t queuedRenderables = 0;
        uint32_t queuedEntriesDrained = 0;
        uint32_t staleQueueEntriesSkipped = 0;
        uint32_t missingComponentEntriesSkipped = 0;
        uint32_t unchangedVersionsSkipped = 0;
        uint32_t changedTransformsSynchronized = 0;
        uint32_t modelMatricesCopied = 0;
        uint32_t objectUploadRequests = 0;
        uint32_t duplicateQueueAttempts = 0;
        uint32_t queueDeduplications = 0;
        uint32_t objectSlotLookupFailures = 0;
        float    cpuTimeMs = 0.0f;
        float    scanAndCompareCpuMs = 0.0f;
        float    modelSyncCpuMs = 0.0f;
        float    candidateDiscoveryCpuMs = 0.0f;
        float    validationCpuMs = 0.0f;
        float    versionCheckCpuMs = 0.0f;
        float    matrixCopyCpuMs = 0.0f;
        float    objectUploadEnqueueCpuMs = 0.0f;
        float    snapshotInvalidationCpuMs = 0.0f;
        float    queueCleanupCpuMs = 0.0f;
    };

    static Metrics getLastMetrics()
    {
        return lastMetricsStorage();
    }

    static void setDetailedMetricsEnabled(bool enabled)
    {
        detailedMetricsEnabled = enabled;
    }

    void update(const EcsControllerContext& ctx) override
    {
        const auto startTime = std::chrono::steady_clock::now();
        const bool detailed = detailedMetricsEnabled;
        Metrics metrics;

        const auto discoveryStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        const gts::rendering::RenderTransformSyncQueueStats queueStats =
            gts::rendering::takeRenderTransformSyncQueueStats(ctx.world);
        std::vector<entity_id_type> pending = gts::rendering::takeRenderTransformSyncs(ctx.world);
        if (detailed)
        {
            const auto discoveryEnd = std::chrono::steady_clock::now();
            metrics.candidateDiscoveryCpuMs =
                std::chrono::duration<float, std::milli>(discoveryEnd - discoveryStart).count();
        }

        metrics.queuedTransformDirty = queueStats.queueAttempts;
        metrics.queuedRenderables = static_cast<uint32_t>(pending.size());
        metrics.queuedEntriesDrained = static_cast<uint32_t>(pending.size());
        metrics.duplicateQueueAttempts = queueStats.queueDeduplications;
        metrics.queueDeduplications = queueStats.queueDeduplications;
        metrics.processedTransformDirty = metrics.queuedEntriesDrained;

        for (entity_id_type entityId : pending)
        {
            Entity entity{entityId};

            const auto validationStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            const bool hasRequiredComponents =
                ctx.world.hasComponent<WorldTransformComponent>(entity) &&
                ctx.world.hasComponent<RenderGpuComponent>(entity) &&
                ctx.world.hasComponent<RenderDirtyComponent>(entity);
            if (detailed)
            {
                const auto validationEnd = std::chrono::steady_clock::now();
                metrics.validationCpuMs +=
                    std::chrono::duration<float, std::milli>(validationEnd - validationStart).count();
            }

            if (!hasRequiredComponents)
            {
                metrics.missingComponentEntriesSkipped += 1;
                metrics.staleQueueEntriesSkipped += 1;
                continue;
            }

            WorldTransformComponent& worldTransform = ctx.world.getComponent<WorldTransformComponent>(entity);
            RenderGpuComponent& renderGpu = ctx.world.getComponent<RenderGpuComponent>(entity);
            RenderDirtyComponent& dirty = ctx.world.getComponent<RenderDirtyComponent>(entity);

            metrics.totalRenderables += 1;
            if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                metrics.skippedUnallocatedSlots += 1;
                metrics.objectSlotLookupFailures += 1;
                metrics.staleQueueEntriesSkipped += 1;
                continue;
            }

            const auto versionStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            const bool versionAlreadyUploaded =
                renderGpu.readyToRender &&
                renderGpu.uploadedWorldTransformVersion == worldTransform.version;
            if (detailed)
            {
                const auto versionEnd = std::chrono::steady_clock::now();
                metrics.versionCheckCpuMs +=
                    std::chrono::duration<float, std::milli>(versionEnd - versionStart).count();
            }

            if (versionAlreadyUploaded)
            {
                metrics.readyVersionMatches += 1;
                metrics.unchangedVersionsSkipped += 1;
                continue;
            }

            const auto copyStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            renderGpu.modelMatrix = worldTransform.matrix;
            renderGpu.uploadedWorldTransformVersion = worldTransform.version;
            renderGpu.readyToRender = true;
            metrics.modelMatricesCopied += 1;
            if (detailed)
            {
                const auto copyEnd = std::chrono::steady_clock::now();
                metrics.matrixCopyCpuMs +=
                    std::chrono::duration<float, std::milli>(copyEnd - copyStart).count();
            }

            const auto uploadStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            dirty.transformDirty = true;
            metrics.objectUploadRequests += 1;
            if (detailed)
            {
                const auto uploadEnd = std::chrono::steady_clock::now();
                metrics.objectUploadEnqueueCpuMs +=
                    std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();
            }

            const auto invalidationStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            gts::rendering::queueRenderSnapshotDirty(ctx.world, entity);
            metrics.snapshotDirtyEnqueues += 1;
            if (detailed)
            {
                const auto invalidationEnd = std::chrono::steady_clock::now();
                metrics.snapshotInvalidationCpuMs +=
                    std::chrono::duration<float, std::milli>(
                        invalidationEnd - invalidationStart).count();
            }

            metrics.updatedRenderables += 1;
            metrics.changedTransformsSynchronized += 1;
        }

        const auto cleanupStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        pending.clear();
        if (detailed)
        {
            const auto cleanupEnd = std::chrono::steady_clock::now();
            metrics.queueCleanupCpuMs =
                std::chrono::duration<float, std::milli>(cleanupEnd - cleanupStart).count();
        }

        metrics.scanAndCompareCpuMs =
            metrics.candidateDiscoveryCpuMs + metrics.validationCpuMs + metrics.versionCheckCpuMs;
        metrics.modelSyncCpuMs =
            metrics.matrixCopyCpuMs + metrics.objectUploadEnqueueCpuMs + metrics.snapshotInvalidationCpuMs;
        metrics.cpuTimeMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        lastMetricsStorage() = metrics;
    }

private:
    using TimePoint = std::chrono::steady_clock::time_point;

    static Metrics& lastMetricsStorage()
    {
        static Metrics metrics{};
        return metrics;
    }

    static inline bool detailedMetricsEnabled = false;
};
