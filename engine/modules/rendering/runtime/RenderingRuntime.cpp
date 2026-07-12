#include "RenderingRuntime.h"

#include <algorithm>
#include <any>
#include <chrono>
#include <utility>
#include <vector>

#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "EcsExecutionProfile.h"
#include "EditorPreviewRenderData.h"
#include "EngineServiceRegistry.h"
#include "FrustumCullingStrategy.h"
#include "GtsCommand.h"
#include "GtsFrameStats.h"
#include "GtsScene.hpp"
#include "IGtsGraphicsModule.hpp"
#include "IResourceProvider.hpp"
#include "InputBindingRegistry.h"
#include "MaterialBindingSystem.hpp"
#include "ParticleFrameData.h"
#include "ProfileAccumulator.h"
#include "RenderEngineCommands.h"
#include "RenderGpuSystem.hpp"
#include "RenderPassVisibilityComponent.h"
#include "RenderPipeline.h"
#include "RenderViewportComponent.h"
#include "TimeContext.h"
#include "UiCommand.h"
#include "UiSystem.h"

namespace gts::rendering
{
    RenderingRuntime::RenderingRuntime(bool frustumCullingEnabled,
                                       IGtsGraphicsModule& graphics,
                                       GraphicsSettingsCallback graphicsSettingsCallback)
        : graphics(graphics)
        , graphicsSettingsCallback(std::move(graphicsSettingsCallback))
        , renderPipeline(std::make_unique<RenderPipeline>(
              std::make_unique<FrustumCullingStrategy>(frustumCullingEnabled)))
        , uiSystem(std::make_unique<UiSystem>(graphics.getResourceProvider()))
    {
    }

    RenderingRuntime::~RenderingRuntime() = default;

    const char* RenderingRuntime::name() const
    {
        return "rendering";
    }

    void RenderingRuntime::registerServices(EngineServiceRegistry& services)
    {
        services.registerService(*this);
        services.registerService(*uiSystem);
        if (IResourceProvider* provider = resources())
            services.registerService(*provider);
    }

    void RenderingRuntime::unregisterServices(EngineServiceRegistry& services)
    {
        services.unregisterService<IResourceProvider>();
        services.unregisterService<UiSystem>();
        services.unregisterService<RenderingRuntime>();
    }

    void RenderingRuntime::afterSceneUnload(EngineServiceRegistry&)
    {
        resetSceneState();
    }

    IResourceProvider* RenderingRuntime::resources()
    {
        return graphics.getResourceProvider();
    }

    UiSystem* RenderingRuntime::ui()
    {
        return uiSystem.get();
    }

    void RenderingRuntime::clearUi()
    {
        uiSystem->clear();
    }

    void RenderingRuntime::setUiEnabled(bool enabled)
    {
        uiEnabled = enabled;
        uiSystem->setEnabled(enabled);
    }

    bool RenderingRuntime::isUiEnabled() const
    {
        return uiEnabled;
    }

    void RenderingRuntime::resetSceneState()
    {
        renderPipeline->resetSceneState();
    }

    void RenderingRuntime::setVisibilityEnabled(bool enabled)
    {
        renderPipeline->setVisibilityEnabled(enabled);
    }

    void RenderingRuntime::setVisibilityFrozen(bool frozen)
    {
        renderPipeline->setVisibilityFrozen(frozen);
    }

    bool RenderingRuntime::applyExtensionCommand(const GtsExtensionCommand& command)
    {
        if (command.name == SET_FRUSTUM_CULLING_ENABLED_COMMAND)
        {
            if (const auto* payload = std::any_cast<SetFrustumCullingEnabledCommand>(&command.payload))
                setVisibilityEnabled(payload->enabled);
            return true;
        }

        if (command.name == SET_FRUSTUM_FREEZE_COMMAND)
        {
            if (const auto* payload = std::any_cast<SetFrustumFreezeCommand>(&command.payload))
                setVisibilityFrozen(payload->frozen);
            return true;
        }

        if (command.name == APPLY_GRAPHICS_SETTINGS_COMMAND)
        {
            if (const auto* payload = std::any_cast<ApplyGraphicsSettingsCommand>(&command.payload))
            {
                if (graphicsSettingsCallback)
                    graphicsSettingsCallback(payload->settings);
            }
            return true;
        }

        return false;
    }

