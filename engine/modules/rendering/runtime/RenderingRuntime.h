#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "IEngineModule.h"

class GtsScene;
class IGtsGraphicsModule;
class IResourceProvider;
class ProfileAccumulator;
class RenderPipeline;
class UiSystem;
struct EcsControllerContext;
struct GtsExtensionCommand;
struct RuntimeGraphicsSettings;
struct TimeContext;

namespace gts::rendering
{
    class RenderingRuntime : public IEngineModule
    {
    public:
        using GraphicsSettingsCallback = std::function<void(const RuntimeGraphicsSettings&)>;

        RenderingRuntime(bool frustumCullingEnabled,
                         IGtsGraphicsModule& graphics,
                         GraphicsSettingsCallback graphicsSettingsCallback = {});
        ~RenderingRuntime() override;

        const char* name() const override;
        void registerServices(EngineServiceRegistry& services) override;
        void unregisterServices(EngineServiceRegistry& services) override;
        void afterSceneUnload(EngineServiceRegistry& services) override;

        IResourceProvider* resources();
        UiSystem* ui();

        void clearUi();
        void setUiEnabled(bool enabled);
        bool isUiEnabled() const;

        void resetSceneState();
        void setVisibilityEnabled(bool enabled);
        void setVisibilityFrozen(bool frozen);
        bool applyExtensionCommand(const GtsExtensionCommand& command);

        void applySceneViewportMetrics(EcsControllerContext& ctx,
                                       int windowPixelWidth,
                                       int windowPixelHeight) const;

        void renderFrame(float dt,
                         GtsScene& activeScene,
                         const TimeContext& time,
                         float simulationCpuMs,
                         float controllerCpuMs,
                         float frameCpuMs,
                         uint32_t extraControllerSystemCount,
                         int windowPixelWidth,
                         int windowPixelHeight,
                         ProfileAccumulator& profiler);

    private:
        IGtsGraphicsModule& graphics;
        GraphicsSettingsCallback graphicsSettingsCallback;
        std::unique_ptr<RenderPipeline> renderPipeline;
        std::unique_ptr<UiSystem> uiSystem;
        bool uiEnabled = true;
    };
}
