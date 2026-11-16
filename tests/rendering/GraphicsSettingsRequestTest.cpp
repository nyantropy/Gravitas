#include "GraphicsSettingsRequestTest.h"
#include "EngineServiceRegistry.h"
#include "GtsCommandConsumer.h"
#include "ProfileAccumulator.h"
#include "RenderEngineCommands.h"
#include "RenderingRuntime.h"
#include "SceneExecutionPolicy.h"
#include "TimeContext.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_const_v<gts::rendering::GraphicsSettingsRequest::element_type>);
namespace
{
    int checks = 0;
    void require(const bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace
void SettingsGraphics::renderFrame(float, const std::vector<RenderCommand>&, const MaterialFrameData&,
                                   const std::vector<ObjectUploadCommand>&,
                                   const std::vector<CameraUploadCommand>&, const ParticleFrameData&,
                                   const RenderViewportRect&, const UiCommandBuffer&,
                                   const EditorPreviewRenderData&, const GtsFrameStats& frame,
                                   const GtsModelFrameData&)
{
    stats = frame;
    if (finishOnFrame)
        state.pending = false;
}
texture_id_type SettingsGraphics::ensureEditorPreviewTarget(uint32_t, uint32_t)
{
    return 0;
}
void SettingsGraphics::releaseEditorPreviewTarget()
{
}
void SettingsGraphics::toggleDebugOverlay()
{
}
void SettingsGraphics::cycleDebugOverlayPage()
{
}
void SettingsGraphics::requestScreenshot(const std::string&)
{
}
GtsFrameStats SettingsGraphics::getLastFrameStats() const
{
    return stats;
}
void SettingsGraphics::waitIdle()
{
}
void SettingsGraphics::pollWindowEvents()
{
}
void SettingsGraphics::shutdown()
{
}
bool SettingsGraphics::isWindowOpen() const
{
    return true;
}
float SettingsGraphics::getAspectRatio() const
{
    return 1;
}
void SettingsGraphics::getViewportSize(int& w, int& h) const
{
    w = h = 32;
}
GraphicsSettings SettingsGraphics::getRequestedGraphicsSettings() const
{
    return requested;
}
GraphicsRuntimeState SettingsGraphics::getGraphicsRuntimeState() const
{
    return state;
}
std::vector<GraphicsMonitorInfo> SettingsGraphics::getAvailableMonitors() const
{
    return {};
}
GraphicsSettingsApplyResult SettingsGraphics::applyGraphicsSettings(const GraphicsSettings& value)
{
    const auto error = validateGraphicsSettings(value);
    if (!error.empty())
        return {GraphicsSettingsApplyStatus::Rejected, error};
    requested = value;
    state.pending = defer;
    state.outputExtent = {32, 32};
    state.renderExtent = {32, 32};
    state.presentMode = PresentModePreference::Fifo;
    return {defer ? GraphicsSettingsApplyStatus::Pending : GraphicsSettingsApplyStatus::Applied, {}};
}
IResourceProvider* SettingsGraphics::getResourceProvider()
{
    return nullptr;
}
GtsPlatformEventBus& SettingsGraphics::getEventBus()
{
    return events;
}
void SettingsScene::onLoad(EcsControllerContext&, const GtsSceneTransitionData*)
{
}
void SettingsScene::onUpdateSimulation(const EcsSimulationContext&)
{
}

int main()
try
{
    using namespace gts::rendering;
    using Status = GraphicsSettingsRequestStatus;
    GraphicsSettingsRequest dropped;
    {
        GtsCommandBuffer commands;
        GraphicsSettings settings;
        settings.window.monitorName = "preferred";
        dropped = requestApplyGraphicsSettings(commands, settings);
        settings.window.monitorName = "mutated";
        require(dropped->status == Status::Queued && dropped->requested.window.monitorName == "preferred" &&
                    !dropped->observed,
                "Queued request did not own settings");
    }
    require(dropped->status == Status::Abandoned, "Dropped writer left request queued");
    SettingsGraphics graphics;
    GraphicsSettingsRequest abandoned, pending;
    {
        RenderingRuntime runtime(true, graphics, gts::execution::selectFrameBuildMode,
                                 [&graphics](const GraphicsSettings& settings)
                                 { return graphics.applyGraphicsSettings(settings); });
        GtsCommandBuffer commands;
        auto dispatch = [&]
        {
            for (const auto& command : GtsCommandConsumer::takePendingCommands(commands))
                require(runtime.applyExtensionCommand(std::get<GtsExtensionCommand>(command)),
                        "Request not consumed");
        };
        GraphicsSettings settings;
        settings.presentation.mode = PresentModePreference::Mailbox;
        const auto applied = requestApplyGraphicsSettings(commands, settings);
        dispatch();
        require(applied->status == Status::Applied &&
                    applied->observed->presentMode == PresentModePreference::Fifo,
                "Applied request lost observed fallback");
        graphics.defer = true;
        pending = requestApplyGraphicsSettings(commands, settings);
        dispatch();
        require(pending->status == Status::Pending && pending->observed->pending,
                "Pending request prematurely applied");
        const auto before = graphics.requested;
        settings.window.width = -1;
        const auto rejected = requestApplyGraphicsSettings(commands, settings);
        dispatch();
        require(rejected->status == Status::Rejected && !rejected->message.empty(), "Rejection lost reason");
        require(pending->status == Status::Pending && graphics.requested.window.width == before.window.width,
                "Rejected request replaced pending/accepted preferences");
        SettingsScene oldScene;
        EcsControllerContext oldContext{oldScene.getWorld()};
        oldScene.unload(oldContext);
        EngineServiceRegistry services;
        runtime.afterSceneUnload(services);
        require(pending->status == Status::Pending, "Scene unload lost pending receipt");
        SettingsScene nextScene;
        TimeContext time;
        ProfileAccumulator profiler;
        graphics.finishOnFrame = true;
        runtime.renderFrame(0.01f, nextScene, time, 0, 0, 0, 0, 0, 32, 32, profiler);
        require(pending->status == Status::Applied && !pending->observed->pending,
                "Deferred receipt did not complete after draw");
        settings.window.width = before.window.width;
        graphics.finishOnFrame = false;
        const auto first = requestApplyGraphicsSettings(commands, settings);
        const auto second = requestApplyGraphicsSettings(commands, settings);
        dispatch();
        require(first->status == Status::Superseded && second->status == Status::Pending,
                "Latest accepted request did not supersede pending");
        requestSetFrustumCullingEnabled(commands, false);
        dispatch();
        require(second->status == Status::Superseded, "Untracked accepted change lost supersession");
        abandoned = requestApplyGraphicsSettings(commands, settings);
        dispatch();
        require(abandoned->status == Status::Pending, "Abandonment fixture did not defer");
    }
    require(abandoned->status == Status::Abandoned && pending->status == Status::Applied,
            "Renderer destruction lost terminal state or left pending observer");
    {
        RenderingRuntime runtime(true, graphics, gts::execution::selectFrameBuildMode);
        GtsCommandBuffer commands;
        const auto missing = requestApplyGraphicsSettings(commands, GraphicsSettings{});
        for (const auto& command : GtsCommandConsumer::takePendingCommands(commands))
            runtime.applyExtensionCommand(std::get<GtsExtensionCommand>(command));
        require(missing->status == Status::Rejected && !missing->message.empty(),
                "Missing service silently accepted request");
    }
    std::cout << "Graphics settings receipts: " << checks << " checks passed\n";
    return 0;
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
