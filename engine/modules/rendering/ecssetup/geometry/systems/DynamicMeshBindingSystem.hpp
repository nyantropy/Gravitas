#pragma once

#include <chrono>
#include <cstdint>

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "DynamicMeshComponent.h"
#include "MeshBindingLifecycle.h"

class DynamicMeshBindingSystem : public ECSControllerSystem
{
public:
    struct Metrics
    {
        uint32_t queuedMeshes = 0;
        uint32_t candidateEntities = 0;
        uint32_t missingComponents = 0;
        uint32_t unchangedMeshesSkipped = 0;
        uint32_t failedVersionsSkipped = 0;
        uint32_t changedMeshesProcessed = 0;
        uint32_t invalidMeshes = 0;
        uint32_t gpuMeshAllocations = 0;
        uint32_t gpuMeshReallocations = 0;
        uint32_t inPlaceGpuUpdates = 0;
        uint32_t meshResourcesRecreated = 0;
        uint32_t renderablesInvalidated = 0;
        uint32_t boundsRecomputed = 0;
        uint32_t normalsGenerated = 0;
        uint32_t tangentsGenerated = 0;
        uint64_t verticesProcessed = 0;
        uint64_t indicesProcessed = 0;
        uint64_t cpuBytesCopied = 0;
        uint64_t vertexBytesUploaded = 0;
        uint64_t indexBytesUploaded = 0;
        uint64_t vertexBytesAllocated = 0;
        uint64_t indexBytesAllocated = 0;
        float cpuTimeMs = 0.0f;
        float candidateDiscoveryCpuMs = 0.0f;
        float versionCheckCpuMs = 0.0f;
        float validationCpuMs = 0.0f;
        float geometryPreparationCpuMs = 0.0f;
        float normalTangentCpuMs = 0.0f;
        float boundsCpuMs = 0.0f;
        float temporaryCopyCpuMs = 0.0f;
        float resourceAllocationCpuMs = 0.0f;
        float vertexUploadCpuMs = 0.0f;
        float indexUploadCpuMs = 0.0f;
        float resourceUploadCpuMs = 0.0f;
        float publicationCpuMs = 0.0f;
        float invalidationCpuMs = 0.0f;
        float cleanupCpuMs = 0.0f;
    };

    static void setDetailedMetricsEnabled(bool enabled)
    {
        detailedMetricsEnabled = enabled;
    }

    static Metrics getLastMetrics()
    {
        return lastMetricsStorage();
    }

