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
        sum.simulationCpuMs        += s.simulationCpuMs;
        sum.controllerCpuMs        += s.controllerCpuMs;
        sum.snapshotBuildCpuMs     += s.snapshotBuildCpuMs;
        sum.visibilityCpuMs        += s.visibilityCpuMs;
        sum.uiCpuMs                += s.uiCpuMs;
        sum.frameCpuMs             += s.frameCpuMs;
        sum.gpuFrameMs             += s.gpuFrameMs;

        sum.visibleObjects         += s.visibleObjects;
        sum.totalObjects           += s.totalObjects;
        sum.triangleCount          += s.triangleCount;
        sum.drawCalls              += s.drawCalls;
        sum.pipelineSwitches       += s.pipelineSwitches;
        sum.textureSwitches        += s.textureSwitches;
        sum.renderGpuUpdatedCount  += s.renderGpuUpdatedCount;
        sum.renderCommandVisitedCount += s.renderCommandVisitedCount;
        sum.renderCommandTotalCount   += s.renderCommandTotalCount;
        sum.renderCommandUpdatedCount += s.renderCommandUpdatedCount;
        sum.renderCommandSortedCount  += s.renderCommandSortedCount;
        sum.backendObjectWrites       += s.backendObjectWrites;
        sum.backendObjectWritesSkipped += s.backendObjectWritesSkipped;
        sum.sceneEntityCount          += s.sceneEntityCount;
        sum.controllerSystemCount     += s.controllerSystemCount;
        sum.simulationSystemCount     += s.simulationSystemCount;
        sum.uiNodeCount               += s.uiNodeCount;
        sum.uiPrimitiveCount          += s.uiPrimitiveCount;
        sum.uiCommandCount            += s.uiCommandCount;
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
        max.simulationCpuMs        = std::max(max.simulationCpuMs, s.simulationCpuMs);
        max.controllerCpuMs        = std::max(max.controllerCpuMs, s.controllerCpuMs);
        max.snapshotBuildCpuMs     = std::max(max.snapshotBuildCpuMs, s.snapshotBuildCpuMs);
        max.visibilityCpuMs        = std::max(max.visibilityCpuMs, s.visibilityCpuMs);
        max.uiCpuMs                = std::max(max.uiCpuMs, s.uiCpuMs);
        max.frameCpuMs             = std::max(max.frameCpuMs, s.frameCpuMs);
        max.gpuFrameMs             = std::max(max.gpuFrameMs, s.gpuFrameMs);

        max.visibleObjects         = s.visibleObjects;
        max.totalObjects           = s.totalObjects;
        max.triangleCount          = s.triangleCount;
        max.drawCalls              = s.drawCalls;
        max.pipelineSwitches       = s.pipelineSwitches;
        max.textureSwitches        = s.textureSwitches;
        max.renderGpuUpdatedCount  = s.renderGpuUpdatedCount;
        max.renderCommandVisitedCount = s.renderCommandVisitedCount;
        max.renderCommandTotalCount   = s.renderCommandTotalCount;
        max.renderCommandUpdatedCount = s.renderCommandUpdatedCount;
        max.renderCommandSortedCount  = s.renderCommandSortedCount;
        max.backendObjectWrites       = s.backendObjectWrites;
        max.backendObjectWritesSkipped = s.backendObjectWritesSkipped;
        max.sceneEntityCount          = s.sceneEntityCount;
        max.controllerSystemCount     = s.controllerSystemCount;
        max.simulationSystemCount     = s.simulationSystemCount;
        max.uiNodeCount               = s.uiNodeCount;
        max.uiPrimitiveCount          = s.uiPrimitiveCount;
        max.uiCommandCount            = s.uiCommandCount;
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

    std::cout << "\n=== FRAME PROFILE (5s avg) ===\n";
    std::cout << "FPS: " << std::fixed << std::setprecision(0) << (acc.sum.fps * inv)
              << " | Frame: " << std::setprecision(2) << (acc.sum.frameTimeMs * inv) << " ms";
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
    std::cout.flush();
}
