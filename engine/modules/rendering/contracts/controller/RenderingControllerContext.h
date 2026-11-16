#pragma once

#include "EcsControllerContext.hpp"

class IResourceProvider;
class IUiResourceProvider;

namespace gts::rendering
{
    struct RenderingControllerContext
    {
        IResourceProvider* resources                = nullptr;
        IUiResourceProvider* uiResources             = nullptr;
        float              windowAspectRatio        = 1.0f;
        float              windowPixelWidth         = 1.0f;
        float              windowPixelHeight        = 1.0f;
        float              sceneViewportPixelX      = 0.0f;
        float              sceneViewportPixelY      = 0.0f;
        float              sceneViewportPixelWidth  = 1.0f;
        float              sceneViewportPixelHeight = 1.0f;
        float              sceneViewportAspectRatio = 1.0f;
    };

    inline const RenderingControllerContext& controllerContext(const EcsControllerContext& ctx)
    {
        return ctx.frameData.get<RenderingControllerContext>();
    }

    inline RenderingControllerContext& controllerContext(EcsControllerContext& ctx)
    {
        return ctx.frameData.edit<RenderingControllerContext>();
    }
} // namespace gts::rendering
