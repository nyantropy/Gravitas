#pragma once

#include <vector>

#include "GtsEvent.hpp"
#include "GtsKeyEvent.h"
#include "RenderCommand.h"
#include "UiCommand.h"
#include "IResourceProvider.hpp"
#include "GtsFrameStats.h"

// Engine-facing interface for the graphics module.
// Zero Vulkan, zero GLFW — only engine-facing types.
class IGtsGraphicsModule
{
public:
    virtual ~IGtsGraphicsModule() = default;

    virtual void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                             const UiCommandBuffer& uiBuffer,
                             const GtsFrameStats& stats) = 0;
    virtual void toggleDebugOverlay() = 0;
    virtual void pollWindowEvents() = 0;
    virtual void shutdown() = 0;
    virtual bool isWindowOpen() const = 0;
    virtual float getAspectRatio() const = 0;
    virtual void getViewportSize(int& width, int& height) const = 0;
    virtual IResourceProvider* getResourceProvider() = 0;

    // Events the engine subscribes to at startup
    virtual GtsEvent<int, int>& onResize() = 0;
    virtual GtsEvent<GtsKeyEvent>& onKeyPressed() = 0;
    virtual GtsEvent<int, uint32_t>& onFrameEnded() = 0;
};
