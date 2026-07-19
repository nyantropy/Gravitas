#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "GtsFrameStats.h"

struct ProfileAccumulator
{
    float timeAccum = 0.0f;
    int   frameCount = 0;

    GtsFrameStats sum{};
    GtsFrameStats max{};

    void add(const GtsFrameStats& s, float dt)
    {
        timeAccum += dt;
        ++frameCount;

        sum.fps                    += s.fps;
        sum.frameTimeMs            += s.frameTimeMs;
        sum.renderGpuCpuMs         += s.renderGpuCpuMs;
        sum.renderExtractCpuMs     += s.renderExtractCpuMs;
        sum.renderExtractSortCpuMs += s.renderExtractSortCpuMs;
        sum.uiLayoutCpuMs          += s.uiLayoutCpuMs;
        sum.uiVisualCpuMs          += s.uiVisualCpuMs;
        sum.uiCommandCpuMs         += s.uiCommandCpuMs;
        sum.renderSubmitCpuMs      += s.renderSubmitCpuMs;
        sum.backendFrameCpuMs      += s.backendFrameCpuMs;
        sum.backendFenceWaitCpuMs  += s.backendFenceWaitCpuMs;
        sum.backendAcquireCpuMs    += s.backendAcquireCpuMs;
        sum.backendImageWaitCpuMs  += s.backendImageWaitCpuMs;
        sum.backendObjectWriteCpuMs += s.backendObjectWriteCpuMs;
        sum.backendFenceResetCpuMs += s.backendFenceResetCpuMs;
        sum.backendCmdResetCpuMs   += s.backendCmdResetCpuMs;
        sum.backendCmdRecordCpuMs  += s.backendCmdRecordCpuMs;
        sum.backendQueueSubmitCpuMs += s.backendQueueSubmitCpuMs;
        sum.backendPresentCpuMs    += s.backendPresentCpuMs;
        sum.screenshotScheduleCpuMs += s.screenshotScheduleCpuMs;
        sum.screenshotPollCpuMs     += s.screenshotPollCpuMs;
        sum.screenshotReadbackCpuMs += s.screenshotReadbackCpuMs;
        sum.simulationCpuMs        += s.simulationCpuMs;
        sum.controllerCpuMs        += s.controllerCpuMs;
        sum.snapshotBuildCpuMs     += s.snapshotBuildCpuMs;
        sum.visibilityCpuMs        += s.visibilityCpuMs;
        sum.uiCpuMs                += s.uiCpuMs;
        sum.frameCpuMs             += s.frameCpuMs;
        sum.gpuFrameMs             += s.gpuFrameMs;
        sum.sceneGpuMs             += s.sceneGpuMs;
        sum.particleGpuMs          += s.particleGpuMs;
        sum.uiGpuMs                += s.uiGpuMs;
        sum.gpuTimingSupported     += s.gpuTimingSupported;
        sum.gpuTimingAvailable     += s.gpuTimingAvailable;

        sum.visibleObjects         += s.visibleObjects;
        sum.totalObjects           += s.totalObjects;
        sum.triangleCount          += s.triangleCount;
        sum.drawCalls              += s.drawCalls;
        sum.pipelineBinds          += s.pipelineBinds;
        sum.descriptorBinds        += s.descriptorBinds;
        sum.pipelineSwitches       += s.pipelineSwitches;
        sum.textureSwitches        += s.textureSwitches;
        sum.renderGpuUpdatedCount  += s.renderGpuUpdatedCount;
        sum.renderCommandVisitedCount += s.renderCommandVisitedCount;
        sum.renderCommandTotalCount   += s.renderCommandTotalCount;
        sum.renderCommandUpdatedCount += s.renderCommandUpdatedCount;
        sum.renderCommandSortedCount  += s.renderCommandSortedCount;
        sum.materialQueuedCount       += s.materialQueuedCount;
        sum.materialSynchronizedCount += s.materialSynchronizedCount;
        sum.materialUserInvalidationCount += s.materialUserInvalidationCount;
        sum.materialFallbackSubstitutionCount += s.materialFallbackSubstitutionCount;
        sum.materialReferenceAddCount += s.materialReferenceAddCount;
        sum.materialReferenceRemoveCount += s.materialReferenceRemoveCount;
        sum.materialFullScanCount     += s.materialFullScanCount;
        sum.objectUploadCommandCount  += s.objectUploadCommandCount;
        sum.objectUploadBytes         += s.objectUploadBytes;
        sum.backendObjectWrites       += s.backendObjectWrites;
        sum.backendObjectWritesSkipped += s.backendObjectWritesSkipped;
        sum.backendObjectWriteBytes   += s.backendObjectWriteBytes;
        sum.backendObjectWriteContiguousRuns += s.backendObjectWriteContiguousRuns;
        sum.screenshotRequestedCount += s.screenshotRequestedCount;
        sum.screenshotScheduledCount += s.screenshotScheduledCount;
        sum.screenshotCompletedCount += s.screenshotCompletedCount;
        sum.screenshotSkippedCount += s.screenshotSkippedCount;
        sum.screenshotPendingGpuCount += s.screenshotPendingGpuCount;
        sum.screenshotPendingWriteCount += s.screenshotPendingWriteCount;
        sum.screenshotReadbackBytes += s.screenshotReadbackBytes;
        sum.sceneEntityCount          += s.sceneEntityCount;
        sum.controllerSystemCount     += s.controllerSystemCount;
        sum.simulationSystemCount     += s.simulationSystemCount;
        sum.uiNodeCount               += s.uiNodeCount;
        sum.uiPrimitiveCount          += s.uiPrimitiveCount;
        sum.uiCommandCount            += s.uiCommandCount;
        sum.uiVertexCount             += s.uiVertexCount;
        sum.uiIndexCount              += s.uiIndexCount;
        sum.uiCommandCacheHit         += s.uiCommandCacheHit;
        sum.uiRenderDrawCalls         += s.uiRenderDrawCalls;
        sum.uiSubmittedDrawCalls      += s.uiSubmittedDrawCalls;
        sum.uiSubmittedColoredDrawCalls += s.uiSubmittedColoredDrawCalls;
        sum.uiSubmittedTexturedDrawCalls += s.uiSubmittedTexturedDrawCalls;
        sum.uiSkippedDrawCalls        += s.uiSkippedDrawCalls;
        sum.uiUploadBytes             += s.uiUploadBytes;
        sum.minimapCellCount          += s.minimapCellCount;
        sum.physicsCollisionCount     += s.physicsCollisionCount;
        sum.playerCollisionCount      += s.playerCollisionCount;
        sum.snapshotStaticCount       += s.snapshotStaticCount;
        sum.snapshotDynamicCount      += s.snapshotDynamicCount;
        sum.snapshotDirtyCount        += s.snapshotDirtyCount;
        sum.snapshotReusedCount       += s.snapshotReusedCount;

        max.fps                    = std::max(max.fps, s.fps);
        max.frameTimeMs            = std::max(max.frameTimeMs, s.frameTimeMs);
        max.renderGpuCpuMs         = std::max(max.renderGpuCpuMs, s.renderGpuCpuMs);
        max.renderExtractCpuMs     = std::max(max.renderExtractCpuMs, s.renderExtractCpuMs);
        max.renderExtractSortCpuMs = std::max(max.renderExtractSortCpuMs, s.renderExtractSortCpuMs);
        max.uiLayoutCpuMs          = std::max(max.uiLayoutCpuMs, s.uiLayoutCpuMs);
        max.uiVisualCpuMs          = std::max(max.uiVisualCpuMs, s.uiVisualCpuMs);
        max.uiCommandCpuMs         = std::max(max.uiCommandCpuMs, s.uiCommandCpuMs);
        max.renderSubmitCpuMs      = std::max(max.renderSubmitCpuMs, s.renderSubmitCpuMs);
        max.backendFrameCpuMs      = std::max(max.backendFrameCpuMs, s.backendFrameCpuMs);
        max.backendFenceWaitCpuMs  = std::max(max.backendFenceWaitCpuMs, s.backendFenceWaitCpuMs);
        max.backendAcquireCpuMs    = std::max(max.backendAcquireCpuMs, s.backendAcquireCpuMs);
        max.backendImageWaitCpuMs  = std::max(max.backendImageWaitCpuMs, s.backendImageWaitCpuMs);
        max.backendObjectWriteCpuMs = std::max(max.backendObjectWriteCpuMs, s.backendObjectWriteCpuMs);
        max.backendFenceResetCpuMs = std::max(max.backendFenceResetCpuMs, s.backendFenceResetCpuMs);
        max.backendCmdResetCpuMs   = std::max(max.backendCmdResetCpuMs, s.backendCmdResetCpuMs);
        max.backendCmdRecordCpuMs  = std::max(max.backendCmdRecordCpuMs, s.backendCmdRecordCpuMs);
        max.backendQueueSubmitCpuMs = std::max(max.backendQueueSubmitCpuMs, s.backendQueueSubmitCpuMs);
        max.backendPresentCpuMs    = std::max(max.backendPresentCpuMs, s.backendPresentCpuMs);
        max.screenshotScheduleCpuMs = std::max(max.screenshotScheduleCpuMs, s.screenshotScheduleCpuMs);
        max.screenshotPollCpuMs     = std::max(max.screenshotPollCpuMs, s.screenshotPollCpuMs);
        max.screenshotReadbackCpuMs = std::max(max.screenshotReadbackCpuMs, s.screenshotReadbackCpuMs);
        max.simulationCpuMs        = std::max(max.simulationCpuMs, s.simulationCpuMs);
        max.controllerCpuMs        = std::max(max.controllerCpuMs, s.controllerCpuMs);
        max.snapshotBuildCpuMs     = std::max(max.snapshotBuildCpuMs, s.snapshotBuildCpuMs);
        max.visibilityCpuMs        = std::max(max.visibilityCpuMs, s.visibilityCpuMs);
        max.uiCpuMs                = std::max(max.uiCpuMs, s.uiCpuMs);
        max.frameCpuMs             = std::max(max.frameCpuMs, s.frameCpuMs);
        max.gpuFrameMs             = std::max(max.gpuFrameMs, s.gpuFrameMs);
        max.sceneGpuMs             = std::max(max.sceneGpuMs, s.sceneGpuMs);
        max.particleGpuMs          = std::max(max.particleGpuMs, s.particleGpuMs);
        max.uiGpuMs                = std::max(max.uiGpuMs, s.uiGpuMs);
        max.gpuTimingSupported     = std::max(max.gpuTimingSupported, s.gpuTimingSupported);
        max.gpuTimingAvailable     = std::max(max.gpuTimingAvailable, s.gpuTimingAvailable);

        max.visibleObjects         = s.visibleObjects;
        max.totalObjects           = s.totalObjects;
        max.triangleCount          = s.triangleCount;
        max.drawCalls              = s.drawCalls;
        max.pipelineBinds          = s.pipelineBinds;
        max.descriptorBinds        = s.descriptorBinds;
        max.pipelineSwitches       = s.pipelineSwitches;
        max.textureSwitches        = s.textureSwitches;
        max.renderGpuUpdatedCount  = s.renderGpuUpdatedCount;
        max.renderCommandVisitedCount = s.renderCommandVisitedCount;
        max.renderCommandTotalCount   = s.renderCommandTotalCount;
        max.renderCommandUpdatedCount = s.renderCommandUpdatedCount;
        max.renderCommandSortedCount  = s.renderCommandSortedCount;
        max.materialQueuedCount       = s.materialQueuedCount;
        max.materialSynchronizedCount = s.materialSynchronizedCount;
        max.materialUserInvalidationCount = s.materialUserInvalidationCount;
        max.materialFallbackSubstitutionCount = s.materialFallbackSubstitutionCount;
        max.materialReferenceAddCount = s.materialReferenceAddCount;
        max.materialReferenceRemoveCount = s.materialReferenceRemoveCount;
        max.materialFullScanCount     = s.materialFullScanCount;
        max.objectUploadCommandCount  = s.objectUploadCommandCount;
        max.objectUploadBytes         = s.objectUploadBytes;
        max.backendObjectWrites       = s.backendObjectWrites;
        max.backendObjectWritesSkipped = s.backendObjectWritesSkipped;
        max.backendObjectWriteBytes   = s.backendObjectWriteBytes;
        max.backendObjectWriteContiguousRuns = s.backendObjectWriteContiguousRuns;
        max.screenshotRequestedCount = std::max(max.screenshotRequestedCount, s.screenshotRequestedCount);
        max.screenshotScheduledCount = std::max(max.screenshotScheduledCount, s.screenshotScheduledCount);
        max.screenshotCompletedCount = std::max(max.screenshotCompletedCount, s.screenshotCompletedCount);
        max.screenshotSkippedCount = std::max(max.screenshotSkippedCount, s.screenshotSkippedCount);
        max.screenshotPendingGpuCount = std::max(max.screenshotPendingGpuCount, s.screenshotPendingGpuCount);
        max.screenshotPendingWriteCount = std::max(max.screenshotPendingWriteCount, s.screenshotPendingWriteCount);
        max.screenshotReadbackBytes = std::max(max.screenshotReadbackBytes, s.screenshotReadbackBytes);
        max.sceneEntityCount          = s.sceneEntityCount;
        max.controllerSystemCount     = s.controllerSystemCount;
        max.simulationSystemCount     = s.simulationSystemCount;
        max.uiNodeCount               = s.uiNodeCount;
        max.uiPrimitiveCount          = s.uiPrimitiveCount;
        max.uiCommandCount            = s.uiCommandCount;
        max.uiVertexCount             = s.uiVertexCount;
        max.uiIndexCount              = s.uiIndexCount;
        max.uiCommandCacheHit         = s.uiCommandCacheHit;
        max.uiRenderDrawCalls         = s.uiRenderDrawCalls;
        max.uiSubmittedDrawCalls      = s.uiSubmittedDrawCalls;
        max.uiSubmittedColoredDrawCalls = s.uiSubmittedColoredDrawCalls;
        max.uiSubmittedTexturedDrawCalls = s.uiSubmittedTexturedDrawCalls;
        max.uiSkippedDrawCalls        = s.uiSkippedDrawCalls;
        max.uiUploadBytes             = s.uiUploadBytes;
        max.minimapCellCount          = s.minimapCellCount;
        max.backendPresentMode        = s.backendPresentMode;
        max.physicsCollisionCount     = s.physicsCollisionCount;
        max.playerCollisionCount      = s.playerCollisionCount;
        max.frameIndex                = s.frameIndex;
        max.snapshotStaticCount       = s.snapshotStaticCount;
        max.snapshotDynamicCount      = s.snapshotDynamicCount;
        max.snapshotDirtyCount        = s.snapshotDirtyCount;
        max.snapshotReusedCount       = s.snapshotReusedCount;
    }

