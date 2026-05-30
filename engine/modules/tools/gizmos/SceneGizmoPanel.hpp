#pragma once

#include <string>
#include <string_view>

#include "EngineGizmoStateComponent.h"
#include "EngineToolPanel.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    class SceneGizmoPanel : public EngineToolPanel
    {
    public:
        std::string_view id() const override { return "gizmos"; }
        std::string_view title() const override { return "Gizmos"; }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root = createContainer(ctx.ui, parent, {0.0f, 0.0f, 0.390f, 0.760f});
            header = createText(ctx.ui, root, {0.014f, 0.006f, 0.300f, 0.030f}, font,
                                "SCENE GIZMO", color(0.95f, 0.98f, 1.0f), 0.016f);
            summary = createText(ctx.ui, root, {0.014f, 0.038f, 0.350f, 0.028f}, font,
                                 "TRANSLATE TOOL", color(0.66f, 0.74f, 0.80f), 0.0125f);

            enabledButton = createButton(ctx.ui, root, {0.014f, 0.082f, 0.112f, 0.034f}, font, "ON", 0.0125f);
            spaceButton = createButton(ctx.ui, root, {0.140f, 0.082f, 0.112f, 0.034f}, font, "WORLD", 0.0125f);
            snapButton = createButton(ctx.ui, root, {0.266f, 0.082f, 0.102f, 0.034f}, font, "SNAP", 0.0125f);

            lengthSlider = createSlider(ctx.ui, root, 0.146f, "LENGTH", 0.35f, 4.0f, false, font);
            snapSlider = createSlider(ctx.ui, root, 0.198f, "SNAP", 0.05f, 2.0f, false, font);
            radiusSlider = createSlider(ctx.ui, root, 0.250f, "PICK", 0.03f, 0.40f, false, font);

            footer = createText(ctx.ui, root, {0.014f, 0.724f, 0.350f, 0.024f}, font,
                                "GIZMO READY", color(0.80f, 0.86f, 0.92f), 0.0125f);
        }

        void update(EngineToolContext& ctx,
                    EngineToolStateComponent& state,
                    const UiInteractionResult& interaction) override
        {
            EngineGizmoStateComponent& gizmo = ensureGizmoState(ctx.world);

            if (wasClicked(interaction, enabledButton.rect))
            {
                gizmo.enabled = !gizmo.enabled;
                state.status = gizmo.enabled ? "GIZMO ENABLED" : "GIZMO DISABLED";
            }
            if (wasClicked(interaction, spaceButton.rect))
            {
                gizmo.space = gizmo.space == EngineGizmoSpace::World
                    ? EngineGizmoSpace::Local
                    : EngineGizmoSpace::World;
                state.status = gizmo.space == EngineGizmoSpace::World ? "GIZMO WORLD SPACE" : "GIZMO LOCAL SPACE";
            }
            if (wasClicked(interaction, snapButton.rect))
            {
                gizmo.snapEnabled = !gizmo.snapEnabled;
                state.status = gizmo.snapEnabled ? "GIZMO SNAP ON" : "GIZMO SNAP OFF";
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
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle header = UI_INVALID_HANDLE;
        UiHandle summary = UI_INVALID_HANDLE;
        UiHandle footer = UI_INVALID_HANDLE;
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
                case EngineGizmoAxis::X: return "X";
                case EngineGizmoAxis::Y: return "Y";
                case EngineGizmoAxis::Z: return "Z";
                case EngineGizmoAxis::None: break;
            }
            return "-";
        }

        void updateDisplay(UiSystem& ui,
                           const EngineGizmoStateComponent& gizmo,
                           const EngineToolStateComponent& state)
        {
            updateToggleButton(ui, enabledButton, "GIZMO", gizmo.enabled);
            updateButton(ui, spaceButton, gizmo.space == EngineGizmoSpace::World ? "WORLD" : "LOCAL");
            updateToggleButton(ui, snapButton, "SNAP", gizmo.snapEnabled);

            updateSlider(ui, lengthSlider, gizmo.handleLength, color(0.88f, 0.28f, 0.30f, 1.0f));
            updateSlider(ui, snapSlider, gizmo.snapStep, color(0.80f, 0.58f, 0.24f, 1.0f));
            updateSlider(ui, radiusSlider, gizmo.pickRadius, color(0.25f, 0.52f, 0.92f, 1.0f));

            if (gizmo.activeAxis != EngineGizmoAxis::None)
                setText(ui, footer, "DRAG " + axisName(gizmo.activeAxis));
            else if (gizmo.hoveredAxis != EngineGizmoAxis::None)
                setText(ui, footer, "HOVER " + axisName(gizmo.hoveredAxis));
            else
                setText(ui, footer, state.status);
        }
    };
}
