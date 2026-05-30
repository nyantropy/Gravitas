#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "AssetStatusPanel.hpp"
#include "BitmapFontLoader.h"
#include "DebugDrawPanel.hpp"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolContext.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolRegistry.h"
#include "EntityInspectorPanel.hpp"
#include "GravitasFontAtlas.h"
#include "GraphicsConstants.h"
#include "ParticleEmitterInspectorPanel.hpp"
#include "SceneGizmoPanel.hpp"
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
            registry.addPanel<ParticleEmitterInspectorPanel>();
            registry.addPanel<DebugDrawPanel>();
            registry.addPanel<AssetStatusPanel>();
        }

        void update(const EcsControllerContext& ctx) override
        {
            if (ctx.ui == nullptr)
                return;

            EngineToolStateComponent& state = ensureState(ctx.world);
            state.selectionChangedThisFrame = false;
            if (ctx.input != nullptr && ctx.input->isPressed("engine.tools_toggle"))
            {
                state.visible = !state.visible;
                setToolsInputContext(ctx, state.visible);
                if (!state.visible)
                {
                    clearInputCapture(ctx.world);
                    destroyUi(*ctx.ui);
                }
            }

            if (state.visible)
                setToolsInputContext(ctx, true);

            if (!state.visible)
            {
                setToolsInputContext(ctx, false);
                clearInputCapture(ctx.world);
                return;
            }

            if (!ensureFont(ctx))
                return;

            EngineToolContext toolCtx = EngineToolContext::fromController(ctx);
            if (!ensureUi(toolCtx, state))
                return;

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
        }

    private:
        static constexpr float RootX = 0.018f;
        static constexpr float RootY = 0.055f;
        static constexpr float RootW = 0.500f;
        static constexpr float RootH = 0.925f;
        static constexpr float Pad = 0.014f;

        EngineToolRegistry registry;
        BitmapFont font;
        bool fontReady = false;
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle background = UI_INVALID_HANDLE;
        UiHandle title = UI_INVALID_HANDLE;
        UiHandle contentRoot = UI_INVALID_HANDLE;
        UiHandle footer = UI_INVALID_HANDLE;
        std::vector<ToolButton> tabs;
        bool toolsContextRequested = false;

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
            capture = {};
        }

        bool ensureFont(const EcsControllerContext& ctx)
        {
            if (fontReady)
                return true;
            if (ctx.resources == nullptr)
                return false;

            font = BitmapFontLoader::load(
                ctx.resources,
                GraphicsConstants::ENGINE_RESOURCES + "/fonts/gravitasfont.png",
                GravitasFontAtlas::ATLAS_W,
                GravitasFontAtlas::ATLAS_H,
                GravitasFontAtlas::CELL_W,
                GravitasFontAtlas::CELL_H,
                GravitasFontAtlas::ATLAS_COLS,
                std::string(GravitasFontAtlas::CHAR_ORDER),
                GravitasFontAtlas::LINE_HEIGHT,
                true);
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
            root = createContainer(ctx.ui, UI_INVALID_HANDLE, {RootX, RootY, RootW, RootH});
            background = createRect(ctx.ui, root, {0.0f, 0.0f, RootW, RootH},
                                    color(0.022f, 0.027f, 0.033f, 0.94f));
            title = createText(ctx.ui, root, {Pad, Pad, 0.250f, 0.032f}, &font,
                               "GRAVITAS TOOLS", color(0.95f, 0.98f, 1.0f), 0.018f);

            buildTabs(ctx, state);
            contentRoot = createContainer(ctx.ui, root, {Pad, 0.128f, 0.470f, 0.760f});
            footer = createText(ctx.ui, root, {Pad, RootH - 0.035f, 0.462f, 0.024f}, &font,
                                state.status, color(0.80f, 0.86f, 0.92f), 0.0125f);

            for (auto& panel : registry.all())
                panel->build(ctx, contentRoot, &font);

            state.activePanelIndex = std::min(
                state.activePanelIndex,
                registry.empty() ? size_t{0} : registry.size() - size_t{1});
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

            const float gap = 0.007f;
            const float tabWidth = (RootW - Pad * 2.0f - gap * static_cast<float>(registry.size() - 1))
                / static_cast<float>(registry.size());
            for (size_t i = 0; i < registry.size(); ++i)
            {
                const EngineToolPanel* panel = registry.at(i);
                const std::string label = panel == nullptr ? "PANEL" : std::string(panel->title());
                const float x = Pad + static_cast<float>(i) * (tabWidth + gap);
                tabs.push_back(createButton(ctx.ui,
                                            root,
                                            {x, 0.074f, tabWidth, 0.034f},
                                            &font,
                                            label,
                                            0.0125f));
            }
        }

        void destroyUi(UiSystem& ui)
        {
            for (auto& panel : registry.all())
                panel->destroy(ui);

            if (root != UI_INVALID_HANDLE)
                ui.removeNode(root);

            root = UI_INVALID_HANDLE;
            background = UI_INVALID_HANDLE;
            title = UI_INVALID_HANDLE;
            contentRoot = UI_INVALID_HANDLE;
            footer = UI_INVALID_HANDLE;
            tabs.clear();
        }

        UiInteractionResult updateInteraction(const EcsControllerContext& ctx,
                                              const EngineToolStateComponent&)
        {
            if (ctx.input == nullptr || ctx.ui == nullptr)
                return {};

            const float width = std::max(1.0f, ctx.windowPixelWidth);
            const float height = std::max(1.0f, ctx.windowPixelHeight);

            UiInputFrame frame;
            frame.pointerX = std::clamp(static_cast<float>(ctx.input->mouseX()) / width, 0.0f, 1.0f);
            frame.pointerY = std::clamp(static_cast<float>(ctx.input->mouseY()) / height, 0.0f, 1.0f);
            const char* primaryAction = ctx.input->isContextActive(ToolsInputContext)
                ? ToolsSelectAction
                : "engine.ui_primary";
            frame.primaryDown = ctx.input->isHeld(primaryAction);
            frame.primaryPressed = ctx.input->isPressed(primaryAction);
            frame.primaryReleased = ctx.input->isReleased(primaryAction);
            frame.scrollX = static_cast<float>(ctx.input->scrollX());
            frame.scrollY = static_cast<float>(ctx.input->scrollY());
            return ctx.ui->updateInteraction(frame);
        }

        void updateInputCapture(ECSWorld& world,
                                const EngineToolStateComponent&,
                                const UiInteractionResult& interaction,
                                const EcsControllerContext& ctx)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            capture.pointerOverToolUi = interaction.hovered != UI_INVALID_HANDLE;
            capture.toolUiPressed = interaction.pressed != UI_INVALID_HANDLE;
            capture.worldConsumed = false;
            capture.pointerX = interaction.pointerX;
            capture.pointerY = interaction.pointerY;
            capture.hoveredUi = interaction.hovered;
            capture.pressedUi = interaction.pressed;

            if (ctx.input == nullptr)
            {
                capture.primaryDown = false;
                capture.primaryPressed = false;
                capture.primaryReleased = false;
                return;
            }

            const char* primaryAction = ctx.input->isContextActive(ToolsInputContext)
                ? ToolsSelectAction
                : "engine.ui_primary";
            capture.primaryDown = ctx.input->isHeld(primaryAction);
            capture.primaryPressed = ctx.input->isPressed(primaryAction);
            capture.primaryReleased = ctx.input->isReleased(primaryAction);
        }

        void updateTabs(EngineToolContext& ctx,
                        EngineToolStateComponent& state,
                        const UiInteractionResult& interaction)
        {
            for (size_t i = 0; i < tabs.size(); ++i)
            {
                if (wasClicked(interaction, tabs[i].rect))
                {
                    state.activePanelIndex = i;
                    const EngineToolPanel* panel = registry.at(i);
                    state.status = "ACTIVE PANEL " + (panel == nullptr
                        ? std::string("PANEL")
                        : std::string(panel->title()));
                }

                updateButton(ctx.ui, tabs[i], tabLabel(i, state));
                if (i == state.activePanelIndex)
                    setRectColor(ctx.ui, tabs[i].rect, color(0.20f, 0.30f, 0.38f, 1.0f));
            }
        }

        std::string tabLabel(size_t index, const EngineToolStateComponent&) const
        {
            const EngineToolPanel* panel = registry.at(index);
            if (panel == nullptr)
                return "PANEL";
            return std::string(panel->title());
        }
    };
}
