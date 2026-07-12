#pragma once

#include <algorithm>
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
        uint32_t syncWorkItems = 0;
        uint32_t syncBatches = 0;
        uint32_t syncMaxBatchSize = 0;
        uint32_t syncStateUpdates = 0;
        uint32_t syncSnapshotInvalidations = 0;
        uint32_t ecsLookupsDuringGather = 0;
        uint32_t ecsLookupsDuringPublish = 0;
        float    syncAverageBatchSize = 0.0f;
        float    cpuTimeMs = 0.0f;
        float    scanAndCompareCpuMs = 0.0f;
        float    modelSyncCpuMs = 0.0f;
        float    candidateDiscoveryCpuMs = 0.0f;
        float    inputGatherCpuMs = 0.0f;
        float    validationCpuMs = 0.0f;
        float    versionCheckCpuMs = 0.0f;
        float    batchProcessingCpuMs = 0.0f;
        float    outputMergeCpuMs = 0.0f;
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

        const auto gatherStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        gatherSyncItems(ctx.world, pending, metrics);
        if (detailed)
        {
            const auto gatherEnd = std::chrono::steady_clock::now();
            metrics.inputGatherCpuMs =
                std::chrono::duration<float, std::milli>(gatherEnd - gatherStart).count();
        }

        const auto processStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        buildBatchRanges(metrics);
        processBatches();
        if (detailed)
        {
            const auto processEnd = std::chrono::steady_clock::now();
            metrics.batchProcessingCpuMs =
                std::chrono::duration<float, std::milli>(processEnd - processStart).count();
        }

        const auto mergeStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        publishBatchOutput(ctx.world, metrics);
        if (detailed)
        {
            const auto mergeEnd = std::chrono::steady_clock::now();
            metrics.outputMergeCpuMs =
                std::chrono::duration<float, std::milli>(mergeEnd - mergeStart).count();
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
            metrics.candidateDiscoveryCpuMs + metrics.inputGatherCpuMs +
            metrics.validationCpuMs + metrics.versionCheckCpuMs;
        metrics.modelSyncCpuMs =
            metrics.batchProcessingCpuMs + metrics.outputMergeCpuMs +
            metrics.matrixCopyCpuMs + metrics.objectUploadEnqueueCpuMs + metrics.snapshotInvalidationCpuMs;
        metrics.cpuTimeMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        lastMetricsStorage() = metrics;
    }

