#pragma once

#include <array>
#include <string_view>

#include "DebugDrawPrimitives.h"
#include "DebugDrawSettingsComponent.h"
#include "EngineToolPanel.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    class DebugDrawPanel : public EngineToolPanel
    {
    public:
        std::string_view id() const override { return "debugdraw"; }
        std::string_view title() const override { return "Debug"; }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root = createContainer(ctx.ui, parent, {0.0f, 0.0f, 0.390f, 0.760f});
            header = createText(ctx.ui, root, {0.014f, 0.006f, 0.300f, 0.030f}, font,
                                "DEBUG DRAW", color(0.95f, 0.98f, 1.0f), 0.016f);
            summary = createText(ctx.ui, root, {0.014f, 0.038f, 0.350f, 0.028f}, font,
                                 "VISUAL OVERLAYS", color(0.66f, 0.74f, 0.80f), 0.0125f);

            toggles[0] = createButton(ctx.ui, root, {0.014f, 0.082f, 0.170f, 0.034f}, font, "DRAW", 0.0125f);
            toggles[1] = createButton(ctx.ui, root, {0.198f, 0.082f, 0.170f, 0.034f}, font, "SELECT", 0.0125f);
            toggles[2] = createButton(ctx.ui, root, {0.014f, 0.126f, 0.170f, 0.034f}, font, "ALL", 0.0125f);
            toggles[3] = createButton(ctx.ui, root, {0.198f, 0.126f, 0.170f, 0.034f}, font, "AXES", 0.0125f);
            toggles[4] = createButton(ctx.ui, root, {0.014f, 0.170f, 0.170f, 0.034f}, font, "FRUSTUM", 0.0125f);
            toggles[5] = createButton(ctx.ui, root, {0.198f, 0.170f, 0.170f, 0.034f}, font, "RAY", 0.0125f);

            thicknessSlider = createSlider(ctx.ui, root, 0.238f, "THICK", 0.008f, 0.120f, false, font);
            axisSlider = createSlider(ctx.ui, root, 0.290f, "AXIS LEN", 0.25f, 4.0f, false, font);
            raySlider = createSlider(ctx.ui, root, 0.342f, "RAY LEN", 2.0f, 80.0f, false, font);

            footer = createText(ctx.ui, root, {0.014f, 0.724f, 0.350f, 0.024f}, font,
                                "DEBUG DRAW QUEUE", color(0.80f, 0.86f, 0.92f), 0.0125f);
        }

        void update(EngineToolContext& ctx,
                    EngineToolStateComponent& state,
                    const UiInteractionResult& interaction) override
        {
            auto& settings = gts::debugdraw::ensureSettings(ctx.world);
            applyToggles(settings, state, interaction);
            applySliders(ctx.ui, settings, interaction);
            updateDisplay(ctx.ui, settings, state);
        }

        void setVisible(UiSystem& ui, bool visible) override
        {
            setVisibleRecursive(ui, root, visible);
        }

        void destroy(UiSystem& ui) override
        {
            if (root != UI_INVALID_HANDLE)
                ui.removeNode(root);
            root = UI_INVALID_HANDLE;
        }

    private:
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle header = UI_INVALID_HANDLE;
        UiHandle summary = UI_INVALID_HANDLE;
        UiHandle footer = UI_INVALID_HANDLE;
        std::array<ToolButton, 6> toggles{};
        ToolSlider thicknessSlider;
        ToolSlider axisSlider;
        ToolSlider raySlider;

        void applyToggles(gts::debugdraw::DebugDrawSettingsComponent& settings,
                          EngineToolStateComponent& state,
                          const UiInteractionResult& interaction)
        {
            if (wasClicked(interaction, toggles[0].rect)) settings.enabled = !settings.enabled;
            if (wasClicked(interaction, toggles[1].rect)) settings.selectedBounds = !settings.selectedBounds;
            if (wasClicked(interaction, toggles[2].rect)) settings.allBounds = !settings.allBounds;
            if (wasClicked(interaction, toggles[3].rect)) settings.transformAxes = !settings.transformAxes;
            if (wasClicked(interaction, toggles[4].rect)) settings.cameraFrustum = !settings.cameraFrustum;
            if (wasClicked(interaction, toggles[5].rect)) settings.pickRay = !settings.pickRay;

            if (interaction.clicked != UI_INVALID_HANDLE)
                state.status = "DEBUG DRAW UPDATED";
        }

        void applySliders(UiSystem& ui,
                          gts::debugdraw::DebugDrawSettingsComponent& settings,
                          const UiInteractionResult& interaction)
        {
            if (isPressed(interaction, thicknessSlider.track))
                settings.lineThickness = valueFromSliderPointer(ui, thicknessSlider, interaction.pointerX);
            if (isPressed(interaction, axisSlider.track))
                settings.axisLength = valueFromSliderPointer(ui, axisSlider, interaction.pointerX);
            if (isPressed(interaction, raySlider.track))
                settings.pickRayLength = valueFromSliderPointer(ui, raySlider, interaction.pointerX);
        }

        void updateDisplay(UiSystem& ui,
                           const gts::debugdraw::DebugDrawSettingsComponent& settings,
                           const EngineToolStateComponent& state)
        {
            updateToggleButton(ui, toggles[0], "DRAW", settings.enabled);
            updateToggleButton(ui, toggles[1], "SELECT", settings.selectedBounds);
            updateToggleButton(ui, toggles[2], "ALL", settings.allBounds);
            updateToggleButton(ui, toggles[3], "AXES", settings.transformAxes);
            updateToggleButton(ui, toggles[4], "FRUSTUM", settings.cameraFrustum);
            updateToggleButton(ui, toggles[5], "RAY", settings.pickRay);

            updateSlider(ui, thicknessSlider, settings.lineThickness, color(0.90f, 0.82f, 0.24f, 1.0f));
            updateSlider(ui, axisSlider, settings.axisLength, color(0.33f, 0.72f, 0.88f, 1.0f));
            updateSlider(ui, raySlider, settings.pickRayLength, color(0.55f, 0.62f, 0.88f, 1.0f));
            setText(ui, footer, state.status);
        }
    };
}
