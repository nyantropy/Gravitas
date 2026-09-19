#pragma once
#include "FrameBuildMode.h"
#include "GtsModelFrameData.h"

#include <cstdint>
#include <functional>
#include <memory>

#include "IEngineModule.h"

class GtsScene;
class IGtsGraphicsModule;
class IResourceProvider;
class InputBindingRegistry;
class ProfileAccumulator;
class RenderPipeline;
class UiSystem;
struct EcsControllerContext;
struct GtsExtensionCommand;
struct GraphicsSettings;
struct TimeContext;
struct EcsExecutionSelection;

namespace gts::rendering
{
    using FrameBuildModeSelector = FrameBuildMode (*)(const EcsExecutionSelection&);

    class RenderingRuntime : public IEngineModule
    {
    public:
        using GraphicsSettingsCallback = std::function<void(const GraphicsSettings&)>;

        RenderingRuntime(bool                     frustumCullingEnabled,
                         IGtsGraphicsModule&      graphics,
                         FrameBuildModeSelector   frameBuildModeSelector,
                         GraphicsSettingsCallback graphicsSettingsCallback = {});
        ~RenderingRuntime() override;

        const char* name() const override;
        void registerInputBindings(InputBindingRegistry& input) override;
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
        void dispatchUiInput(const InputBindingRegistry* input,
                             int windowPixelWidth,
                             int windowPixelHeight,
                             uint64_t frameId);

        void renderFrame(float dt,
                         GtsScene& activeScene,
                         const TimeContext& time,
                         uint32_t simulationTickCount,
                         float simulationCpuMs,
                         float controllerCpuMs,
                         float frameCpuMs,
                         uint32_t extraControllerSystemCount,
                         int windowPixelWidth,
                         int windowPixelHeight,
                         ProfileAccumulator& profiler);

    private:
        IGtsGraphicsModule& graphics;
        FrameBuildModeSelector          frameBuildModeSelector;
        GraphicsSettingsCallback graphicsSettingsCallback;
        std::unique_ptr<RenderPipeline> renderPipeline;
        std::unique_ptr<UiSystem> uiSystem;
        bool uiEnabled = true;
        GtsModelFrameData modelFrame;
    };
}
