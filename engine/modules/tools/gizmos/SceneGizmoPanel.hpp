#pragma once

#include <string>
#include <string_view>

#include "EngineGizmoStateComponent.h"
#include "EngineToolPanel.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    class SceneGizmoPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "gizmos";
        }
        std::string_view title() const override
        {
            return "Gizmos";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root    = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header  = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.000f, 1.0f, 0.036f},
                                         font,
                                         "SCENE GIZMO",
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.042f, 1.0f, 0.036f},
                                         font,
                                         "TRANSLATE TOOL",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            enabledButton = createButtonRelative(
                ctx.ui, root, {0.000f, 0.092f, 0.310f, 0.044f}, font, "ON", ToolTheme::buttonTextScale);
            spaceButton = createButtonRelative(
                ctx.ui, root, {0.345f, 0.092f, 0.310f, 0.044f}, font, "WORLD", ToolTheme::buttonTextScale);
            snapButton = createButtonRelative(
                ctx.ui, root, {0.690f, 0.092f, 0.310f, 0.044f}, font, "SNAP", ToolTheme::buttonTextScale);

            lengthSlider = createSlider(ctx.ui, root, 0.170f, "LENGTH", 0.35f, 4.0f, false, font);
            snapSlider   = createSlider(ctx.ui, root, 0.222f, "SNAP", 0.05f, 2.0f, false, font);
            radiusSlider = createSlider(ctx.ui, root, 0.274f, "PICK", 0.03f, 0.40f, false, font);

            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.940f, 1.0f, 0.040f},
                                        font,
                                        "GIZMO READY",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            EngineGizmoStateComponent& gizmo = ensureGizmoState(ctx.world);

            if (wasClicked(interaction, enabledButton.rect))
            {
                gizmo.enabled = !gizmo.enabled;
                state.status  = gizmo.enabled ? "GIZMO ENABLED" : "GIZMO DISABLED";
            }
            if (wasClicked(interaction, spaceButton.rect))
            {
                gizmo.space =
                    gizmo.space == EngineGizmoSpace::World ? EngineGizmoSpace::Local : EngineGizmoSpace::World;
                state.status = gizmo.space == EngineGizmoSpace::World ? "GIZMO WORLD SPACE" : "GIZMO LOCAL SPACE";
            }
            if (wasClicked(interaction, snapButton.rect))
            {
                gizmo.snapEnabled = !gizmo.snapEnabled;
                state.status      = gizmo.snapEnabled ? "GIZMO SNAP ON" : "GIZMO SNAP OFF";
            }

            if (isPressed(interaction, lengthSlider.track))
                gizmo.handleLength = valueFromSliderPointer(ctx.ui, lengthSlider, interaction.pointerX);
            if (isPressed(interaction, snapSlider.track))
                gizmo.snapStep = valueFromSliderPointer(ctx.ui, snapSlider, interaction.pointerX);
            if (isPressed(interaction, radiusSlider.track))
                gizmo.pickRadius = valueFromSliderPointer(ctx.ui, radiusSlider, interaction.pointerX);

            updateDisplay(ctx.ui, gizmo, state);
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
        UiHandle   root    = UI_INVALID_HANDLE;
        UiHandle   header  = UI_INVALID_HANDLE;
        UiHandle   summary = UI_INVALID_HANDLE;
        UiHandle   footer  = UI_INVALID_HANDLE;
        ToolButton enabledButton;
        ToolButton spaceButton;
        ToolButton snapButton;
        ToolSlider lengthSlider;
        ToolSlider snapSlider;
        ToolSlider radiusSlider;

        static EngineGizmoStateComponent& ensureGizmoState(ECSWorld& world)
        {
            if (!world.hasAny<EngineGizmoStateComponent>())
                return world.createSingleton<EngineGizmoStateComponent>();
            return world.getSingleton<EngineGizmoStateComponent>();
        }

        static std::string axisName(EngineGizmoAxis axis)
        {
            switch (axis)
            {
            case EngineGizmoAxis::X:
                return "X";
            case EngineGizmoAxis::Y:
                return "Y";
            case EngineGizmoAxis::Z:
                return "Z";
            case EngineGizmoAxis::None:
                break;
            }
            return "-";
        }

        void updateDisplay(UiSystem& ui, const EngineGizmoStateComponent& gizmo, const EngineToolStateComponent& state)
        {
            updateToggleButton(ui, enabledButton, "GIZMO", gizmo.enabled);
            updateButton(ui, spaceButton, gizmo.space == EngineGizmoSpace::World ? "WORLD" : "LOCAL");
            updateToggleButton(ui, snapButton, "SNAP", gizmo.snapEnabled);

            updateSlider(ui, lengthSlider, gizmo.handleLength, ToolTheme::axisX);
            updateSlider(ui, snapSlider, gizmo.snapStep, ToolTheme::warning);
            updateSlider(ui, radiusSlider, gizmo.pickRadius, ToolTheme::axisZ);

            if (gizmo.activeAxis != EngineGizmoAxis::None)
                setText(ui, footer, "DRAG " + axisName(gizmo.activeAxis));
            else if (gizmo.hoveredAxis != EngineGizmoAxis::None)
                setText(ui, footer, "HOVER " + axisName(gizmo.hoveredAxis));
            else
                setText(ui, footer, state.status);
        }
    };
} // namespace gts::tools
