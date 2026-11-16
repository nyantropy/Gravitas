#pragma once
#include "GtsScene.hpp"
#include "IGtsGraphicsModule.hpp"

class SettingsGraphics : public IGtsGraphicsModule
{
  public:
    GraphicsSettings requested;
    GraphicsRuntimeState state;
    bool defer = false;
    bool finishOnFrame = false;
    GtsFrameStats stats;
    GtsPlatformEventBus events;
    void renderFrame(float, const std::vector<RenderCommand>&, const MaterialFrameData&,
                     const std::vector<ObjectUploadCommand>&, const std::vector<CameraUploadCommand>&,
                     const ParticleFrameData&, const RenderViewportRect&, const UiCommandBuffer&,
                     const EditorPreviewRenderData&, const GtsFrameStats&, const GtsModelFrameData&) override;
    texture_id_type ensureEditorPreviewTarget(uint32_t, uint32_t) override;
    void releaseEditorPreviewTarget() override;
    void toggleDebugOverlay() override;
    void cycleDebugOverlayPage() override;
    void requestScreenshot(const std::string&) override;
    GtsFrameStats getLastFrameStats() const override;
    void waitIdle() override;
    void pollWindowEvents() override;
    void shutdown() override;
    bool isWindowOpen() const override;
    float getAspectRatio() const override;
    void getViewportSize(int&, int&) const override;
    GraphicsSettings getRequestedGraphicsSettings() const override;
    GraphicsRuntimeState getGraphicsRuntimeState() const override;
    std::vector<GraphicsMonitorInfo> getAvailableMonitors() const override;
    GraphicsSettingsApplyResult applyGraphicsSettings(const GraphicsSettings&) override;
    IResourceProvider* getResourceProvider() override;
    GtsPlatformEventBus& getEventBus() override;
};

class SettingsScene : public GtsScene
{
  public:
    void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override;
    void onUpdateSimulation(const EcsSimulationContext&) override;
};
