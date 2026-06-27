#pragma once

#include <array>
#include <string_view>

#include "DebugDrawPrimitives.h"
#include "DebugDrawSettingsComponent.h"
#include "EngineToolPanel.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    class DebugDrawPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "debugdraw";
        }
        std::string_view title() const override
        {
            return "Debug";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root    = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header  = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.000f, 1.0f, 0.036f},
                                         font,
                                         "DEBUG DRAW",
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.042f, 1.0f, 0.036f},
                                         font,
                                         "VISUAL OVERLAYS",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            toggles[0] = createButtonRelative(
                ctx.ui, root, {0.000f, 0.092f, 0.480f, 0.042f}, font, "DRAW", ToolTheme::buttonTextScale);
            toggles[1] = createButtonRelative(
                ctx.ui, root, {0.520f, 0.092f, 0.480f, 0.042f}, font, "SELECT", ToolTheme::buttonTextScale);
            toggles[2] = createButtonRelative(
                ctx.ui, root, {0.000f, 0.144f, 0.480f, 0.042f}, font, "ALL", ToolTheme::buttonTextScale);
            toggles[3] = createButtonRelative(
                ctx.ui, root, {0.520f, 0.144f, 0.480f, 0.042f}, font, "AXES", ToolTheme::buttonTextScale);
            toggles[4] = createButtonRelative(
                ctx.ui, root, {0.000f, 0.196f, 0.480f, 0.042f}, font, "FRUSTUM", ToolTheme::buttonTextScale);
            toggles[5] = createButtonRelative(
                ctx.ui, root, {0.520f, 0.196f, 0.480f, 0.042f}, font, "RAY", ToolTheme::buttonTextScale);

            thicknessSlider = createSlider(ctx.ui, root, 0.272f, "THICK", 0.008f, 0.120f, false, font);
            axisSlider      = createSlider(ctx.ui, root, 0.324f, "AXIS LEN", 0.25f, 4.0f, false, font);
            raySlider       = createSlider(ctx.ui, root, 0.376f, "RAY LEN", 2.0f, 80.0f, false, font);

            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.940f, 1.0f, 0.040f},
                                        font,
                                        "DEBUG DRAW QUEUE",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
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
        UiHandle                  root    = UI_INVALID_HANDLE;
        UiHandle                  header  = UI_INVALID_HANDLE;
        UiHandle                  summary = UI_INVALID_HANDLE;
        UiHandle                  footer  = UI_INVALID_HANDLE;
        std::array<ToolButton, 6> toggles{};
        ToolSlider                thicknessSlider;
        ToolSlider                axisSlider;
        ToolSlider                raySlider;

        void applyToggles(gts::debugdraw::DebugDrawSettingsComponent& settings,
                          EngineToolStateComponent&                   state,
                          const UiInteractionResult&                  interaction)
        {
            if (wasClicked(interaction, toggles[0].rect))
                settings.enabled = !settings.enabled;
            if (wasClicked(interaction, toggles[1].rect))
                settings.selectedBounds = !settings.selectedBounds;
            if (wasClicked(interaction, toggles[2].rect))
                settings.allBounds = !settings.allBounds;
            if (wasClicked(interaction, toggles[3].rect))
                settings.transformAxes = !settings.transformAxes;
            if (wasClicked(interaction, toggles[4].rect))
                settings.cameraFrustum = !settings.cameraFrustum;
            if (wasClicked(interaction, toggles[5].rect))
                settings.pickRay = !settings.pickRay;

            if (interaction.clicked != UI_INVALID_HANDLE)
                state.status = "DEBUG DRAW UPDATED";
        }

        void applySliders(UiSystem&                                   ui,
                          gts::debugdraw::DebugDrawSettingsComponent& settings,
                          const UiInteractionResult&                  interaction)
        {
            if (isPressed(interaction, thicknessSlider.track))
                settings.lineThickness = valueFromSliderPointer(ui, thicknessSlider, interaction.pointerX);
            if (isPressed(interaction, axisSlider.track))
                settings.axisLength = valueFromSliderPointer(ui, axisSlider, interaction.pointerX);
            if (isPressed(interaction, raySlider.track))
                settings.pickRayLength = valueFromSliderPointer(ui, raySlider, interaction.pointerX);
        }

        void updateDisplay(UiSystem&                                         ui,
                           const gts::debugdraw::DebugDrawSettingsComponent& settings,
                           const EngineToolStateComponent&                   state)
        {
            updateToggleButton(ui, toggles[0], "DRAW", settings.enabled);
            updateToggleButton(ui, toggles[1], "SELECT", settings.selectedBounds);
            updateToggleButton(ui, toggles[2], "ALL", settings.allBounds);
            updateToggleButton(ui, toggles[3], "AXES", settings.transformAxes);
            updateToggleButton(ui, toggles[4], "FRUSTUM", settings.cameraFrustum);
            updateToggleButton(ui, toggles[5], "RAY", settings.pickRay);

            updateSlider(ui, thicknessSlider, settings.lineThickness, ToolTheme::warning);
            updateSlider(ui, axisSlider, settings.axisLength, ToolTheme::info);
            updateSlider(ui, raySlider, settings.pickRayLength, ToolTheme::secondaryAccent);
            setText(ui, footer, state.status);
        }
    };
} // namespace gts::tools
