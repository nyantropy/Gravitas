#pragma once

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
    explicit RenderPipeline(std::unique_ptr<IVisibilityStrategy> strategy)
        : visibilityStrategy(std::move(strategy))
    {}

    const std::vector<RenderCommand>& build(ECSWorld& world)
    {
        RenderExtractionSnapshot& snapshot = snapshotBuilder.build(world);

        if (visibilityStrategy)
            visibilityStrategy->filter(snapshot);

        return extractor.extract(snapshot);
    }

    RenderCommandExtractor& getExtractor()
    {
        return extractor;
    }

    const RenderCommandExtractor& getExtractor() const
    {
        return extractor;
    }

    RenderExtractionSnapshotBuilder& getSnapshotBuilder()
    {
        return snapshotBuilder;
    }

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
};
