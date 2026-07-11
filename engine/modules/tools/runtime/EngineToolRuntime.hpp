#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

#include "DebugDrawPrimitives.h"
#include "DebugDrawSettingsComponent.h"
#include "ECSControllerSystem.hpp"
#include "DebugDrawSystem.hpp"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "EngineGizmoStateComponent.h"
#include "EngineGizmoSystem.hpp"
#include "EngineToolCameraSystem.hpp"
#include "EngineToolDebugDrawSystem.hpp"
#include "EngineToolSelectionHighlightSystem.hpp"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolShellSystem.hpp"
#include "EngineToolStateComponent.h"
#include "EngineToolWorldPickerSystem.hpp"
#include "EngineToolWorkspaceComponent.h"
#include "ToolLaunchPreset.h"

namespace gts::tools
{
    // Engine-owned tooling runner that keeps editor state outside individual scenes.
    class EngineToolRuntime
    {
        public:
        EngineToolRuntime()
        {
            resetSceneSystems();
        }

        void prepare(const EcsControllerContext& ctx)
        {
            ensurePersistentDefaults();

            const std::string sceneName = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            if (sceneName != activeSceneName)
                handleSceneChanged(sceneName);

            applyPendingStartupOptions();
            seedPersistentState(ctx.world);
            shellSystem.prepareVisibility(ctx);
            capturePersistentState(ctx.world);
            publishWorkspace(ctx);
        }

        void update(const EcsControllerContext& ctx)
        {
            prepare(ctx);

            runSystem(shellSystem, ctx);
            runSystem(cameraSystem, ctx);
            runSystem(gizmoSystem, ctx);
            runSystem(toolDebugDrawSystem, ctx);
            runSystem(worldPickerSystem, ctx);
            runSystem(selectionHighlightSystem, ctx);
            runSystem(debugDrawSystem, ctx);

            capturePersistentState(ctx.world);
        }

        uint32_t controllerSystemCount() const
        {
            return 7;
        }

        void shutdown()
        {
            shellSystem.shutdown();
        }

        void setStartupOptions(ToolStartupOptions options)
        {
            startupOptions = std::move(options);
            startupOptionsPending = startupOptions.hasAnyToolState();
        }

        private:
        EngineToolShellSystem              shellSystem;
        EngineToolCameraSystem             cameraSystem;
        EngineGizmoSystem                  gizmoSystem;
        EngineToolDebugDrawSystem          toolDebugDrawSystem;
        EngineToolWorldPickerSystem        worldPickerSystem;
        EngineToolSelectionHighlightSystem selectionHighlightSystem;
        gts::debugdraw::DebugDrawSystem    debugDrawSystem;

        EngineToolStateComponent                   persistentToolState;
        EngineGizmoStateComponent                  persistentGizmoState;
        gts::debugdraw::DebugDrawSettingsComponent persistentDebugDrawSettings;
        std::string                                activeSceneName;
        bool                                       persistentToolStateReady  = false;
        bool                                       persistentGizmoStateReady = false;
        bool                                       persistentDebugDrawReady  = false;
        ToolStartupOptions                         startupOptions;
        bool                                       startupOptionsPending = false;

        template <typename System> static void runSystem(System& system, const EcsControllerContext& ctx)
        {
            const auto start = std::chrono::steady_clock::now();
            system.update(ctx);
            const auto  end = std::chrono::steady_clock::now();
            const float ms  = std::chrono::duration<float, std::milli>(end - start).count();
            ctx.world.recordControllerProfile(system.getName(), ms);
            ctx.world.flushCommands();
        }

        void resetSceneSystems()
        {
            shellSystem              = EngineToolShellSystem{};
            cameraSystem             = EngineToolCameraSystem{};
            gizmoSystem              = EngineGizmoSystem{};
            toolDebugDrawSystem      = EngineToolDebugDrawSystem{};
            worldPickerSystem        = EngineToolWorldPickerSystem{};
            selectionHighlightSystem = EngineToolSelectionHighlightSystem{};
            debugDrawSystem          = gts::debugdraw::DebugDrawSystem{};

            shellSystem.setDebugName(gts::detail::typeName<EngineToolShellSystem>());
            cameraSystem.setDebugName(gts::detail::typeName<EngineToolCameraSystem>());
            gizmoSystem.setDebugName(gts::detail::typeName<EngineGizmoSystem>());
            toolDebugDrawSystem.setDebugName(gts::detail::typeName<EngineToolDebugDrawSystem>());
            worldPickerSystem.setDebugName(gts::detail::typeName<EngineToolWorldPickerSystem>());
            selectionHighlightSystem.setDebugName(gts::detail::typeName<EngineToolSelectionHighlightSystem>());
            debugDrawSystem.setDebugName(gts::detail::typeName<gts::debugdraw::DebugDrawSystem>());
        }

        void ensurePersistentDefaults()
        {
            if (persistentToolStateReady)
                return;

            persistentToolState      = EngineToolStateComponent{};
            persistentToolStateReady = true;
        }

