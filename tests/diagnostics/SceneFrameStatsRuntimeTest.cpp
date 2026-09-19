#include "GtsScene.hpp"
#include "IGtsGraphicsModule.hpp"
#include "ISceneFrameStats.h"
#include "ProfileAccumulator.h"
#include "RenderingRuntime.h"
#include "TimeContext.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    class Graphics final : public IGtsGraphicsModule
    {
        public:
        std::vector<std::string> events;
        GtsFrameStats            submitted;
        GtsFrameStats            finalStats;
        bool                     staleResult = false;

        void renderFrame(float,
                         const std::vector<RenderCommand>&,
                         const MaterialFrameData&,
                         const std::vector<ObjectUploadCommand>&,
                         const std::vector<CameraUploadCommand>&,
                         const ParticleFrameData&,
                         const RenderViewportRect&,
                         const UiCommandBuffer&,
                         const EditorPreviewRenderData&,
                         const GtsFrameStats& stats,
                         const GtsModelFrameData&) override
        {
            events.push_back("submit");
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
        void pollWindowEvents() override {}
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
        GtsPlatformEventBus eventsBus;
    };

    class Scene : public GtsScene
    {
        public:
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override {}
        void onUpdateSimulation(const EcsSimulationContext&) override {}
    };

    class Participant final : public Scene, public ISceneFrameStats
    {
        public:
        Participant(Graphics& graphics, ProfileAccumulator& accumulator) : graphics(graphics), accumulator(accumulator)
        {
        }
        mutable int   contributions = 0;
        int           observations  = 0;
        GtsFrameStats observed;

        void populateFrameStats(GtsFrameStats& stats) const override
        {
            graphics.events.push_back("contribute");
            ++contributions;
            require(stats.simulationTickCount == 4 && stats.frameCpuMs == 7.0f,
                    "Initial stats must precede contribution");
            stats.sceneEntityCount      = 64000;
            stats.minimapCellCount      = 90;
            stats.physicsCollisionCount = 2;
            stats.playerCollisionCount  = 1;
            // Extraction owns this field and must still overwrite the contribution.
            stats.totalObjects = 999;
        }
        void onFrameStats(const GtsFrameStats& stats) override
        {
            graphics.events.push_back("observe");
            require(accumulator.frameCount == observations, "Observation must precede accumulation");
            require(graphics.submitted.frameIndex == stats.frameIndex,
                    "Observation must follow this frame's submission");
            require(stats.renderSubmitCpuMs >= 0.0f, "Submission timing must be finalized before observation");
            observed = stats;
            ++observations;
        }

        private:
        Graphics&           graphics;
        ProfileAccumulator& accumulator;
    };

    // Benchmark scenes only observe; contribution remains an optional no-op.
    class Observer final : public Scene, public ISceneFrameStats
    {
        public:
        Observer(Graphics& graphics, ProfileAccumulator& accumulator) : graphics(graphics), accumulator(accumulator) {}
        int  observations = 0;
        void onFrameStats(const GtsFrameStats& stats) override
        {
            require(graphics.events.back() == "submit" && stats.gpuTimingAvailable == 1 && stats.drawCalls == 12,
                    "Benchmark observer must receive submitted backend/GPU statistics");
            require(accumulator.frameCount == observations, "Benchmark observer must run before accumulation");
            ++observations;
        }

        private:
        Graphics&           graphics;
        ProfileAccumulator& accumulator;
    };
} // namespace

int main()
{
    Graphics                         graphics;
    gts::rendering::RenderingRuntime runtime(false, graphics);
    runtime.setUiEnabled(false);
    TimeContext time;
    auto        render = [&](GtsScene& scene, ProfileAccumulator& accumulator)
    {
        ++time.frame;
        runtime.renderFrame(0.01f, scene, time, 4, 1.0f, 2.0f, 7.0f, 0, 32, 32, accumulator);
    };

    ProfileAccumulator accumulator;
    Participant        participant(graphics, accumulator);
    render(participant, accumulator);
    require(graphics.events == std::vector<std::string>{"contribute", "extracted", "submit", "observe"},
            "Profiling phase order changed");
    require(graphics.submitted.sceneEntityCount == 64000 && graphics.submitted.minimapCellCount == 90 &&
                graphics.submitted.physicsCollisionCount == 2 && graphics.submitted.playerCollisionCount == 1,
            "Scene contributions must reach submission unchanged");
    require(graphics.submitted.totalObjects == 0, "Extraction must follow scene contribution");
    require(participant.observed.gpuFrameMs == 3.5f && participant.observed.drawCalls == 12 &&
                accumulator.sum.drawCalls == 12 && accumulator.sum.gpuFrameMs == 3.5f,
            "Final backend stats must reach observer and accumulator");
    graphics.staleResult = true;
    graphics.events.clear();
    render(participant, accumulator);
    require(participant.observed.frameIndex == time.frame && participant.observed.drawCalls == 0 &&
                participant.observed.sceneEntityCount == 64000,
            "Stale backend fallback semantics changed");
    require(participant.contributions == 2 && participant.observations == 2 && accumulator.frameCount == 2,
            "Hooks must run once per rendered frame, not once per simulation tick");

    graphics.staleResult = false;
    graphics.events.clear();
    Scene              plain;
    ProfileAccumulator plainAccumulator;
    render(plain, plainAccumulator);
    require(graphics.events == std::vector<std::string>{"extracted", "submit"} && plainAccumulator.frameCount == 1 &&
                graphics.submitted.sceneEntityCount == 0,
            "Nonparticipating scenes must render normally without stale contribution");

    ProfileAccumulator benchmarkAccumulator;
    Observer           observer(graphics, benchmarkAccumulator);
    render(observer, benchmarkAccumulator);
    render(observer, benchmarkAccumulator);
    require(observer.observations == 2 && benchmarkAccumulator.frameCount == 2,
            "Observation-only scenes must receive exactly one callback per frame");
}