    void update(const EcsControllerContext& ctx) override
    {
        const auto systemStart = std::chrono::steady_clock::now();
        Metrics metrics;

        const auto discoveryStart = std::chrono::steady_clock::now();
        auto pendingMeshes = gts::rendering::takeDynamicMeshRefreshes(ctx.world);
        metrics.candidateDiscoveryCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - discoveryStart).count();
        metrics.queuedMeshes = static_cast<uint32_t>(pendingMeshes.size());
        if (pendingMeshes.empty())
        {
            metrics.cpuTimeMs =
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - systemStart).count();
            lastMetricsStorage() = metrics;
            return;
        }

        auto& commands = ctx.world.commands();
        if (ctx.resources != nullptr)
        {
            ctx.resources->takeProceduralMeshUploadMetrics();
            ctx.resources->beginProceduralMeshUpdateBatch(metrics.queuedMeshes);
        }

        for (entity_id_type entityId : pendingMeshes)
        {
            metrics.candidateEntities += 1;
            Entity entity{entityId};
            if (!ctx.world.hasComponent<DynamicMeshComponent>(entity))
            {
                metrics.missingComponents += 1;
                continue;
            }

            const gts::rendering::DynamicMeshSyncResult result =
                gts::rendering::syncDynamicMeshBinding(ctx.world, entity, ctx.resources, commands);
            metrics.versionCheckCpuMs += result.versionCheckCpuMs;
            metrics.validationCpuMs += result.validationCpuMs;
            metrics.boundsCpuMs += result.boundsCpuMs;
            metrics.resourceUploadCpuMs += result.uploadCpuMs;
            metrics.publicationCpuMs += result.publicationCpuMs;
            metrics.invalidationCpuMs += result.invalidationCpuMs;
            metrics.cleanupCpuMs += result.cleanupCpuMs;
            if (result.skippedUnchanged)
                metrics.unchangedMeshesSkipped += 1;
            if (result.skippedFailedVersion)
                metrics.failedVersionsSkipped += 1;
            if (result.invalid)
                metrics.invalidMeshes += 1;
            if (result.processed)
            {
                metrics.changedMeshesProcessed += 1;
                metrics.verticesProcessed += result.vertexCount;
                metrics.indicesProcessed += result.indexCount;
                metrics.cpuBytesCopied += static_cast<uint64_t>(result.vertexBytes) + result.indexBytes;
                if (result.uploaded)
                {
                    metrics.vertexBytesUploaded += result.vertexBytes;
                    metrics.indexBytesUploaded += result.indexBytes;
                }
            }
            if (result.allocated)
                metrics.gpuMeshAllocations += 1;
            if (result.reallocated)
                metrics.gpuMeshReallocations += 1;
            if (result.inPlaceUpdate)
                metrics.inPlaceGpuUpdates += 1;
            if (result.renderableInvalidated)
                metrics.renderablesInvalidated += 1;
            if (result.boundsRecomputed)
                metrics.boundsRecomputed += 1;
        }

        if (ctx.resources != nullptr)
        {
            ctx.resources->endProceduralMeshUpdateBatch();
            const ProceduralMeshUploadMetrics uploads = ctx.resources->takeProceduralMeshUploadMetrics();
            if (uploads.uploadCalls != 0)
            {
                metrics.gpuMeshAllocations = uploads.gpuAllocations;
                metrics.gpuMeshReallocations = uploads.gpuReallocations;
                metrics.inPlaceGpuUpdates = uploads.inPlaceUpdates;
                metrics.meshResourcesRecreated = uploads.meshResourcesRecreated;
                metrics.normalsGenerated = uploads.generatedNormals;
                metrics.tangentsGenerated = uploads.generatedTangents;
                metrics.vertexBytesUploaded = uploads.vertexBytesUploaded;
                metrics.indexBytesUploaded = uploads.indexBytesUploaded;
                metrics.vertexBytesAllocated = uploads.vertexBytesAllocated;
                metrics.indexBytesAllocated = uploads.indexBytesAllocated;
                metrics.geometryPreparationCpuMs = uploads.geometryPreparationCpuMs;
                metrics.temporaryCopyCpuMs = uploads.temporaryCopyCpuMs;
                metrics.resourceAllocationCpuMs = uploads.resourceAllocationCpuMs;
                metrics.vertexUploadCpuMs = uploads.vertexUploadCpuMs;
                metrics.indexUploadCpuMs = uploads.indexUploadCpuMs;
                metrics.cleanupCpuMs += uploads.cleanupCpuMs;
                if (uploads.cpuBytesCopied != 0)
                    metrics.cpuBytesCopied = uploads.cpuBytesCopied;
            }
        }

        if (!detailedMetricsEnabled)
        {
            metrics.candidateDiscoveryCpuMs = 0.0f;
            metrics.versionCheckCpuMs = 0.0f;
            metrics.validationCpuMs = 0.0f;
            metrics.geometryPreparationCpuMs = 0.0f;
            metrics.normalTangentCpuMs = 0.0f;
            metrics.boundsCpuMs = 0.0f;
            metrics.temporaryCopyCpuMs = 0.0f;
            metrics.resourceAllocationCpuMs = 0.0f;
            metrics.vertexUploadCpuMs = 0.0f;
            metrics.indexUploadCpuMs = 0.0f;
            metrics.resourceUploadCpuMs = 0.0f;
            metrics.publicationCpuMs = 0.0f;
            metrics.invalidationCpuMs = 0.0f;
            metrics.cleanupCpuMs = 0.0f;
        }

        metrics.cpuTimeMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - systemStart).count();
        lastMetricsStorage() = metrics;
    }

private:
    static Metrics& lastMetricsStorage()
    {
        static Metrics metrics{};
        return metrics;
    }

    static inline bool detailedMetricsEnabled = false;
};