private:
    using TimePoint = std::chrono::steady_clock::time_point;
    static constexpr uint32_t TargetBatchSize = 1024;

    struct RenderTransformSyncItem
    {
        Entity       entity = INVALID_ENTITY;
        uint32_t     worldVersion = 0;
        ssbo_id_type objectSlot = RENDERABLE_SLOT_UNALLOCATED;
        glm::mat4    modelMatrix = glm::mat4(1.0f);
    };

    struct RenderStateUpdate
    {
        Entity    entity = INVALID_ENTITY;
        uint32_t  worldVersion = 0;
        glm::mat4 modelMatrix = glm::mat4(1.0f);
    };

    struct BatchRange
    {
        uint32_t begin = 0;
        uint32_t end = 0;
    };

    struct RenderSyncBatchOutput
    {
        std::vector<RenderStateUpdate> stateUpdates;
        std::vector<Entity>            snapshotInvalidations;

        void clear()
        {
            stateUpdates.clear();
            snapshotInvalidations.clear();
        }
    };

    std::vector<RenderTransformSyncItem> syncItems;
    std::vector<BatchRange>              batchRanges;
    RenderSyncBatchOutput                batchOutput;

    void gatherSyncItems(ECSWorld& world,
                         const std::vector<entity_id_type>& pending,
                         Metrics& metrics)
    {
        syncItems.clear();
        syncItems.reserve(pending.size());

        const bool detailed = detailedMetricsEnabled;
        for (entity_id_type entityId : pending)
        {
            Entity entity{entityId};

            const auto validationStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            metrics.ecsLookupsDuringGather += 3;
            const bool hasRequiredComponents =
                world.hasComponent<WorldTransformComponent>(entity) &&
                world.hasComponent<RenderGpuComponent>(entity) &&
                world.hasComponent<RenderDirtyComponent>(entity);
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

            WorldTransformComponent& worldTransform =
                world.getComponent<WorldTransformComponent>(entity);
            RenderGpuComponent& renderGpu =
                world.getComponent<RenderGpuComponent>(entity);

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

            syncItems.push_back({
                entity,
                worldTransform.version,
                renderGpu.objectSSBOSlot,
                worldTransform.matrix
            });
        }

        metrics.syncWorkItems = static_cast<uint32_t>(syncItems.size());
    }

    void buildBatchRanges(Metrics& metrics)
    {
        batchRanges.clear();
        if (syncItems.empty())
            return;

        for (size_t begin = 0; begin < syncItems.size(); begin += TargetBatchSize)
        {
            const size_t end = std::min(begin + TargetBatchSize, syncItems.size());
            batchRanges.push_back({static_cast<uint32_t>(begin), static_cast<uint32_t>(end)});
            metrics.syncMaxBatchSize = std::max(
                metrics.syncMaxBatchSize,
                static_cast<uint32_t>(end - begin));
        }

        metrics.syncBatches = static_cast<uint32_t>(batchRanges.size());
        metrics.syncAverageBatchSize = metrics.syncBatches == 0
            ? 0.0f
            : static_cast<float>(syncItems.size()) / static_cast<float>(metrics.syncBatches);
    }

    void processBatches()
    {
        batchOutput.clear();
        batchOutput.stateUpdates.reserve(syncItems.size());
        batchOutput.snapshotInvalidations.reserve(syncItems.size());

        for (const BatchRange& range : batchRanges)
        {
            for (uint32_t index = range.begin; index < range.end; ++index)
            {
                const RenderTransformSyncItem& item = syncItems[index];
                batchOutput.stateUpdates.push_back({
                    item.entity,
                    item.worldVersion,
                    item.modelMatrix
                });
                batchOutput.snapshotInvalidations.push_back(item.entity);
            }
        }
    }

    void publishBatchOutput(ECSWorld& world, Metrics& metrics)
    {
        const bool detailed = detailedMetricsEnabled;
        const size_t updateCount = batchOutput.stateUpdates.size();
        for (size_t index = 0; index < updateCount; ++index)
        {
            const RenderStateUpdate& update = batchOutput.stateUpdates[index];

            metrics.ecsLookupsDuringPublish += 2;
            if (!world.hasComponent<RenderGpuComponent>(update.entity) ||
                !world.hasComponent<RenderDirtyComponent>(update.entity))
            {
                metrics.staleQueueEntriesSkipped += 1;
                continue;
            }

            RenderGpuComponent& renderGpu = world.getComponent<RenderGpuComponent>(update.entity);
            RenderDirtyComponent& dirty = world.getComponent<RenderDirtyComponent>(update.entity);

            const auto copyStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            renderGpu.modelMatrix = update.modelMatrix;
            renderGpu.uploadedWorldTransformVersion = update.worldVersion;
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
            gts::rendering::queueRenderSnapshotDirty(world, batchOutput.snapshotInvalidations[index]);
            metrics.snapshotDirtyEnqueues += 1;
            metrics.syncSnapshotInvalidations += 1;
            if (detailed)
            {
                const auto invalidationEnd = std::chrono::steady_clock::now();
                metrics.snapshotInvalidationCpuMs +=
                    std::chrono::duration<float, std::milli>(
                        invalidationEnd - invalidationStart).count();
            }

            metrics.updatedRenderables += 1;
            metrics.changedTransformsSynchronized += 1;
            metrics.syncStateUpdates += 1;
        }
    }

    static Metrics& lastMetricsStorage()
    {
        static Metrics metrics{};
        return metrics;
    }

    static inline bool detailedMetricsEnabled = false;
};
