#pragma once

#include <cstdint>

// Plain data struct written by multiple engine systems each frame and read
// by GtsDebugOverlay.  No ownership — passed by value via the frame graph
// data blackboard.
struct GtsFrameStats
{
    float    fps                    = 0.0f;
    float    frameTimeMs            = 0.0f;
    float    renderGpuCpuMs         = 0.0f;
    float    renderExtractCpuMs     = 0.0f;
    float    renderExtractSortCpuMs = 0.0f;
    float    uiLayoutCpuMs          = 0.0f;
    float    uiVisualCpuMs          = 0.0f;
    float    uiCommandCpuMs         = 0.0f;
    float    renderSubmitCpuMs      = 0.0f;
    float    backendFrameCpuMs      = 0.0f;
    float    backendFenceWaitCpuMs  = 0.0f;
    float    backendAcquireCpuMs    = 0.0f;
    float    backendImageWaitCpuMs  = 0.0f;
    float    backendObjectWriteCpuMs = 0.0f;
    float    backendFenceResetCpuMs = 0.0f;
    float    backendCmdResetCpuMs   = 0.0f;
    float    backendCmdRecordCpuMs  = 0.0f;
    float    backendQueueSubmitCpuMs = 0.0f;
    float    backendPresentCpuMs    = 0.0f;
    uint32_t visibleObjects         = 0;
    uint32_t totalObjects           = 0;
    uint32_t triangleCount          = 0;
    uint32_t drawCalls              = 0;
    uint32_t pipelineBinds          = 0;
    uint32_t descriptorBinds        = 0;
    uint32_t pipelineSwitches       = 0;
    uint32_t textureSwitches        = 0;
    uint32_t particleCount          = 0;
    uint32_t particleDrawCalls      = 0;
    uint32_t renderGpuUpdatedCount  = 0;
    uint32_t renderGpuTotalCount    = 0;
    uint32_t renderGpuVersionMatchCount = 0;
    uint32_t renderGpuUnallocatedSlotCount = 0;
    uint32_t renderGpuSnapshotDirtyEnqueueCount = 0;
    uint32_t dynamicMeshQueuedCount = 0;
    uint32_t dynamicMeshCandidateCount = 0;
    uint32_t dynamicMeshUnchangedSkippedCount = 0;
    uint32_t dynamicMeshFailedVersionSkippedCount = 0;
    uint32_t dynamicMeshChangedCount = 0;
    uint32_t dynamicMeshInvalidCount = 0;
    uint32_t dynamicMeshGpuAllocationCount = 0;
    uint32_t dynamicMeshGpuReallocationCount = 0;
    uint32_t dynamicMeshInPlaceUpdateCount = 0;
    uint32_t dynamicMeshResourceRecreatedCount = 0;
    uint32_t dynamicMeshInvalidationCount = 0;
    uint32_t dynamicMeshBoundsRecomputedCount = 0;
    uint32_t dynamicMeshNormalsGeneratedCount = 0;
    uint32_t dynamicMeshTangentsGeneratedCount = 0;
    uint64_t dynamicMeshVerticesProcessed = 0;
    uint64_t dynamicMeshIndicesProcessed = 0;
    uint64_t dynamicMeshCpuBytesCopied = 0;
    uint64_t dynamicMeshVertexBytesUploaded = 0;
    uint64_t dynamicMeshIndexBytesUploaded = 0;
    uint64_t dynamicMeshVertexBytesAllocated = 0;
    uint64_t dynamicMeshIndexBytesAllocated = 0;
    float    dynamicMeshCandidateDiscoveryCpuMs = 0.0f;
    float    dynamicMeshVersionCheckCpuMs = 0.0f;
    float    dynamicMeshValidationCpuMs = 0.0f;
    float    dynamicMeshBoundsCpuMs = 0.0f;
    float    dynamicMeshGeometryPreparationCpuMs = 0.0f;
    float    dynamicMeshTemporaryCopyCpuMs = 0.0f;
    float    dynamicMeshResourceAllocationCpuMs = 0.0f;
    float    dynamicMeshVertexUploadCpuMs = 0.0f;
    float    dynamicMeshIndexUploadCpuMs = 0.0f;
    float    dynamicMeshPublicationCpuMs = 0.0f;
    float    dynamicMeshInvalidationCpuMs = 0.0f;
    float    dynamicMeshCleanupCpuMs = 0.0f;
    uint32_t renderCommandVisitedCount = 0;
    uint32_t renderCommandTotalCount   = 0;
    uint32_t renderCommandUpdatedCount = 0;
    uint32_t renderCommandSortedCount  = 0;
    uint32_t materialQueuedCount       = 0;
    uint32_t materialSynchronizedCount = 0;
    uint32_t materialUserInvalidationCount = 0;
    uint32_t materialFallbackSubstitutionCount = 0;
    uint32_t materialReferenceAddCount = 0;
    uint32_t materialReferenceRemoveCount = 0;
    uint32_t materialFullScanCount     = 0;
    uint32_t backendObjectWrites       = 0;
    uint32_t backendObjectWritesSkipped = 0;
    uint32_t objectUploadCommandCount  = 0;
    uint32_t objectUploadBytes         = 0;
    uint32_t backendObjectWriteBytes   = 0;
    uint32_t backendObjectWriteContiguousRuns = 0;
    uint32_t sceneEntityCount          = 0;
    uint32_t controllerSystemCount  = 0;
    uint32_t simulationSystemCount  = 0;
    uint32_t uiNodeCount            = 0;
    uint32_t uiPrimitiveCount       = 0;
    uint32_t uiCommandCount         = 0;
    uint32_t uiVertexCount          = 0;
    uint32_t uiIndexCount           = 0;
    uint32_t uiCommandCacheHit      = 0;
    uint32_t uiRenderDrawCalls      = 0;
    uint32_t uiUploadBytes          = 0;
    uint32_t minimapCellCount       = 0;
    uint32_t backendPresentMode     = 0;
    uint32_t physicsCollisionCount  = 0;
    uint32_t playerCollisionCount   = 0;

    // Per-stage CPU timings — populated by GravitasEngine each frame
    float    simulationCpuMs    = 0.0f;
    float    controllerCpuMs    = 0.0f;
    float    snapshotBuildCpuMs = 0.0f;
    float    visibilityCpuMs    = 0.0f;
    float    uiCpuMs            = 0.0f;   // layout + visual + command total
    float    frameCpuMs         = 0.0f;   // total main-loop iteration time
    float    transformResolveCpuMs = 0.0f;
    float    transformQueueChildrenCpuMs = 0.0f;
    float    transformResolveWorldCpuMs = 0.0f;
    float    transformPublishWorldCpuMs = 0.0f;
    float    renderGpuScanCompareCpuMs = 0.0f;
    float    renderGpuModelSyncCpuMs = 0.0f;
    float    gpuFrameMs         = 0.0f;
    float    sceneGpuMs         = 0.0f;
    float    particleGpuMs      = 0.0f;
    float    uiGpuMs            = 0.0f;
    uint32_t gpuTimingSupported = 0;
    uint32_t gpuTimingAvailable = 0;

    // Frame counter
    uint64_t frameIndex         = 0;

    // Snapshot builder breakdown (from RenderExtractionSnapshotBuilder::Metrics)
    uint32_t snapshotStaticCount  = 0;
    uint32_t snapshotDynamicCount = 0;
    uint32_t snapshotDirtyCount   = 0;
    uint32_t snapshotReusedCount  = 0;
    uint32_t transformQueuedCount = 0;
    uint32_t transformProcessedCount = 0;
    uint32_t transformUpdatedCount = 0;
};