        void handleSceneChanged(const std::string& sceneName)
        {
            activeSceneName = sceneName;

            persistentToolState.selectedEntity            = invalidToolEntity();
            persistentToolState.hoveredEntity             = invalidToolEntity();
            persistentToolState.selectionSource           = EngineToolSelectionSource::None;
            persistentToolState.selectionChangedThisFrame = false;
            if (!sceneName.empty())
                persistentToolState.status = "SCENE " + sceneName;

            persistentGizmoState.hoveredAxis  = EngineGizmoAxis::None;
            persistentGizmoState.activeAxis   = EngineGizmoAxis::None;
            persistentGizmoState.activeEntity = invalidToolEntity();

            resetSceneSystems();
        }

        void applyPendingStartupOptions()
        {
            if (!startupOptionsPending)
                return;

            if (startupOptions.hasVisible)
            {
                persistentToolState.visible = startupOptions.visible;
                persistentToolState.status = startupOptions.visible ? "TOOLS VISIBLE" : "TOOLS HIDDEN";
            }

            if (startupOptions.hasVisualEvaluation && startupOptions.visualEvaluation)
            {
                persistentToolState.selectedEntity = invalidToolEntity();
                persistentToolState.hoveredEntity = invalidToolEntity();
                persistentToolState.selectionSource = EngineToolSelectionSource::None;

                persistentGizmoState = EngineGizmoStateComponent{};
                persistentGizmoState.enabled = false;
                persistentGizmoStateReady = true;

                persistentDebugDrawSettings = gts::debugdraw::DebugDrawSettingsComponent{};
                persistentDebugDrawSettings.enabled = false;
                persistentDebugDrawSettings.selectedBounds = false;
                persistentDebugDrawSettings.allBounds = false;
                persistentDebugDrawSettings.transformAxes = false;
                persistentDebugDrawSettings.cameraFrustum = false;
                persistentDebugDrawSettings.pickRay = false;
                persistentDebugDrawReady = true;
            }

            if (startupOptions.hasGizmos)
            {
                if (!persistentGizmoStateReady)
                    persistentGizmoState = EngineGizmoStateComponent{};
                persistentGizmoState.enabled = startupOptions.gizmosEnabled;
                persistentGizmoStateReady = true;
            }

            if (startupOptions.hasDebugDraw)
            {
                if (!persistentDebugDrawReady)
                    persistentDebugDrawSettings = gts::debugdraw::DebugDrawSettingsComponent{};
                persistentDebugDrawSettings.enabled = startupOptions.debugDrawEnabled;
                if (!startupOptions.debugDrawEnabled)
                {
                    persistentDebugDrawSettings.selectedBounds = false;
                    persistentDebugDrawSettings.allBounds = false;
                    persistentDebugDrawSettings.transformAxes = false;
                    persistentDebugDrawSettings.cameraFrustum = false;
                    persistentDebugDrawSettings.pickRay = false;
                }
                persistentDebugDrawReady = true;
            }

            const std::string shellStatus = shellSystem.applyStartupOptions(startupOptions);
            if (!shellStatus.empty())
                persistentToolState.status = shellStatus;

            startupOptionsPending = false;
        }

        void seedPersistentState(ECSWorld& world)
        {
            ensurePersistentDefaults();

            EngineToolStateComponent& toolState = ensureToolState(world);
            toolState                           = persistentToolState;
            toolState.selectionChangedThisFrame = false;

            if (persistentGizmoStateReady)
                ensureGizmoState(world) = persistentGizmoState;

            if (persistentDebugDrawReady)
                gts::debugdraw::ensureSettings(world) = persistentDebugDrawSettings;
        }

        void publishWorkspace(const EcsControllerContext& ctx) const
        {
            const int width  = std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth)));
            const int height = std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight)));
            publishEngineToolWorkspace(ctx.world,
                                       width,
                                       height,
                                       persistentToolState.visible,
                                       shellSystem.currentWorkspace());
        }

        void capturePersistentState(ECSWorld& world)
        {
            if (world.hasAny<EngineToolStateComponent>())
            {
                persistentToolState      = world.getSingleton<EngineToolStateComponent>();
                persistentToolStateReady = true;
            }

            if (world.hasAny<EngineGizmoStateComponent>())
            {
                persistentGizmoState      = world.getSingleton<EngineGizmoStateComponent>();
                persistentGizmoStateReady = true;
            }

            if (world.hasAny<gts::debugdraw::DebugDrawSettingsComponent>())
            {
                persistentDebugDrawSettings = world.getSingleton<gts::debugdraw::DebugDrawSettingsComponent>();
                persistentDebugDrawReady    = true;
            }
        }

        static EngineToolStateComponent& ensureToolState(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolStateComponent>())
                return world.createSingleton<EngineToolStateComponent>();
            return world.getSingleton<EngineToolStateComponent>();
        }

        static EngineGizmoStateComponent& ensureGizmoState(ECSWorld& world)
        {
            if (!world.hasAny<EngineGizmoStateComponent>())
                return world.createSingleton<EngineGizmoStateComponent>();
            return world.getSingleton<EngineGizmoStateComponent>();
        }
    };
} // namespace gts::tools