    namespace
    {
        RenderViewportRect resolveSceneViewport(ECSWorld& world, int windowPixelWidth, int windowPixelHeight)
        {
            const RenderViewportRect fullViewport =
                RenderViewportRect::full(windowPixelWidth, windowPixelHeight);
            if (!world.hasAny<RenderViewportComponent>())
                return fullViewport;

            const RenderViewportComponent& viewport = world.getSingleton<RenderViewportComponent>();
            if (!viewport.sceneViewport.valid())
                return fullViewport;

            return viewport.sceneViewport.clampedTo(windowPixelWidth, windowPixelHeight);
        }
    }

    void RenderingRuntime::applySceneViewportMetrics(EcsControllerContext& ctx,
                                                     int windowPixelWidth,
                                                     int windowPixelHeight) const
    {
        const RenderViewportRect viewport = resolveSceneViewport(ctx.world, windowPixelWidth, windowPixelHeight);

        ctx.sceneViewportPixelX      = static_cast<float>(viewport.x);
        ctx.sceneViewportPixelY      = static_cast<float>(viewport.y);
        ctx.sceneViewportPixelWidth  = static_cast<float>(viewport.width);
        ctx.sceneViewportPixelHeight = static_cast<float>(viewport.height);
        ctx.sceneViewportAspectRatio = viewport.aspect();
    }

    void RenderingRuntime::dispatchUiInput(const InputBindingRegistry* input,
                                           int windowPixelWidth,
                                           int windowPixelHeight,
                                           uint64_t frameId)
    {
        UiInputFrame frame;
        if (input != nullptr)
        {
            const float width = std::max(1.0f, static_cast<float>(windowPixelWidth));
            const float height = std::max(1.0f, static_cast<float>(windowPixelHeight));
            frame.pointerX = std::clamp(static_cast<float>(input->mouseX()) / width, 0.0f, 1.0f);
            frame.pointerY = std::clamp(static_cast<float>(input->mouseY()) / height, 0.0f, 1.0f);
            const char* primaryAction =
                input->isContextActive("engine.tools") ? "engine.tools_select" : "engine.ui_primary";
            frame.primaryDown = input->isHeld(primaryAction);
            frame.primaryPressed = input->isPressed(primaryAction);
            frame.primaryReleased = input->isReleased(primaryAction);
            frame.cancelPressed = input->isPressed("engine.ui_cancel");
            frame.navigationUpPressed = input->isPressed("engine.ui_nav_up");
            frame.navigationDownPressed = input->isPressed("engine.ui_nav_down");
            frame.navigationLeftPressed = input->isPressed("engine.ui_nav_left");
            frame.navigationRightPressed = input->isPressed("engine.ui_nav_right");
            frame.navigationNextPressed = input->isPressed("engine.ui_nav_next");
            frame.navigationPreviousPressed = input->isPressed("engine.ui_nav_previous");
            frame.navigationSubmitPressed = input->isPressed("engine.ui_submit");
            frame.scrollX = static_cast<float>(input->scrollX());
            frame.scrollY = static_cast<float>(input->scrollY());
        }

        uiSystem->dispatchInput(frame, frameId);
    }

