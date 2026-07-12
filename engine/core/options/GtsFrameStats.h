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
};
