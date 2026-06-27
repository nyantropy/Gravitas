#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "AssetStatusPanel.hpp"
#include "CameraPanel.hpp"
#include "DebugDrawPanel.hpp"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolContext.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolPreviewCameraComponent.h"
#include "EngineToolRegistry.h"
#include "EngineToolWorkspaceComponent.h"
#include "EntityInspectorPanel.hpp"
#include "GraphicsConstants.h"
#include "ParticleEmitterInspectorPanel.hpp"
#include "ParticleEffectEditorPanel.hpp"
#include "RenderViewportComponent.h"
#include "SceneGizmoPanel.hpp"
#include "SceneCatalogPanel.hpp"
#include "ToolTheme.h"
#include "ToolWidgets.h"
#include "UiSystem.h"

namespace gts::tools
{
    class EngineToolShellSystem : public ECSControllerSystem
    {
        public:
        EngineToolShellSystem()
        {
            registry.addPanel<EntityInspectorPanel>();
            registry.addPanel<SceneGizmoPanel>();
            registry.addPanel<CameraPanel>();
            registry.addPanel<ParticleEmitterInspectorPanel>();
            registry.addPanel<ParticleEffectEditorPanel>();
            registry.addPanel<DebugDrawPanel>();
            registry.addPanel<AssetStatusPanel>();
            registry.addPanel<SceneCatalogPanel>();
        }

        void update(const EcsControllerContext& ctx) override
        {
            if (ctx.ui == nullptr)
                return;

            EngineToolStateComponent& state = ensureState(ctx.world);
            state.selectionChangedThisFrame = false;
            processVisibilityToggle(ctx, state);

            if (state.visible)
                setToolsInputContext(ctx, true);

            if (!state.visible)
            {
                setToolsInputContext(ctx, false);
                clearInputCapture(ctx.world);
                clearPreviewCameraRequest(ctx.world);
                publishWorkspace(ctx, false);
                return;
            }

            const EngineToolWorkspaceComponent& workspace = publishWorkspace(ctx, true);

            if (!ensureFont(ctx))
                return;

            EngineToolContext toolCtx = EngineToolContext::fromController(ctx);
            if (!ensureUi(toolCtx, state))
                return;
            syncLayout(toolCtx, workspace);
            syncPreviewCameraRequest(toolCtx.world);

            UiInteractionResult interaction = updateInteraction(ctx, state);
            updateInputCapture(ctx.world, state, interaction, ctx);
            updateTabs(toolCtx, state, interaction);

            if (registry.empty())
                return;

            state.activePanelIndex = std::min(state.activePanelIndex, registry.size() - 1);
            for (size_t i = 0; i < registry.size(); ++i)
            {
                EngineToolPanel* panel = registry.at(i);
                if (panel == nullptr)
                    continue;

                const bool active = i == state.activePanelIndex;
                panel->setVisible(toolCtx.ui, active);
                if (active)
                    panel->update(toolCtx, state, interaction);
            }

            setText(toolCtx.ui, footer, state.status);
            setText(toolCtx.ui, panelTitle, activePanelTitle(state));
        }

        void prepareVisibility(const EcsControllerContext& ctx)
        {
            if (ctx.ui == nullptr)
                return;

            EngineToolStateComponent& state = ensureState(ctx.world);
            processVisibilityToggle(ctx, state);
        }

        private:
        static constexpr float TopTabX           = 0.122f;
        static constexpr float TopTabY           = 0.004f;
        static constexpr float TopTabWidth       = 0.062f;
        static constexpr float TopTabHeight      = 0.016f;
        static constexpr float TopTabGap         = 0.004f;
        static constexpr float PaneHeaderHeight  = 0.066f;
        static constexpr float PaneFooterReserve = 0.016f;