    void RenderingRuntime::renderFrame(float dt,
                                       GtsScene& activeScene,
                                       const TimeContext& time,
                                       float simulationCpuMs,
                                       float controllerCpuMs,
                                       float frameCpuMs,
                                       uint32_t extraControllerSystemCount,
                                       int windowPixelWidth,
                                       int windowPixelHeight,
                                       ProfileAccumulator& profiler)
    {
        ECSWorld& world = activeScene.getWorld();
        const RenderViewportRect sceneViewport = resolveSceneViewport(world, windowPixelWidth, windowPixelHeight);

        GtsFrameStats stats;
        stats.fps                   = (dt > 0.0f) ? 1.0f / dt : 0.0f;
        stats.frameTimeMs           = dt * 1000.0f;
        stats.visibleObjects        = static_cast<uint32_t>(renderPipeline->getExtractor().getLastVisibleRenderables());
        stats.totalObjects          = static_cast<uint32_t>(renderPipeline->getExtractor().getLastTotalRenderables());
        stats.controllerSystemCount =
            static_cast<uint32_t>(world.getControllerSystemCount()) + extraControllerSystemCount;
        stats.simulationSystemCount = static_cast<uint32_t>(world.getSimulationSystemCount());
        const auto renderMetrics    = RenderGpuSystem::getLastMetrics();
        stats.renderGpuUpdatedCount = renderMetrics.updatedRenderables;
        stats.renderGpuCpuMs        = renderMetrics.cpuTimeMs;
        stats.simulationCpuMs       = simulationCpuMs;
        stats.controllerCpuMs       = controllerCpuMs;
        stats.frameCpuMs            = frameCpuMs;
        stats.frameIndex            = time.frame;
        activeScene.populateFrameStats(stats);

        const SceneExecutionProfile& executionProfile = world.getCurrentExecutionProfile();
        const FrameBuildMode frameBuildMode = executionProfile.frameBuildMode;

        static const std::vector<RenderCommand> emptyRenderList;
        static const MaterialFrameData emptyMaterialFrameData;
        const std::vector<RenderCommand>* renderList = &emptyRenderList;
        const MaterialFrameData* materialFrameData = &emptyMaterialFrameData;
        if (frameBuildMode == FrameBuildMode::FullWorld)
        {
            renderList = &renderPipeline->build(world);
            materialFrameData = &renderPipeline->getLatestSnapshot().materialFrameData;
        }
        else if (frameBuildMode == FrameBuildMode::CachedWorldFrame)
        {
            renderList = &renderPipeline->getExtractor().getLastCommands();
            materialFrameData = &renderPipeline->getLatestSnapshot().materialFrameData;
        }

        static const UiCommandBuffer emptyUiBuffer;
        const UiCommandBuffer* uiBuffer = &emptyUiBuffer;
        static const EditorPreviewRenderData emptyEditorPreview;
        EditorPreviewRenderData* editorPreview = nullptr;
        if (world.hasAny<EditorPreviewRenderComponent>())
        {
            editorPreview = &world.getSingleton<EditorPreviewRenderComponent>().data;
            if (editorPreview->enabled)
            {
                editorPreview->width = std::max(1u, editorPreview->width);
                editorPreview->height = std::max(1u, editorPreview->height);
                editorPreview->viewport =
                    RenderViewportRect::full(static_cast<int>(editorPreview->width),
                                             static_cast<int>(editorPreview->height));
                editorPreview->colorTextureID =
                    graphics.ensureEditorPreviewTarget(editorPreview->width, editorPreview->height);
            }
            else
            {
                graphics.releaseEditorPreviewTarget();
                editorPreview->colorTextureID = 0;
            }
        }
        else
        {
            graphics.releaseEditorPreviewTarget();
        }

        if (uiEnabled && frameBuildMode != FrameBuildMode::None)
        {
            uiSystem->setEnabled(true);
            uiSystem->updateBindings();
            uiSystem->updateAnimations(dt);
            uiBuffer = &uiSystem->extractCommandsRef(windowPixelWidth, windowPixelHeight);
        }
        else
        {
            uiSystem->setEnabled(false);
        }

        ParticleFrameData emptyParticleData;
        const ParticleFrameData& particleData = world.hasAny<ParticleFrameDataComponent>()
            ? world.getSingleton<ParticleFrameDataComponent>().frameData
            : emptyParticleData;
        const RenderPassVisibilityComponent passVisibility = world.hasAny<RenderPassVisibilityComponent>()
            ? world.getSingleton<RenderPassVisibilityComponent>()
            : RenderPassVisibilityComponent::allVisible();

        const std::vector<RenderCommand>& submittedRenderList =
            passVisibility.renderScene ? *renderList : emptyRenderList;
        const MaterialFrameData& submittedMaterialFrameData =
            passVisibility.renderScene ? *materialFrameData : emptyMaterialFrameData;
        const ParticleFrameData& submittedParticleData =
            passVisibility.renderParticles && frameBuildMode == FrameBuildMode::FullWorld
                ? particleData
                : emptyParticleData;

        const auto& pipelineMetrics  = renderPipeline->getLastPipelineMetrics();
        const auto extractorMetrics  = renderPipeline->getExtractor().getLastMetrics();
        const auto& snapMetrics      = renderPipeline->getSnapshotBuilder().getLastMetrics();
        const auto uiMetrics         = uiSystem->getLastMetrics();

        stats.snapshotBuildCpuMs     = pipelineMetrics.snapshotBuildCpuMs;
        stats.visibilityCpuMs        = pipelineMetrics.visibilityCpuMs;
        stats.renderExtractCpuMs     = extractorMetrics.extractCpuMs;
        stats.renderExtractSortCpuMs = extractorMetrics.sortCpuMs;

        stats.snapshotStaticCount  = snapMetrics.staticRenderableCount;
        stats.snapshotDynamicCount = snapMetrics.dynamicRenderableCount;
        stats.snapshotDirtyCount   = snapMetrics.dirtyUpdatedCount;
        stats.snapshotReusedCount  = snapMetrics.reusedCount;

        stats.uiLayoutCpuMs     = uiMetrics.layoutMs;
        stats.uiVisualCpuMs     = uiMetrics.visualMs;
        stats.uiCommandCpuMs    = uiMetrics.commandBuildMs;
        stats.uiCpuMs           = uiMetrics.layoutMs + uiMetrics.visualMs + uiMetrics.commandBuildMs;
        stats.uiNodeCount       = uiMetrics.nodeCount;
        stats.uiPrimitiveCount  = uiMetrics.primitiveCount;
        stats.uiCommandCount    = uiMetrics.commandCount;
        stats.uiVertexCount     = uiMetrics.vertexCount;
        stats.uiIndexCount      = uiMetrics.indexCount;
        stats.uiCommandCacheHit = uiMetrics.commandCacheHit;

        stats.totalObjects              = extractorMetrics.totalRenderables;
        stats.visibleObjects            = extractorMetrics.visibleRenderables;
        stats.renderCommandVisitedCount = extractorMetrics.visitedRenderables;
        stats.renderCommandTotalCount   = extractorMetrics.cachedCommands;
        stats.renderCommandUpdatedCount = extractorMetrics.updatedCommands;
        stats.renderCommandSortedCount  = extractorMetrics.sortedThisFrame ? 1u : 0u;
        const auto materialMetrics = MaterialBindingSystem::getLastMetrics();
        stats.materialQueuedCount = materialMetrics.queuedMaterials;
        stats.materialSynchronizedCount = materialMetrics.synchronizedMaterials;
        stats.materialUserInvalidationCount = materialMetrics.userInvalidations;
        stats.materialFallbackSubstitutionCount = materialMetrics.fallbackSubstitutions;
        stats.materialReferenceAddCount = materialMetrics.referenceAdds;
        stats.materialReferenceRemoveCount = materialMetrics.referenceRemoves;
        stats.materialFullScanCount = materialMetrics.fullMaterialScans;

        const auto submitStart = std::chrono::steady_clock::now();
        graphics.renderFrame(dt,
                             submittedRenderList,
                             submittedMaterialFrameData,
                             renderPipeline->getLatestSnapshot().objectUploads,
                             renderPipeline->getLatestSnapshot().cameraUploads,
                             submittedParticleData,
                             sceneViewport,
                             *uiBuffer,
                             editorPreview != nullptr && editorPreview->enabled ? *editorPreview : emptyEditorPreview,
                             stats);
        const auto submitEnd    = std::chrono::steady_clock::now();
        stats.renderSubmitCpuMs =
            std::chrono::duration<float, std::milli>(submitEnd - submitStart).count();

        profiler.add(stats, dt);
        if (profiler.shouldPrint())
        {
            printProfile(profiler);
            world.printSimulationProfiles();
            world.printControllerProfiles();
            world.resetSimulationProfiles();
            world.resetControllerProfiles();
            profiler.reset();
        }
    }
}
