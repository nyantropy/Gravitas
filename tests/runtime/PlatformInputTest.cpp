#include "GtsPlatform.h"
#include "GtsKeyEvent.h"
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
    class Graphics final : public IGtsGraphicsModule
    {
        public:
        explicit Graphics(GtsPlatformEventBus& bus) : eventsBus(bus) {}
        std::function<void(GtsPlatformEventBus&)> poll;
        std::vector<std::string> events;
        GtsFrameStats            submitted;
        GtsFrameStats            finalStats;
        bool                     staleResult = false;
        size_t submittedParticles = 0;
        view_id_type submittedCamera = 0;

        void renderFrame(float,
                         const std::vector<RenderCommand>&,
                         const MaterialFrameData&,
                         const std::vector<ObjectUploadCommand>&,
                         const std::vector<CameraUploadCommand>& cameras,
                         const ParticleFrameData& particles,
                         const RenderViewportRect&,
                         const UiCommandBuffer&,
                         const EditorPreviewRenderData&,
                         const GtsFrameStats& stats,
                         const GtsModelFrameData&) override
        {
            events.push_back("submit");
            submittedParticles = particles.instances.size();
            submittedCamera = cameras.empty() ? 0 : cameras.front().cameraViewID;
            submitted                     = stats;
            finalStats                    = stats;
            finalStats.gpuTimingSupported = 1;
            finalStats.gpuTimingAvailable = 1;
            finalStats.gpuFrameMs         = 3.5f;
            finalStats.drawCalls          = 12;
            if (staleResult)
                --finalStats.frameIndex;
        }
        GtsFrameStats getLastFrameStats() const override
        {
            return finalStats;
        }
        texture_id_type ensureEditorPreviewTarget(uint32_t, uint32_t) override
        {
            return 0;
        }
        void releaseEditorPreviewTarget() override
        {
            events.push_back("extracted");
        }
        void toggleDebugOverlay() override {}
        void cycleDebugOverlayPage() override {}
        void requestScreenshot(const std::string&) override {}
        void waitIdle() override {}
        void pollWindowEvents() override { if (poll) poll(eventsBus); }
        void shutdown() override {}
        bool isWindowOpen() const override
        {
            return true;
        }
        float getAspectRatio() const override
        {
            return 1.0f;
        }
        void getViewportSize(int& width, int& height) const override
        {
            width = height = 32;
        }
        GraphicsSettings getRequestedGraphicsSettings() const override
        {
            return {};
        }
        GraphicsRuntimeState getGraphicsRuntimeState() const override
        {
            return {};
        }
        std::vector<GraphicsMonitorInfo> getAvailableMonitors() const override
        {
            return {};
        }
        GraphicsSettingsApplyResult applyGraphicsSettings(const GraphicsSettings&) override
        {
            return {};
        }
        IResourceProvider* getResourceProvider() override
        {
            return nullptr;
        }
        GtsPlatformEventBus& getEventBus() override
        {
            return eventsBus;
        }

        private:
        GtsPlatformEventBus& eventsBus;
    };


    class Provider final : public gts::rendering::IGraphicsBackendProvider
    {
    public:
        mutable Graphics* graphics = nullptr;
        mutable GtsPlatformEventBus bus;
        GraphicsBackend backend() const override { return GraphicsBackend::Vulkan; }
        std::unique_ptr<IGtsGraphicsModule> create(const GraphicsConfig&) const override
        {
            auto result = std::make_unique<Graphics>(bus);
            graphics = result.get();
            return result;
        }
    };
}