        EngineToolRegistry      registry;
        BitmapFont              font;
        bool                    fontReady       = false;
        UiHandle                root            = UI_INVALID_HANDLE;
        UiHandle                topBar          = UI_INVALID_HANDLE;
        UiHandle                viewportToolbar = UI_INVALID_HANDLE;
        UiHandle                leftRail        = UI_INVALID_HANDLE;
        UiHandle                rightPane       = UI_INVALID_HANDLE;
        UiHandle                bottomBar       = UI_INVALID_HANDLE;
        UiHandle                title           = UI_INVALID_HANDLE;
        UiHandle                viewportLabel   = UI_INVALID_HANDLE;
        UiHandle                viewportMode    = UI_INVALID_HANDLE;
        UiHandle                panelTitle      = UI_INVALID_HANDLE;
        UiHandle                panelSubtitle   = UI_INVALID_HANDLE;
        UiHandle                contentRoot     = UI_INVALID_HANDLE;
        UiHandle                footer          = UI_INVALID_HANDLE;
        std::vector<ToolButton> tabs;
        std::vector<ToolButton> leftTools;
        bool                    toolsContextRequested = false;
        uint64_t                lastVisibilityToggleFrame = std::numeric_limits<uint64_t>::max();

        static constexpr const char* ToolsInputContext = "engine.tools";
        static constexpr const char* ToolsSelectAction = "engine.tools_select";

        static EngineToolStateComponent& ensureState(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolStateComponent>())
                return world.createSingleton<EngineToolStateComponent>();
            return world.getSingleton<EngineToolStateComponent>();
        }