    bool shouldPrint() const
    {
        return timeAccum >= 5.0f;
    }

    float averageFrameDeltaMs() const
    {
        if (frameCount <= 0)
            return 0.0f;
        return (timeAccum * 1000.0f) / static_cast<float>(frameCount);
    }

    float windowFps() const
    {
        if (timeAccum <= 0.0f)
            return 0.0f;
        return static_cast<float>(frameCount) / timeAccum;
    }

    void reset()
    {
        timeAccum = 0.0f;
        frameCount = 0;
        sum = {};
        max = {};
    }
};

inline void printProfile(const ProfileAccumulator& acc)
{
    if (acc.frameCount <= 0)
        return;

    const float inv = 1.0f / static_cast<float>(acc.frameCount);

    auto printRow = [](const char* label, float avgMs, float maxMs)
    {
        std::cout << std::left << std::setw(12) << label
                  << std::right << std::setw(6) << std::fixed << std::setprecision(2) << avgMs
                  << " / "
                  << std::setw(6) << maxMs
                  << '\n';
    };
    auto printCounter = [](const char* label, float avgValue, uint32_t lastValue)
    {
        std::cout << std::left << std::setw(22) << label
                  << std::right << std::setw(10) << std::fixed << std::setprecision(1) << avgValue
                  << " avg/frame | last "
                  << lastValue
                  << '\n';
    };

    std::cout << "\n=== FRAME PROFILE (5s window, avg / max) ===\n";
    std::cout << "Samples: " << acc.frameCount
              << " | Window FPS: " << std::fixed << std::setprecision(0) << acc.windowFps()
              << " | Frame dt: " << std::setprecision(2)
              << acc.averageFrameDeltaMs() << " / " << acc.max.frameTimeMs << " ms"
              << " | CPU frame: "
              << (acc.sum.frameCpuMs * inv) << " / " << acc.max.frameCpuMs << " ms";
    if (acc.max.gpuFrameMs > 0.0f)
    {
        std::cout << " | GPU: " << (acc.sum.gpuFrameMs * inv) << " / " << acc.max.gpuFrameMs << " ms";
    }
    std::cout << '\n';

    std::cout << "\n--- CPU ---\n";
    printRow("Sim:",         acc.sum.simulationCpuMs * inv,        acc.max.simulationCpuMs);
    printRow("Controllers:", acc.sum.controllerCpuMs * inv,        acc.max.controllerCpuMs);
    printRow("Snapshot:",    acc.sum.snapshotBuildCpuMs * inv,     acc.max.snapshotBuildCpuMs);
    printRow("Visibility:",  acc.sum.visibilityCpuMs * inv,        acc.max.visibilityCpuMs);
    printRow("Extract:",     acc.sum.renderExtractCpuMs * inv,     acc.max.renderExtractCpuMs);
    printRow("Sort:",        acc.sum.renderExtractSortCpuMs * inv, acc.max.renderExtractSortCpuMs);
    printRow("UI:",          acc.sum.uiCpuMs * inv,                acc.max.uiCpuMs);
    printRow("Submit:",      acc.sum.renderSubmitCpuMs * inv,      acc.max.renderSubmitCpuMs);

    std::cout << "\n--- SUBMIT / BACKEND ---\n";
    printRow("Wrapper:",     acc.sum.renderSubmitCpuMs * inv,       acc.max.renderSubmitCpuMs);
    printRow("Backend:",     acc.sum.backendFrameCpuMs * inv,       acc.max.backendFrameCpuMs);
    printRow("FenceWait:",   acc.sum.backendFenceWaitCpuMs * inv,   acc.max.backendFenceWaitCpuMs);
    printRow("Acquire:",     acc.sum.backendAcquireCpuMs * inv,     acc.max.backendAcquireCpuMs);
    printRow("ImageWait:",   acc.sum.backendImageWaitCpuMs * inv,   acc.max.backendImageWaitCpuMs);
    printRow("ObjWrite:",    acc.sum.backendObjectWriteCpuMs * inv, acc.max.backendObjectWriteCpuMs);
    printRow("FenceReset:",  acc.sum.backendFenceResetCpuMs * inv,  acc.max.backendFenceResetCpuMs);
    printRow("CmdReset:",    acc.sum.backendCmdResetCpuMs * inv,    acc.max.backendCmdResetCpuMs);
    printRow("CmdRecord:",   acc.sum.backendCmdRecordCpuMs * inv,   acc.max.backendCmdRecordCpuMs);
    printRow("QueueSub:",    acc.sum.backendQueueSubmitCpuMs * inv, acc.max.backendQueueSubmitCpuMs);
    printRow("Present:",     acc.sum.backendPresentCpuMs * inv,     acc.max.backendPresentCpuMs);
    printCounter("Draw calls:",       acc.sum.drawCalls * inv,              acc.max.drawCalls);
    printCounter("Pipeline binds:",   acc.sum.pipelineBinds * inv,          acc.max.pipelineBinds);
    printCounter("Descriptor binds:", acc.sum.descriptorBinds * inv,        acc.max.descriptorBinds);
    printCounter("Pipeline switches:", acc.sum.pipelineSwitches * inv,      acc.max.pipelineSwitches);
    printCounter("Texture switches:", acc.sum.textureSwitches * inv,        acc.max.textureSwitches);
    printCounter("Object uploads:",   acc.sum.objectUploadCommandCount * inv, acc.max.objectUploadCommandCount);
    printCounter("Object writes:",    acc.sum.backendObjectWrites * inv,    acc.max.backendObjectWrites);
    printCounter("Object write bytes:", acc.sum.backendObjectWriteBytes * inv, acc.max.backendObjectWriteBytes);
    std::cout << "Present mode: " << acc.max.backendPresentMode << '\n';
    const bool screenshotActive =
        acc.sum.screenshotRequestedCount > 0 ||
        acc.sum.screenshotScheduledCount > 0 ||
        acc.sum.screenshotCompletedCount > 0 ||
        acc.sum.screenshotSkippedCount > 0 ||
        acc.max.screenshotPendingGpuCount > 0 ||
        acc.max.screenshotPendingWriteCount > 0;
    if (screenshotActive)
    {
        std::cout << "\n--- SCREENSHOT ---\n";
        printRow("Schedule:", acc.sum.screenshotScheduleCpuMs * inv, acc.max.screenshotScheduleCpuMs);
        printRow("Poll:",     acc.sum.screenshotPollCpuMs * inv,     acc.max.screenshotPollCpuMs);
        printRow("Readback:", acc.sum.screenshotReadbackCpuMs * inv, acc.max.screenshotReadbackCpuMs);
        std::cout << "Requests/Scheduled/Completed/Skipped: "
                  << acc.sum.screenshotRequestedCount << " / "
                  << acc.sum.screenshotScheduledCount << " / "
                  << acc.sum.screenshotCompletedCount << " / "
                  << acc.sum.screenshotSkippedCount << '\n';
        std::cout << "Pending GPU/Write max: "
                  << acc.max.screenshotPendingGpuCount << " / "
                  << acc.max.screenshotPendingWriteCount
                  << " | Readback bytes: " << acc.sum.screenshotReadbackBytes << '\n';
    }
    if (acc.max.gpuTimingAvailable > 0)
    {
        std::cout << "\n--- GPU ---\n";
        printRow("Frame:",     acc.sum.gpuFrameMs * inv,    acc.max.gpuFrameMs);
        printRow("Scene:",     acc.sum.sceneGpuMs * inv,    acc.max.sceneGpuMs);
        printRow("Particles:", acc.sum.particleGpuMs * inv, acc.max.particleGpuMs);
        printRow("UI:",        acc.sum.uiGpuMs * inv,       acc.max.uiGpuMs);
    }
    std::cout.flush();
}