int main()
{
    Provider provider;
    gts::rendering::GraphicsBackendRegistry backends;
    backends.registerProvider(provider);
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        GtsPlatform platform({}, backends);
        auto& bindings = *platform.getInputBindingRegistry();
        auto bind = [&](const char* action, InputTrigger::Type type, int code,
                        ActivationMode mode, ModifierFlags mods = ModifierFlags::None,
                        PausePolicy pause = PausePolicy::AlwaysActive)
        {
            bindings.bind({action, {type, code, mods}, mode, "", pause, true});
        };
        bind("press", InputTrigger::Type::Key, static_cast<int>(GtsKey::A), ActivationMode::Pressed);
        bind("release", InputTrigger::Type::Key, static_cast<int>(GtsKey::A), ActivationMode::Released);
        bind("held", InputTrigger::Type::Key, static_cast<int>(GtsKey::A), ActivationMode::Held);
        bind("modified", InputTrigger::Type::Key, static_cast<int>(GtsKey::B), ActivationMode::Pressed,
             ModifierFlags::Shift | ModifierFlags::Ctrl | ModifierFlags::Alt | ModifierFlags::Super);
        bind("mouse", InputTrigger::Type::MouseButton, 1, ActivationMode::Pressed);
        bind("mouse-release", InputTrigger::Type::MouseButton, 1, ActivationMode::Released);
        bind("game", InputTrigger::Type::Key, static_cast<int>(GtsKey::C), ActivationMode::Pressed,
             ModifierFlags::None, PausePolicy::Gameplay);
        bind("always", InputTrigger::Type::Key, static_cast<int>(GtsKey::C), ActivationMode::Pressed);

        provider.graphics->poll = [&](GtsPlatformEventBus& bus)
        {
            require(!bindings.isPressed("press"), "Bindings update after event polling");
            bus.emit(GtsKeyEvent{GtsKey::A, true, 0});
            bus.emit(GtsKeyEvent{GtsKey::A, false, 0});
            bus.emit(GtsCursorPositionEvent{12.5, -9.0});
            bus.emit(GtsScrollEvent{1.0, 2.0});
            bus.emit(GtsScrollEvent{-0.5, 3.0});
        };
        platform.beginFrame();
        require(bindings.isPressed("press") && bindings.isPressed("release") && !bindings.isHeld("held"),
                "Press/release in one poll retains both frame edges");
        require(bindings.mouseX() == 12.5 && bindings.mouseY() == -9.0 &&
                bindings.scrollX() == 0.5 && bindings.scrollY() == 5.0, "Cursor and accumulated scroll");
        provider.graphics->poll = {};
        platform.beginFrame();
        require(!bindings.isPressed("press") && bindings.scrollY() == 0 && bindings.mouseX() == 12.5,
                "Frame advancement clears edges/scroll, retains position");
        require(bindings.isSimulationPressed("press") && bindings.isSimulationReleased("release"),
                "No simulation tick: edges survive next frame");
        bindings.finishSimulationTick();
        require(!bindings.isSimulationPressed("press") && !bindings.isSimulationReleased("release"),
                "Exactly one tick consumes edges");

        provider.bus.emit(GtsKeyEvent{GtsKey::A, true, 0});
        platform.beginFrame();
        require(bindings.isHeld("held") && bindings.isPressed("press"), "Queued events dispatch after beginFrame");
        bindings.finishSimulationTick();
        provider.bus.emit(GtsKeyEvent{GtsKey::A, true, 0});
        platform.beginFrame();
        require(bindings.isHeld("held") && !bindings.isPressed("press") && !bindings.isSimulationPressed("press"),
                "Repeated key down does not create a new press");
        provider.bus.emit(GtsKeyEvent{GtsKey::B, true, 15});
        platform.beginFrame();
        require(bindings.isPressed("modified"), "All modifier bits translated");
        provider.bus.emit(GtsMouseButtonEvent{1, true, 0});
        provider.bus.emit(GtsMouseButtonEvent{1, false, 0});
        platform.beginFrame();
        require(bindings.isPressed("mouse") && bindings.isPressed("mouse-release"), "Mouse tap retains both edges");

        bindings.setPaused(true);
        require(!bindings.isSimulationPressed("modified"), "Pause transition clears pending simulation edges");
        provider.bus.emit(GtsKeyEvent{GtsKey::C, true, 0});
        platform.beginFrame();
        require(!bindings.isPressed("game") && bindings.isPressed("always"), "Pause gating unchanged");
        bindings.finishSimulationTick();
        bindings.setPaused(false);
        platform.beginFrame();
        require(!bindings.isSimulationPressed("game"), "Resume does not synthesize an edge");
    }
    // The bus intentionally outlives each platform. Stale subscriptions would write freed input state.
    provider.bus.emit(GtsKeyEvent{GtsKey::A, true, 0});
    provider.bus.emit(GtsMouseButtonEvent{0, true, 0});
    provider.bus.emit(GtsCursorPositionEvent{1, 2});
    provider.bus.emit(GtsScrollEvent{1, 2});
    provider.bus.dispatch();
}