        static EngineToolInputCaptureComponent& ensureInputCapture(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>())
                return world.createSingleton<EngineToolInputCaptureComponent>();
            return world.getSingleton<EngineToolInputCaptureComponent>();
        }

        bool processVisibilityToggle(const EcsControllerContext& ctx, EngineToolStateComponent& state)
        {
            if (ctx.input == nullptr || !ctx.input->isPressed("engine.tools_toggle"))
                return false;

            const uint64_t frame = ctx.time == nullptr ? 0u : ctx.time->frame;
            if (lastVisibilityToggleFrame == frame)
                return false;

            lastVisibilityToggleFrame = frame;
            state.visible = !state.visible;
            setToolsInputContext(ctx, state.visible);
            if (!state.visible && ctx.ui != nullptr)
            {
                clearInputCapture(ctx.world);
                destroyUi(*ctx.ui);
                publishWorkspace(ctx, false);
            }
            return true;
        }

        void setToolsInputContext(const EcsControllerContext& ctx, bool enabled)
        {
            if (ctx.input == nullptr)
                return;

            if (enabled)
            {
                if (!toolsContextRequested && !ctx.input->isContextActive(ToolsInputContext))
                    ctx.input->pushContext(ToolsInputContext);
                toolsContextRequested = true;
                return;
            }

            if (toolsContextRequested || ctx.input->isContextActive(ToolsInputContext))
                ctx.input->popContext(ToolsInputContext);
            toolsContextRequested = enabled;
        }

        static void clearInputCapture(ECSWorld& world)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            capture                                  = {};
        }

        static EngineToolWorkspaceComponent& publishWorkspace(const EcsControllerContext& ctx, bool active)
        {
            const int width  = std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth)));
            const int height = std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight)));
            return publishEngineToolWorkspace(ctx.world, width, height, active);
        }

        bool ensureFont(const EcsControllerContext& ctx)
        {
            if (fontReady)
                return true;
            if (ctx.resources == nullptr)
                return false;

            const font_id_type fontID =
                ctx.resources->requestFont(GraphicsConstants::ENGINE_RESOURCES + "/fonts/gravitasfont.font.json");
            const BitmapFont* loadedFont = ctx.resources->getFont(fontID);
            if (loadedFont == nullptr)
                return false;

            font      = *loadedFont;
            fontReady = true;
            return true;
        }

        bool ensureUi(EngineToolContext& ctx, EngineToolStateComponent& state)
        {
            if (root != UI_INVALID_HANDLE && ctx.ui.findNode(root) != nullptr)
                return true;

            buildUi(ctx, state);
            return root != UI_INVALID_HANDLE;
        }

        void buildUi(EngineToolContext& ctx, EngineToolStateComponent& state)
        {
            root            = createContainer(ctx.ui, UI_INVALID_HANDLE, {0.0f, 0.0f, 1.0f, 1.0f});
            topBar          = createRect(ctx.ui, root, {0.0f, 0.0f, 1.0f, 0.024f}, ToolTheme::barBackground);
            viewportToolbar = createRect(ctx.ui, root, {0.0f, 0.024f, 1.0f, 0.024f}, ToolTheme::viewportBar);
            leftRail        = createRect(ctx.ui, root, {0.0f, 0.024f, 0.030f, 0.954f}, ToolTheme::railBackground);
            rightPane       = createRect(ctx.ui, root, {0.830f, 0.024f, 0.170f, 0.954f}, ToolTheme::paneBackground);
            bottomBar       = createRect(ctx.ui, root, {0.0f, 0.978f, 1.0f, 0.022f}, ToolTheme::barBackground);
            title           = createText(ctx.ui,
                                         topBar,
                                         {ToolTheme::shellPadding, 0.004f, 0.100f, 0.016f},
                                         &font,
                                         "GRAVITAS",
                                         ToolTheme::text,
                                         ToolTheme::titleTextScale);
            viewportLabel   = createText(ctx.ui,
                                         viewportToolbar,
                                         {ToolTheme::shellPadding, 0.005f, 0.110f, 0.014f},
                                         &font,
                                         "VIEWPORT",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);
            viewportMode    = createText(ctx.ui,
                                         viewportToolbar,
                                         {0.110f, 0.005f, 0.300f, 0.014f},
                                         &font,
                                         "TOOL CAMERA",
                                         ToolTheme::statusText,
                                         ToolTheme::smallTextScale);
            panelTitle      = createText(ctx.ui,
                                         rightPane,
                                         {ToolTheme::shellPadding, 0.010f, 0.150f, 0.020f},
                                         &font,
                                         activePanelTitle(state),
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            panelSubtitle   = createText(ctx.ui,
                                         rightPane,
                                         {ToolTheme::shellPadding, 0.036f, 0.150f, 0.018f},
                                         &font,
                                         "EDITOR TOOLS",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            buildTabs(ctx, state);
            buildLeftTools(ctx);
            contentRoot =
                createContainer(ctx.ui, rightPane, {ToolTheme::shellPadding, PaneHeaderHeight, 0.150f, 0.850f});
            footer = createText(ctx.ui,
                                bottomBar,
                                {ToolTheme::shellPadding, 0.004f, 0.820f, 0.014f},
                                &font,
                                state.status,
                                ToolTheme::statusText,
                                ToolTheme::smallTextScale);

            for (auto& panel : registry.all())
                panel->build(ctx, contentRoot, &font);

            state.activePanelIndex =
                std::min(state.activePanelIndex, registry.empty() ? size_t{0} : registry.size() - size_t{1});
            for (size_t i = 0; i < registry.size(); ++i)
            {
                if (EngineToolPanel* panel = registry.at(i))
                    panel->setVisible(ctx.ui, i == state.activePanelIndex);
            }
        }

        void buildTabs(EngineToolContext& ctx, const EngineToolStateComponent&)
        {
            tabs.clear();
            if (registry.empty())
                return;

            for (size_t i = 0; i < registry.size(); ++i)
            {
                const float x = TopTabX + static_cast<float>(i) * (TopTabWidth + TopTabGap);
                tabs.push_back(createButton(ctx.ui,
                                            topBar,
                                            {x, TopTabY, TopTabWidth, TopTabHeight},
                                            &font,
                                            tabLabel(i, EngineToolStateComponent{}),
                                            ToolTheme::smallTextScale));
            }
        }

        void buildLeftTools(EngineToolContext& ctx)
        {
            leftTools.clear();
            const char* labels[] = {"E", "G", "C", "P", "X", "D", "A", "S"};
            float       y        = 0.012f;
            for (const char* label : labels)
            {
                leftTools.push_back(createButton(
                    ctx.ui, leftRail, {0.004f, y, 0.015f, 0.030f}, &font, label, ToolTheme::railIconScale));
                y += 0.038f;
            }
        }

        void syncLayout(EngineToolContext& ctx, const EngineToolWorkspaceComponent& workspace)
        {
            const bool  widePanel    = activePanelWantsWorkspace(ctx.world);
            const float paneX        = widePanel ? workspace.leftRailWidth : 1.0f - workspace.rightPaneWidth;
            const float paneWidth    = widePanel ? 1.0f - workspace.leftRailWidth : workspace.rightPaneWidth;
            const float chromeHeight = 1.0f - workspace.topBarHeight - workspace.bottomBarHeight;

            setRect(ctx.ui, topBar, {0.0f, 0.0f, 1.0f, workspace.topBarHeight});
            setRect(ctx.ui,
                    viewportToolbar,
                    {workspace.leftRailWidth,
                     workspace.topBarHeight,
                     workspace.viewportWidth,
                     workspace.viewportToolbarHeight});
            setRect(ctx.ui, leftRail, {0.0f, workspace.topBarHeight, workspace.leftRailWidth, chromeHeight});
            setRect(ctx.ui, rightPane, {paneX, workspace.topBarHeight, paneWidth, chromeHeight});
            setRect(ctx.ui, bottomBar, {0.0f, 1.0f - workspace.bottomBarHeight, 1.0f, workspace.bottomBarHeight});

            const float contentHeight = std::max(0.05f, chromeHeight - PaneHeaderHeight - PaneFooterReserve);
            setRect(ctx.ui,
                    contentRoot,
                    {ToolTheme::shellPadding,
                     PaneHeaderHeight,
                     std::max(0.05f, paneWidth - ToolTheme::shellPadding * 2.0f),
                     contentHeight});
            setRect(
                ctx.ui,
                panelTitle,
                {ToolTheme::shellPadding, 0.010f, paneWidth - ToolTheme::shellPadding * 2.0f, 0.020f});
            setRect(
                ctx.ui,
                panelSubtitle,
                {ToolTheme::shellPadding, 0.036f, paneWidth - ToolTheme::shellPadding * 2.0f, 0.018f});

            float y = 0.012f;
            for (ToolButton& tool : leftTools)
            {
                const float xPad        = std::min(0.004f, workspace.leftRailWidth * 0.18f);
                const float buttonWidth = std::max(0.010f, workspace.leftRailWidth - xPad * 2.0f);
                setRect(ctx.ui, tool.rect, {xPad, y, buttonWidth, 0.030f});
                y += 0.038f;
            }
        }

        void destroyUi(UiSystem& ui)
        {
            for (auto& panel : registry.all())
                panel->destroy(ui);

            if (root != UI_INVALID_HANDLE)
                ui.removeNode(root);

            root            = UI_INVALID_HANDLE;
            topBar          = UI_INVALID_HANDLE;
            viewportToolbar = UI_INVALID_HANDLE;
            leftRail        = UI_INVALID_HANDLE;
            rightPane       = UI_INVALID_HANDLE;
            bottomBar       = UI_INVALID_HANDLE;
            title           = UI_INVALID_HANDLE;
            viewportLabel   = UI_INVALID_HANDLE;
            viewportMode    = UI_INVALID_HANDLE;
            panelTitle      = UI_INVALID_HANDLE;
            panelSubtitle   = UI_INVALID_HANDLE;
            contentRoot     = UI_INVALID_HANDLE;
            footer          = UI_INVALID_HANDLE;
            tabs.clear();
            leftTools.clear();
        }

        UiInteractionResult updateInteraction(const EcsControllerContext& ctx, const EngineToolStateComponent&)
        {
            if (ctx.input == nullptr || ctx.ui == nullptr)
                return {};

            const float width  = std::max(1.0f, ctx.windowPixelWidth);
            const float height = std::max(1.0f, ctx.windowPixelHeight);

            UiInputFrame frame;
            frame.pointerX = std::clamp(static_cast<float>(ctx.input->mouseX()) / width, 0.0f, 1.0f);
            frame.pointerY = std::clamp(static_cast<float>(ctx.input->mouseY()) / height, 0.0f, 1.0f);
            const char* primaryAction =
                ctx.input->isContextActive(ToolsInputContext) ? ToolsSelectAction : "engine.ui_primary";
            frame.primaryDown     = ctx.input->isHeld(primaryAction);
            frame.primaryPressed  = ctx.input->isPressed(primaryAction);
            frame.primaryReleased = ctx.input->isReleased(primaryAction);
            frame.scrollX         = static_cast<float>(ctx.input->scrollX());
            frame.scrollY         = static_cast<float>(ctx.input->scrollY());
            return ctx.ui->updateInteraction(frame);
        }

        void updateInputCapture(ECSWorld& world,
                                const EngineToolStateComponent&,
                                const UiInteractionResult&  interaction,
                                const EcsControllerContext& ctx)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            capture.pointerOverToolUi                = interaction.hovered != UI_INVALID_HANDLE;
            capture.toolUiPressed                    = interaction.pressed != UI_INVALID_HANDLE;
            capture.keyboardCaptured                 = false;
            capture.worldConsumed                    = false;
            capture.pointerX                         = interaction.pointerX;
            capture.pointerY                         = interaction.pointerY;
            capture.hoveredUi                        = interaction.hovered;
            capture.pressedUi                        = interaction.pressed;

            if (ctx.input == nullptr)
            {
                capture.primaryDown         = false;
                capture.primaryPressed      = false;
                capture.primaryReleased     = false;
                capture.pointerOverViewport = false;
                capture.viewportPointerX    = 0.0f;
                capture.viewportPointerY    = 0.0f;
                return;
            }

            RenderViewportRect viewport =
                RenderViewportRect::full(std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth))),
                                         std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight))));
            if (world.hasAny<RenderViewportComponent>())
                viewport = world.getSingleton<RenderViewportComponent>().sceneViewport;
            capture.pointerOverViewport = viewport.remapPointer(static_cast<float>(ctx.input->mouseX()),
                                                                static_cast<float>(ctx.input->mouseY()),
                                                                capture.viewportPointerX,
                                                                capture.viewportPointerY);

            const char* primaryAction =
                ctx.input->isContextActive(ToolsInputContext) ? ToolsSelectAction : "engine.ui_primary";
            capture.primaryDown     = ctx.input->isHeld(primaryAction);
            capture.primaryPressed  = ctx.input->isPressed(primaryAction);
            capture.primaryReleased = ctx.input->isReleased(primaryAction);
        }

        void updateTabs(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (size_t i = 0; i < tabs.size(); ++i)
            {
                if (wasClicked(interaction, tabs[i].rect))
                {
                    state.activePanelIndex       = i;
                    const EngineToolPanel* panel = registry.at(i);
                    state.status =
                        "ACTIVE PANEL " + (panel == nullptr ? std::string("PANEL") : std::string(panel->title()));
                }

                updateButton(ctx.ui, tabs[i], tabLabel(i, state));
                if (i == state.activePanelIndex)
                    setRectColor(ctx.ui, tabs[i].rect, ToolTheme::buttonActive);
            }

            updateLeftTools(ctx, state, interaction);
        }

        void
        updateLeftTools(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (size_t i = 0; i < leftTools.size(); ++i)
            {
                if (wasClicked(interaction, leftTools[i].rect) && i < registry.size())
                {
                    state.activePanelIndex       = i;
                    const EngineToolPanel* panel = registry.at(i);
                    state.status =
                        "ACTIVE PANEL " + (panel == nullptr ? std::string("PANEL") : std::string(panel->title()));
                }

                updateButton(ctx.ui, leftTools[i], leftTools[i].text);
                if (i == state.activePanelIndex)
                    setRectColor(ctx.ui, leftTools[i].rect, ToolTheme::buttonActive);
            }
        }

        std::string tabLabel(size_t index, const EngineToolStateComponent&) const
        {
            const EngineToolPanel* panel = registry.at(index);
            if (panel == nullptr)
                return "PANEL";
            if (panel->id() == "gizmos")
                return "Gizmo";
            if (panel->id() == "particles")
                return "FX";
            if (panel->id() == "particle_effects")
                return "PFX";
            return std::string(panel->title());
        }

        std::string activePanelTitle(const EngineToolStateComponent& state) const
        {
            if (registry.empty())
                return "Tools";

            const EngineToolPanel* panel = registry.at(std::min(state.activePanelIndex, registry.size() - 1));
            if (panel == nullptr)
                return "Tools";
            return std::string(panel->title());
        }

        bool activePanelWantsWorkspace(ECSWorld& world) const
        {
            if (!world.hasAny<EngineToolStateComponent>() || registry.empty())
                return false;

            const EngineToolStateComponent& state = world.getSingleton<EngineToolStateComponent>();
            const EngineToolPanel*          panel = registry.at(std::min(state.activePanelIndex, registry.size() - 1));
            return panel != nullptr && panel->id() == "particle_effects";
        }

        void syncPreviewCameraRequest(ECSWorld& world) const
        {
            if (activePanelWantsWorkspace(world) || !world.hasAny<EngineToolPreviewCameraComponent>())
                return;

            clearPreviewCameraRequest(world);
        }

        void clearPreviewCameraRequest(ECSWorld& world) const
        {
            if (!world.hasAny<EngineToolPreviewCameraComponent>())
                return;

            world.getSingleton<EngineToolPreviewCameraComponent>().active = false;
        }
    };
} // namespace gts::tools
