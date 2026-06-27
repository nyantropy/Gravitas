#pragma once

#include <vector>

#include "GtsPlatformEventBus.hpp"
#include "GtsKeyEvent.h"
#include "GtsFrameStats.h"
#include "GraphicsConfig.h"
#include "IResourceProvider.hpp"
#include "EditorPreviewRenderData.h"
#include "ParticleFrameData.h"
#include "RenderCommand.h"
#include "RenderViewport.h"
#include "UiCommand.h"

// Engine-facing interface for the graphics module.
// Zero Vulkan, zero GLFW — only engine-facing types.
class IGtsGraphicsModule
{
public:
    virtual ~IGtsGraphicsModule() = default;

    virtual void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                             const std::vector<ObjectUploadCommand>& objectUploads,
                             const std::vector<CameraUploadCommand>& cameraUploads,
                             const ParticleFrameData& particleData,
                             const RenderViewportRect& sceneViewport,
                             const UiCommandBuffer& uiBuffer,
                             const EditorPreviewRenderData& editorPreview,
                             const GtsFrameStats& stats) = 0;
    virtual texture_id_type ensureEditorPreviewTarget(uint32_t width, uint32_t height) = 0;
    virtual void releaseEditorPreviewTarget() = 0;
    virtual void toggleDebugOverlay() = 0;
    virtual void cycleDebugOverlayPage() = 0;
    virtual void requestScreenshot() = 0;
    virtual void waitIdle() = 0;
    virtual void pollWindowEvents() = 0;
    virtual void shutdown() = 0;
    virtual bool isWindowOpen() const = 0;
    virtual float getAspectRatio() const = 0;
    virtual void getViewportSize(int& width, int& height) const = 0;
    virtual RuntimeGraphicsSettings getRuntimeGraphicsSettings() const = 0;
    virtual std::vector<GraphicsMonitorInfo> getAvailableMonitors() const = 0;
    virtual bool applyRuntimeGraphicsSettings(const RuntimeGraphicsSettings& settings) = 0;
    virtual IResourceProvider* getResourceProvider() = 0;
    virtual GtsPlatformEventBus& getEventBus() = 0;
};
