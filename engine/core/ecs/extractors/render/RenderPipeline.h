#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "IVisibilityStrategy.h"
#include "RenderCommand.h"
#include "RenderCommandExtractor.hpp"
#include "RenderExtractionSnapshotBuilder.hpp"

class ECSWorld;

class RenderPipeline
{
public:
    // Per-stage timing from the most recent build() call.
    struct PipelineMetrics
    {
        float snapshotBuildCpuMs = 0.0f;
        float visibilityCpuMs    = 0.0f;
    };

    explicit RenderPipeline(std::unique_ptr<IVisibilityStrategy> strategy)
        : visibilityStrategy(std::move(strategy))
    {}

    const std::vector<RenderCommand>& build(ECSWorld& world)
    {
        const auto t0 = std::chrono::steady_clock::now();
        RenderExtractionSnapshot& snapshot = snapshotBuilder.build(world);
        const auto t1 = std::chrono::steady_clock::now();

        if (visibilityStrategy)
            visibilityStrategy->filter(snapshot);
        const auto t2 = std::chrono::steady_clock::now();

        const auto& result = extractor.extract(snapshot);

        lastPipelineMetrics.snapshotBuildCpuMs =
            std::chrono::duration<float, std::milli>(t1 - t0).count();
        lastPipelineMetrics.visibilityCpuMs =
            std::chrono::duration<float, std::milli>(t2 - t1).count();

        return result;
    }

    PipelineMetrics getLastPipelineMetrics() const { return lastPipelineMetrics; }

    RenderCommandExtractor& getExtractor()             { return extractor; }
    const RenderCommandExtractor& getExtractor() const { return extractor; }

    RenderExtractionSnapshotBuilder& getSnapshotBuilder() { return snapshotBuilder; }
    const RenderExtractionSnapshot& getLatestSnapshot() const { return snapshotBuilder.getLatestSnapshot(); }

    void setVisibilityEnabled(bool enabled)
    {
        if (visibilityStrategy)
            visibilityStrategy->setEnabled(enabled);
    }

    bool isVisibilityEnabled() const
    {
        return visibilityStrategy ? visibilityStrategy->isEnabled() : true;
    }

    void setVisibilityFrozen(bool frozen)
    {
        if (!visibilityStrategy)
            return;

        if (visibilityStrategy->isFrozen() != frozen)
            visibilityStrategy->toggleFreeze();
    }

    bool isVisibilityFrozen() const
    {
        return visibilityStrategy ? visibilityStrategy->isFrozen() : false;
    }

private:
    RenderExtractionSnapshotBuilder      snapshotBuilder;
    std::unique_ptr<IVisibilityStrategy> visibilityStrategy;
    RenderCommandExtractor               extractor;
    PipelineMetrics                      lastPipelineMetrics;
};
