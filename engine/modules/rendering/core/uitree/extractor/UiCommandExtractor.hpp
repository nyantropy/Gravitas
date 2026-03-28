#pragma once

#include "UiTree.h"
#include "UiCommand.h"

// Adapter over UiTree — produces a UiCommandBuffer each frame.
// No ECS dependency. No resource provider dependency.
// The tree owns all resource resolution internally.
class UiCommandExtractor
{
public:
    explicit UiCommandExtractor(UiTree* uiTree) : uiTree(uiTree) {}

    UiCommandBuffer extract(float viewportAspect) const
    {
        return uiTree->buildCommandBuffer(viewportAspect);
    }

private:
    UiTree* uiTree = nullptr;
};
