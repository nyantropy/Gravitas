#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "EngineToolPanel.h"
#include "EngineToolInputCaptureComponent.h"
#include "EditorLayout.h"
#include "EditorPropertySystem.h"
#include "EditorWidgets.h"
#include "GtsKey.h"
#include "InputBindingRegistry.h"
#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleFrameData.h"
#include "ParticleGraphAuthoring.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ParticlePreviewWorld.hpp"
#include "ParticleProgramCompiler.h"
#include "EditorPreviewRenderData.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"
#include "TransformComponent.h"

namespace gts::tools
{
    using gts::particles::buildModulesFromEmitterDescriptor;
    using gts::particles::executionStageLabel;
    using gts::particles::executionStageShortLabel;
    using gts::particles::findParticleModuleDefinition;
    using gts::particles::ParticleModuleDefinition;
    using gts::particles::ParticleModuleEnumOption;
    using gts::particles::ParticleModuleInstance;
    using gts::particles::ParticleModuleParameter;
    using gts::particles::ParticleModuleParameterDefinition;
    using gts::particles::ParticleModuleParameterType;

    class ParticleEffectEditorPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "particle_effects";
        }

        std::string_view title() const override
        {
            return "Particle FX";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root           = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            createRectRelative(ctx.ui, root, {0.0f, 0.0f, 1.0f, 1.0f}, ToolTheme::workspaceBackground);
            buildApplicationChrome(ctx.ui, root, font);
            toolbarFrame   = createFxPanelFrame(ctx.ui,
                                                root,
                                                {0.0f, 0.071f, 1.0f, 0.055f},
                                                font,
                                                "toolbar",
                                                "FX Editor",
                                                "Asset Browser",
                                                EditorDockArea::Top);
            hierarchyFrame = createFxPanelFrame(ctx.ui,
                                                root,
                                                {0.008f, 0.134f, 0.195f, 0.535f},
                                                font,
                                                "hierarchy",
                                                "Effect Hierarchy",
                                                "Effects / Emitters",
                                                EditorDockArea::Left);
            previewFrame   = createFxPanelFrame(ctx.ui,
                                                root,
                                                {0.210f, 0.134f, 0.545f, 0.535f},
                                                font,
                                                "preview",
                                                "Viewport",
                                                "Immediate Feedback",
                                                EditorDockArea::Center);
            inspectorFrame = createFxPanelFrame(ctx.ui,
                                                root,
                                                {0.762f, 0.134f, 0.230f, 0.535f},
                                                font,
                                                "inspector",
                                                "Inspector",
                                                "Selection",
                                                EditorDockArea::Right);
            workspaceFrame = createFxPanelFrame(ctx.ui,
                                                root,
                                                {0.008f, 0.677f, 0.984f, 0.290f},
                                                font,
                                                "workspace",
                                                "Workspace",
                                                "Curves / Timeline / Graph",
                                                EditorDockArea::Bottom);

            header  = createTextRelative(ctx.ui,
                                         toolbarFrame.background,
                                         {0.020f, 0.190f, 0.145f, 0.620f},
                                         font,
                                         "Particle FX",
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
                                         toolbarFrame.background,
                                         {0.165f, 0.210f, 0.105f, 0.580f},
                                         font,
                                         "Open effect",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            addEffectButtonRow(ctx.ui,
                               font,
                               toolbarFrame.background,
                               {0.290f, 0.220f, 0.245f, 0.540f},
                               {{EffectAction::Save, "Save"},
                                {EffectAction::SaveAs, "Save As"},
                                {EffectAction::Reload, "Reload"}});
            addPlaybackButtonRow(ctx.ui,
                                 font,
                                 toolbarFrame.background,
                                 {0.552f, 0.220f, 0.235f, 0.540f},
                                 {{PlaybackAction::PlayPause, "Pause"},
                                  {PlaybackAction::Restart, "Restart"},
                                  {PlaybackAction::Background, "Bg"},
                                  {PlaybackAction::CameraReset, "Camera"}});
            UiHandle toolbarSliders =
                createContainerRelative(ctx.ui, toolbarFrame.background, {0.800f, 0.125f, 0.178f, 0.760f});
            addSlider(ctx.ui, font, toolbarSliders, 0.020f, FloatField::TimeScale, "Time", 0.0f, 2.0f);
            addSlider(ctx.ui, font, toolbarSliders, 0.350f, FloatField::OrbitYaw, "Orbit", -180.0f, 180.0f);
            addSlider(ctx.ui, font, toolbarSliders, 0.680f, FloatField::OrbitDistance, "Distance", 1.0f, 40.0f);
            createRectRelative(ctx.ui, toolbarFrame.background, {0.275f, 0.185f, 0.0010f, 0.610f}, ToolTheme::separator);
            createRectRelative(ctx.ui, toolbarFrame.background, {0.542f, 0.185f, 0.0010f, 0.610f}, ToolTheme::separator);
            createRectRelative(ctx.ui, toolbarFrame.background, {0.792f, 0.185f, 0.0010f, 0.610f}, ToolTheme::separator);

            effectListHeader = createSectionHeaderRelative(
                ctx.ui, hierarchyFrame.background, {0.025f, 0.120f, 0.950f, 0.050f}, font, "Assets", "");
            effectNameField = createTextField(ctx.ui, hierarchyFrame.background, 0.178f, "Effect", font);
            for (size_t i = 0; i < EffectRowCount; ++i)
            {
                const float y = 0.228f + static_cast<float>(i) * 0.041f;
                effectRows.push_back(createButtonRelative(ctx.ui,
                                                          hierarchyFrame.background,
                                                          {0.025f, y, 0.950f, 0.034f},
                                                          font,
                                                          "",
                                                          ToolTheme::buttonTextScale));
            }

            emitterListHeader = createSectionHeaderRelative(
                ctx.ui, hierarchyFrame.background, {0.025f, 0.410f, 0.950f, 0.052f}, font, "Outliner", "");
            emitterSearchField = createSearchFieldRelative(
                ctx.ui, hierarchyFrame.background, {0.025f, 0.474f, 0.950f, 0.043f}, "Search", font);
            emitterNameField = createTextField(ctx.ui, hierarchyFrame.background, 0.525f, "Name", font);
            for (size_t i = 0; i < EmitterRowCount; ++i)
            {
                const float y = 0.525f + static_cast<float>(i) * 0.034f;
                emitterRows.push_back(createButtonRelative(ctx.ui,
                                                           hierarchyFrame.background,
                                                           {0.025f, y, 0.950f, 0.030f},
                                                           font,
                                                           "",
                                                           ToolTheme::buttonTextScale));
            }

            addEmitterButtonRow(ctx.ui,
                                font,
                                hierarchyFrame.background,
                                {0.025f, 0.920f, 0.950f, 0.034f},
                                {{EmitterAction::Add, "Add"},
                                 {EmitterAction::Delete, "Delete"},
                                 {EmitterAction::Duplicate, "Duplicate"},
                                 {EmitterAction::Rename, "Name"},
                                 {EmitterAction::ToggleEnabled, "ON"}});
            addEmitterButtonRow(ctx.ui,
                                font,
                                hierarchyFrame.background,
                                {0.025f, 0.958f, 0.950f, 0.032f},
                                {{EmitterAction::MoveUp, "Up"},
                                 {EmitterAction::MoveDown, "Down"},
                                 {EmitterAction::ToggleSelection, "Select"},
                                 {EmitterAction::Copy, "Copy"},
                                 {EmitterAction::Paste, "Paste"}});

            previewSwatch = createRectRelative(
                ctx.ui, previewFrame.background, {0.014f, 0.120f, 0.972f, 0.800f}, previewColor(), true);
            previewTextureImage = createImageRelative(ctx.ui, previewSwatch, {0.0f, 0.0f, 1.0f, 1.0f});
            buildPreviewReferenceOverlay(ctx.ui, previewSwatch, font);
            previewText = createTextRelative(ctx.ui,
                                             previewSwatch,
                                             {0.060f, 0.410f, 0.880f, 0.170f},
                                             font,
                                             "Live Preview",
                                             ToolTheme::text,
                                             ToolTheme::bodyTextScale);
            setTextAlignment(ctx.ui, previewText, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
            previewOverlay = createRectRelative(ctx.ui,
                                                previewSwatch,
                                                {0.705f, 0.045f, 0.275f, 0.255f},
                                                ToolTheme::panelInset);
            for (size_t i = 0; i < PreviewOverlayLineCount; ++i)
            {
                const float y = 0.075f + static_cast<float>(i) * 0.175f;
                previewOverlayLines.push_back(createTextRelative(ctx.ui,
                                                                 previewOverlay,
                                                                 {0.050f, y, 0.900f, 0.130f},
                                                                 font,
                                                                 "",
                                                                 ToolTheme::statusText,
                                                                 ToolTheme::smallTextScale));
            }
            previewScaleText = createTextRelative(ctx.ui,
                                                  previewSwatch,
                                                  {0.650f, 0.875f, 0.320f, 0.060f},
                                                  font,
                                                  "1m reference",
                                                  ToolTheme::mutedText,
                                                  ToolTheme::smallTextScale);
            setTextAlignment(ctx.ui, previewScaleText, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
            previewCameraText = createTextRelative(ctx.ui,
                                                   previewSwatch,
                                                   {0.025f, 0.045f, 0.380f, 0.055f},
                                                   font,
                                                   "Camera  Orbit  Perspective",
                                                   ToolTheme::mutedText,
                                                   ToolTheme::smallTextScale);
            setTextAlignment(ctx.ui, previewCameraText, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
            addPreviewButtonRow(ctx.ui,
                                font,
                                previewSwatch,
                                {0.330f, 0.858f, 0.360f, 0.048f},
                                {{PreviewAction::TogglePan, "Pan"},
                                 {PreviewAction::FrameSelection, "Frame"},
                                 {PreviewAction::FrameEffect, "All"},
                                 {PreviewAction::ResetCamera, "Reset"}});
            addPreviewButtonRow(ctx.ui,
                                font,
                                previewSwatch,
                                {0.020f, 0.858f, 0.292f, 0.048f},
                                {{PreviewAction::ToggleGrid, "Grid"},
                                 {PreviewAction::ToggleOrigin, "Origin"},
                                 {PreviewAction::ToggleScale, "Scale"},
                                 {PreviewAction::ToggleBounds, "Bounds"},
                                 {PreviewAction::ToggleGizmos, "Gizmos"},
                                 {PreviewAction::ToggleCollision, "Coll"},
                                 {PreviewAction::ToggleCulling, "Cull"}});

            inspectorHeader = createTextRelative(ctx.ui,
                                                 inspectorFrame.background,
                                                 {0.025f, 0.130f, 0.950f, 0.055f},
                                                 font,
                                                 "Selection",
                                                 ToolTheme::text,
                                                 ToolTheme::headerTextScale);
            addInspectorSection(
                ctx.ui, font, InspectorSection::General, {0.025f, 0.220f, 0.950f, 0.044f}, "General", "");
            addInspectorSection(
                ctx.ui, font, InspectorSection::Modules, {0.025f, 0.272f, 0.950f, 0.044f}, "Modules", "");
            addInspectorSection(
                ctx.ui, font, InspectorSection::Parameters, {0.025f, 0.324f, 0.950f, 0.044f}, "Parameters", "");
            addInspectorSection(
                ctx.ui, font, InspectorSection::Runtime, {0.025f, 0.376f, 0.950f, 0.044f}, "Runtime", "");

            for (size_t i = 0; i < InspectorStatusRowCount; ++i)
            {
                const float rowY = 0.455f + static_cast<float>(i) * 0.044f;
                inspectorStatusRows.push_back(createStatusRowRelative(ctx.ui,
                                                                      inspectorFrame.background,
                                                                      {0.035f, rowY, 0.930f, 0.036f},
                                                                      font,
                                                                      inspectorStatusLabel(i),
                                                                      ""));
            }

            for (size_t i = 0; i < ModuleRowCount; ++i)
            {
                const float rowY = 0.455f + static_cast<float>(i) * 0.046f;
                moduleRows.push_back(createButtonRelative(ctx.ui,
                                                          inspectorFrame.background,
                                                          {0.035f, rowY, 0.930f, 0.038f},
                                                          font,
                                                          "",
                                                          ToolTheme::buttonTextScale));
            }

            addModuleButtonRow(ctx.ui,
                               font,
                               inspectorFrame.background,
                               {0.035f, 0.710f, 0.930f, 0.040f},
                               {{ModuleAction::Copy, "Copy"},
                                {ModuleAction::Paste, "Paste"},
                                {ModuleAction::Undo, "Undo"},
                                {ModuleAction::Redo, "Redo"}});

            for (size_t i = 0; i < ParameterControlRowCount; ++i)
            {
                const float rowY = 0.455f + static_cast<float>(i) * 0.060f;
                parameterControls.push_back(
                    {createSlider(ctx.ui, inspectorFrame.background, rowY, "Parameter", 0.0f, 1.0f, false, font),
                     createButtonRelative(ctx.ui,
                                          inspectorFrame.background,
                                          {0.035f, rowY, 0.930f, 0.038f},
                                          font,
                                          "",
                                          ToolTheme::buttonTextScale),
                     createButtonRelative(ctx.ui,
                                          inspectorFrame.background,
                                          {0.035f, rowY, 0.658f, 0.038f},
                                          font,
                                          "",
                                          ToolTheme::buttonTextScale),
                     createButtonRelative(ctx.ui,
                                          inspectorFrame.background,
                                          {0.710f, rowY, 0.075f, 0.038f},
                                          font,
                                          "Reset",
                                          ToolTheme::buttonTextScale),
                     createButtonRelative(ctx.ui,
                                          inspectorFrame.background,
                                          {0.800f, rowY, 0.075f, 0.038f},
                                          font,
                                          "-",
                                          ToolTheme::buttonTextScale),
                     createButtonRelative(ctx.ui,
                                          inspectorFrame.background,
                                          {0.890f, rowY, 0.075f, 0.038f},
                                          font,
                                          "+",
                                          ToolTheme::buttonTextScale)});
            }

            addWorkspaceTab(ctx.ui, font, WorkspaceTab::Curves, {0.020f, 0.130f, 0.095f, 0.095f}, "Curves");
            addWorkspaceTab(ctx.ui, font, WorkspaceTab::Timeline, {0.122f, 0.130f, 0.095f, 0.095f}, "Timeline");
            addWorkspaceTab(ctx.ui, font, WorkspaceTab::Graph, {0.224f, 0.130f, 0.095f, 0.095f}, "Graph");
            addWorkspaceTab(ctx.ui, font, WorkspaceTab::Diagnostics, {0.326f, 0.130f, 0.115f, 0.095f}, "Diagnostics");
            addWorkspaceTab(ctx.ui, font, WorkspaceTab::Preview, {0.448f, 0.130f, 0.130f, 0.095f}, "Preview");
            addWorkspaceTab(ctx.ui, font, WorkspaceTab::Output, {0.585f, 0.130f, 0.095f, 0.095f}, "Output");

            addGraphButtonRow(ctx.ui,
                              font,
                              workspaceFrame.background,
                              {0.705f, 0.130f, 0.270f, 0.095f},
                              {{GraphAction::Search, "Search"},
                               {GraphAction::AddNode, "Add"},
                               {GraphAction::Link, "Link"},
                               {GraphAction::Frame, "Frame"},
                               {GraphAction::Comment, "Note"}});

            workspaceHeadline = createTextRelative(ctx.ui,
                                                   workspaceFrame.background,
                                                   {0.020f, 0.285f, 0.955f, 0.085f},
                                                   font,
                                                   "Diagnostics",
                                                   ToolTheme::text,
                                                   ToolTheme::headerTextScale);
            for (size_t i = 0; i < WorkspaceLineCount; ++i)
            {
                const float y = 0.400f + static_cast<float>(i) * 0.073f;
                workspaceInfoLines.push_back(createTextRelative(ctx.ui,
                                                                workspaceFrame.background,
                                                                {0.020f, y, 0.955f, 0.052f},
                                                                font,
                                                                "",
                                                                ToolTheme::statusText,
                                                                ToolTheme::smallTextScale));
            }

            createRectRelative(ctx.ui, root, {0.0f, 0.971f, 1.0f, 0.029f}, ToolTheme::barBackground);
            createRectRelative(ctx.ui, root, {0.0f, 0.970f, 1.0f, 0.001f}, ToolTheme::separator);
            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.010f, 0.974f, 0.980f, 0.020f},
                                        font,
                                        "Ready",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
            notificationText = createTextRelative(ctx.ui,
                                                  toolbarFrame.background,
                                                  {0.285f, 0.690f, 0.695f, 0.190f},
                                                  font,
                                                  "",
                                                  ToolTheme::statusText,
                                                  ToolTheme::smallTextScale);
            setTextAlignment(ctx.ui, notificationText, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            releaseUndoCapture(interaction);

            std::vector<std::string> effectPaths = collectEffectPaths(ctx.world);
            includeCurrentPath(effectPaths);
            clampEffectBrowserOffset(effectPaths);

            if (!hasAsset && !effectPaths.empty())
                openEffect(ctx.world, effectPaths.front(), state, false);

            applyEffectBrowser(ctx.world, state, effectPaths, interaction);
            applyTextFieldFocus(ctx.world, state, interaction);
            applyInspectorSections(state, interaction);
            applyWorkspaceTabs(state, interaction);
            if (hasAsset)
            {
                applyTextInput(ctx, state);
                applyKeyboardShortcuts(ctx, state);
                applyEffectButtons(ctx.world, state, interaction);
                applyEmitterRows(ctx.world, state, interaction);
                applyEmitterButtons(ctx.world, state, interaction);
                applyModuleRows(state, interaction);
                applyModuleButtons(ctx.world, state, interaction);
                applyGraphButtons(ctx.world, state, interaction);
                applyPlaybackButtons(ctx.world, state, interaction);
                applyPreviewButtons(ctx.world, state, interaction);
                applyPreviewOrbitDrag(state, interaction);
                applyPreviewZoom(state, interaction);
                applySliders(ctx.ui, ctx.world, state, interaction);
                applyModuleParameterControls(ctx.ui, ctx.world, state, interaction);
                syncPreviewWorld(ctx);
            }
            else
            {
                clearPreviewRender(ctx.world);
                previewWorld.destroy();
            }
            claimKeyboardIfEditing(ctx.world);

            updateDisplay(ctx, state, effectPaths);
            publishPreviewRender(ctx);
        }

        void setVisible(UiSystem& ui, bool visible) override
        {
            setVisibleRecursive(ui, root, visible);
        }

        void onDeactivate(EngineToolContext& ctx) override
        {
            clearPreviewRender(ctx.world);
            previewWorld.destroy();
        }

        void shutdown() override
        {
            previewWorld.destroy();
        }

        void destroy(UiSystem& ui) override
        {
            previewWorld.destroy();

            if (root != UI_INVALID_HANDLE)
                ui.removeNode(root);
            root = UI_INVALID_HANDLE;
            effectRows.clear();
            emitterRows.clear();
            effectButtons.clear();
            emitterButtons.clear();
            moduleButtons.clear();
            graphButtons.clear();
            playbackButtons.clear();
            previewButtons.clear();
            moduleRows.clear();
            parameterControls.clear();
            sliders.clear();
            inspectorSections.clear();
            workspaceTabs.clear();
            inspectorStatusRows.clear();
            workspaceInfoLines.clear();
            previewOverlayLines.clear();
            previewGridLines.clear();
            previewOriginLines.clear();
            previewScaleLines.clear();
            previewTextureImage = UI_INVALID_HANDLE;
        }

        private:
        enum class EffectAction
        {
            Save,
            SaveAs,
            Duplicate,
            Reload
        };

        enum class EmitterAction
        {
            Add,
            Delete,
            Duplicate,
            Rename,
            MoveUp,
            MoveDown,
            ToggleEnabled,
            ToggleSelection,
            Copy,
            Paste
        };

        enum class ModuleAction
        {
            Copy,
            Paste,
            Undo,
            Redo
        };

        enum class GraphAction
        {
            Search,
            AddNode,
            Link,
            Frame,
            Comment
        };

        enum class PlaybackAction
        {
            PlayPause,
            Restart,
            Background,
            CameraReset
        };

        enum class PreviewAction
        {
            TogglePan,
            FrameSelection,
            FrameEffect,
            ResetCamera,
            ToggleGrid,
            ToggleOrigin,
            ToggleScale,
            ToggleBounds,
            ToggleGizmos,
            ToggleCollision,
            ToggleCulling
        };

        enum class FloatField
        {
            TimeScale,
            OrbitYaw,
            OrbitDistance
        };

        enum class ActiveTextField
        {
            None,
            EffectName,
            EmitterName,
            EmitterSearch
        };

        enum class FxSelectionKind
        {
            Effect,
            Emitter,
            Module,
            Curve,
            Burst,
            GraphNode
        };

        struct FxSelection
        {
            FxSelectionKind kind = FxSelectionKind::Effect;
            size_t          emitterIndex = 0;
            size_t          moduleIndex = 0;
            std::string     parameterId;
        };

        enum class InspectorSection
        {
            General,
            Modules,
            Parameters,
            Runtime
        };

        enum class WorkspaceTab
        {
            Curves,
            Timeline,
            Graph,
            Diagnostics,
            Preview,
            Output
        };

        struct EffectButtonBinding
        {
            EffectAction action = EffectAction::Save;
            ToolButton   button;
        };

        struct EmitterButtonBinding
        {
            EmitterAction action = EmitterAction::Add;
            ToolButton    button;
        };

        struct ModuleButtonBinding
        {
            ModuleAction action = ModuleAction::Copy;
            ToolButton   button;
        };

        struct GraphButtonBinding
        {
            GraphAction action = GraphAction::Search;
            ToolButton  button;
        };

        struct PlaybackButtonBinding
        {
            PlaybackAction action = PlaybackAction::PlayPause;
            ToolButton     button;
        };

        struct PreviewButtonBinding
        {
            PreviewAction action = PreviewAction::FrameSelection;
            ToolButton    button;
        };

        struct SliderBinding
        {
            FloatField field = FloatField::TimeScale;
            ToolSlider slider;
        };

        struct InspectorSectionBinding
        {
            InspectorSection  section = InspectorSection::General;
            ToolSectionHeader header;
        };

        struct WorkspaceTabBinding
        {
            WorkspaceTab tab = WorkspaceTab::Diagnostics;
            ToolButton   button;
        };

        struct ParameterControl
        {
            ToolSlider slider;
            ToolButton button;
            ToolButton selector;
            ToolButton reset;
            ToolButton decrement;
            ToolButton increment;
        };

        struct ParameterView
        {
            const ParticleModuleParameterDefinition* primary   = nullptr;
            const ParticleModuleParameterDefinition* secondary = nullptr;

            bool isRange() const
            {
                return primary != nullptr && secondary != nullptr;
            }
        };

        struct OutlinerItem
        {
            FxSelection selection;
            std::string label;
            std::string detail;
            int         depth = 0;
            bool        enabled = true;
        };

        static constexpr size_t EffectRowCount           = 4;
        static constexpr size_t EmitterRowCount          = 12;
        static constexpr size_t ModuleRowCount           = 5;
        static constexpr size_t ParameterControlRowCount = 5;
        static constexpr size_t InspectorStatusRowCount  = 7;
        static constexpr size_t WorkspaceLineCount       = 6;
        static constexpr size_t PreviewOverlayLineCount  = 5;
        static constexpr size_t MaxUndoSnapshots         = 32;

        UiHandle root              = UI_INVALID_HANDLE;
        UiHandle header            = UI_INVALID_HANDLE;
        UiHandle summary           = UI_INVALID_HANDLE;
        UiHandle emitterHeader     = UI_INVALID_HANDLE;
        UiHandle previewSwatch     = UI_INVALID_HANDLE;
        UiHandle previewText       = UI_INVALID_HANDLE;
        UiHandle previewOverlay    = UI_INVALID_HANDLE;
        UiHandle previewScaleText  = UI_INVALID_HANDLE;
        UiHandle previewCameraText = UI_INVALID_HANDLE;
        UiHandle notificationText  = UI_INVALID_HANDLE;
        UiHandle inspectorHeader   = UI_INVALID_HANDLE;
        UiHandle footer            = UI_INVALID_HANDLE;
        UiHandle workspaceHeadline = UI_INVALID_HANDLE;

        ToolPanelFrame    toolbarFrame;
        ToolPanelFrame    hierarchyFrame;
        ToolPanelFrame    previewFrame;
        ToolPanelFrame    inspectorFrame;
        ToolPanelFrame    workspaceFrame;
        ToolSectionHeader effectListHeader;
        ToolSectionHeader emitterListHeader;

        ToolTextField effectNameField;
        ToolTextField emitterNameField;
        ToolTextField emitterSearchField;

        std::vector<ToolButton>              effectRows;
        std::vector<ToolButton>              emitterRows;
        std::vector<EffectButtonBinding>     effectButtons;
        std::vector<EmitterButtonBinding>    emitterButtons;
        std::vector<ModuleButtonBinding>     moduleButtons;
        std::vector<GraphButtonBinding>      graphButtons;
        std::vector<PlaybackButtonBinding>   playbackButtons;
        std::vector<PreviewButtonBinding>    previewButtons;
        std::vector<ToolButton>              moduleRows;
        std::vector<ParameterControl>        parameterControls;
        std::vector<SliderBinding>           sliders;
        std::vector<InspectorSectionBinding> inspectorSections;
        std::vector<WorkspaceTabBinding>     workspaceTabs;
        std::vector<ToolStatusRow>           inspectorStatusRows;
        std::vector<UiHandle>                workspaceInfoLines;
        std::vector<UiHandle>                previewOverlayLines;
        std::vector<UiHandle>                previewGridLines;
        std::vector<UiHandle>                previewOriginLines;
        std::vector<UiHandle>                previewScaleLines;

        ParticleEffectAsset                   currentAsset;
        std::string                           currentPath;
        bool                                  hasAsset                 = false;
        bool                                  dirty                    = false;
        size_t                                effectBrowserOffset      = 0;
        size_t                                emitterBrowserOffset     = 0;
        size_t                                selectedEmitterIndex     = 0;
        size_t                                selectedModuleIndex      = 0;
        size_t                                moduleBrowserOffset      = 0;
        size_t                                parameterControlOffset   = 0;
        bool                                  playbackPaused           = false;
        float                                 playbackTimeScale        = 1.0f;
        uint32_t                              backgroundPreset         = 0;
        bool                                  previewPanMode           = false;
        bool                                  previewGridVisible       = true;
        bool                                  previewOriginVisible     = true;
        bool                                  previewScaleVisible      = true;
        bool                                  previewBoundsVisible     = false;
        bool                                  previewGizmosVisible     = true;
        bool                                  previewCollisionVisible  = false;
        bool                                  previewCullingVisible    = false;
        std::string                           lastStatusText;
        std::string                           notificationMessage;
        float                                 notificationSeconds = 0.0f;
        ActiveTextField                       activeTextField          = ActiveTextField::None;
        InspectorSection                      selectedInspectorSection = InspectorSection::Parameters;
        WorkspaceTab                          selectedWorkspaceTab     = WorkspaceTab::Diagnostics;
        std::string                           effectNameDraft;
        std::string                           emitterNameDraft;
        std::string                           emitterSearchDraft;
        std::vector<ParticleEffectAsset>      undoStack;
        std::vector<ParticleEffectAsset>      redoStack;
        std::optional<ParticleModuleInstance> moduleClipboard;
        std::vector<ParticleEffectEmitter>    emitterClipboard;
        std::vector<std::string>              selectedEmitterIds;
        FxSelection                           selectedObject;
        std::vector<OutlinerItem>             visibleOutlinerItems;
        std::string                           selectedRichParameterId;
        size_t                                selectedRichKeyIndex = 0;
        uint32_t                              selectedRichField    = 0;
        std::string                           selectedRangeParameterId;
        bool                                  selectedRangeMax       = false;
        UiHandle                              activeUndoHandle       = UI_INVALID_HANDLE;
        UiHandle                              previewTextureImage    = UI_INVALID_HANDLE;
        ParticlePreviewWorld                  previewWorld;
        bool                                  previewOrbitDragActive = false;
        float                                 previewOrbitDragLastX  = 0.0f;
        float                                 previewOrbitDragLastY  = 0.0f;
        bool                                  emitterDragActive      = false;
        size_t                                emitterDragIndex       = 0;
        float                                 emitterDragLastY       = 0.0f;
        size_t                                graphSearchIndex       = 0;

        static void buildApplicationChrome(UiSystem& ui, UiHandle parent, BitmapFont* font)
        {
            UiHandle menuBar = createRectRelative(ui, parent, {0.0f, 0.0f, 1.0f, 0.030f}, ToolTheme::barBackground);
            createRectRelative(ui, menuBar, {0.0f, 0.960f, 1.0f, 0.040f}, ToolTheme::separator);

            UiHandle brand = createTextRelative(ui,
                                                menuBar,
                                                {0.012f, 0.170f, 0.075f, 0.680f},
                                                font,
                                                "Gravitas",
                                                ToolTheme::text,
                                                ToolTheme::titleTextScale);
            setTextAlignment(ui, brand, UiHorizontalAlign::Left, UiVerticalAlign::Middle);

            const char* menuItems[] = {"File", "Edit", "Assets", "Window", "Help"};
            float       x           = 0.105f;
            for (const char* item : menuItems)
            {
                UiHandle label = createTextRelative(
                    ui, menuBar, {x, 0.190f, 0.052f, 0.640f}, font, item, ToolTheme::mutedText, ToolTheme::buttonTextScale);
                setTextAlignment(ui, label, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
                x += 0.058f;
            }

            UiHandle stateChip =
                createRectRelative(ui, menuBar, {0.842f, 0.180f, 0.070f, 0.640f}, ToolTheme::panelInset);
            UiHandle stateText = createTextRelative(ui,
                                                    stateChip,
                                                    {0.070f, 0.120f, 0.860f, 0.760f},
                                                    font,
                                                    "Auto Reload",
                                                    ToolTheme::statusText,
                                                    ToolTheme::smallTextScale);
            setTextAlignment(ui, stateText, UiHorizontalAlign::Center, UiVerticalAlign::Middle);

            UiHandle modeChip =
                createRectRelative(ui, menuBar, {0.918f, 0.180f, 0.070f, 0.640f}, ToolTheme::accentSoft);
            UiHandle modeText = createTextRelative(ui,
                                                   modeChip,
                                                   {0.070f, 0.120f, 0.860f, 0.760f},
                                                   font,
                                                   "Preview Live",
                                                   ToolTheme::text,
                                                   ToolTheme::smallTextScale);
            setTextAlignment(ui, modeText, UiHorizontalAlign::Center, UiVerticalAlign::Middle);

            UiHandle tabs = createRectRelative(ui, parent, {0.0f, 0.030f, 1.0f, 0.041f}, ToolTheme::paneBackground);
            createRectRelative(ui, tabs, {0.0f, 0.965f, 1.0f, 0.035f}, ToolTheme::separator);

            UiHandle activeTab = createRectRelative(ui, tabs, {0.010f, 0.105f, 0.090f, 0.785f}, ToolTheme::buttonActive);
            createRectRelative(ui, activeTab, {0.0f, 0.0f, 1.0f, 0.060f}, ToolTheme::accent);
            UiHandle activeLabel = createTextRelative(ui,
                                                      activeTab,
                                                      {0.100f, 0.120f, 0.800f, 0.760f},
                                                      font,
                                                      "Particle FX",
                                                      ToolTheme::text,
                                                      ToolTheme::buttonTextScale);
            setTextAlignment(ui, activeLabel, UiHorizontalAlign::Center, UiVerticalAlign::Middle);

            const char* inactiveTabs[] = {"Scene", "Material", "Animation", "Audio"};
            x                          = 0.106f;
            for (const char* tab : inactiveTabs)
            {
                UiHandle tabRect = createRectRelative(ui, tabs, {x, 0.105f, 0.076f, 0.785f}, ToolTheme::panelInset);
                UiHandle tabText = createTextRelative(ui,
                                                      tabRect,
                                                      {0.080f, 0.120f, 0.840f, 0.760f},
                                                      font,
                                                      tab,
                                                      ToolTheme::mutedText,
                                                      ToolTheme::buttonTextScale);
                setTextAlignment(ui, tabText, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
                x += 0.080f;
            }
        }

        static ToolPanelFrame createFxPanelFrame(UiSystem&          ui,
                                                 UiHandle           parent,
                                                 const ToolRect&    rect,
                                                 BitmapFont*        font,
                                                 const std::string& id,
                                                 const std::string& title,
                                                 const std::string& subtitle,
                                                 EditorDockArea     area)
        {
            if (area == EditorDockArea::Top)
            {
                ToolPanelFrame frame;
                frame.background = createRectRelative(ui, parent, rect, ToolTheme::viewportBar);
                createRectRelative(ui, frame.background, {0.0f, 0.000f, 1.0f, 0.030f}, ToolTheme::borderSubtle);
                createRectRelative(ui, frame.background, {0.0f, 0.970f, 1.0f, 0.030f}, ToolTheme::separator);
                frame.accent = createRectRelative(ui, frame.background, {0.0f, 0.0f, 0.002f, 1.0f}, ToolTheme::accentMuted);
                frame.title = createTextRelative(ui,
                                                 frame.background,
                                                 {0.0f, 0.0f, 0.0f, 0.0f},
                                                 font,
                                                 title,
                                                 ToolTheme::mutedText,
                                                 ToolTheme::smallTextScale);
                frame.subtitle = createTextRelative(ui,
                                                    frame.background,
                                                    {0.0f, 0.0f, 0.0f, 0.0f},
                                                    font,
                                                    subtitle,
                                                    ToolTheme::mutedText,
                                                    ToolTheme::smallTextScale);
                return frame;
            }

            EditorPanelState panel;
            panel.id       = id;
            panel.title    = title;
            panel.subtitle = subtitle;
            panel.area     = area;
            return createEditorPanelFrameRelative(ui, parent, rect, font, panel);
        }

        void addEffectButtonRow(UiSystem&                                                ui,
                                BitmapFont*                                              font,
                                UiHandle                                                 parent,
                                const ToolRect&                                          rowRect,
                                const std::vector<std::pair<EffectAction, std::string>>& rowButtons)
        {
            addButtonRow(ui, font, parent, rowRect, rowButtons, effectButtons, 0.015f);
        }

        void addEmitterButtonRow(UiSystem&                                                 ui,
                                 BitmapFont*                                               font,
                                 UiHandle                                                  parent,
                                 const ToolRect&                                           rowRect,
                                 const std::vector<std::pair<EmitterAction, std::string>>& rowButtons)
        {
            addButtonRow(ui, font, parent, rowRect, rowButtons, emitterButtons, 0.015f);
        }

        void addModuleButtonRow(UiSystem&                                                ui,
                                BitmapFont*                                              font,
                                UiHandle                                                 parent,
                                const ToolRect&                                          rowRect,
                                const std::vector<std::pair<ModuleAction, std::string>>& rowButtons)
        {
            addButtonRow(ui, font, parent, rowRect, rowButtons, moduleButtons, 0.015f);
        }

        void addGraphButtonRow(UiSystem&                                               ui,
                               BitmapFont*                                             font,
                               UiHandle                                                parent,
                               const ToolRect&                                         rowRect,
                               const std::vector<std::pair<GraphAction, std::string>>& rowButtons)
        {
            addButtonRow(ui, font, parent, rowRect, rowButtons, graphButtons, 0.010f);
        }

        void addPlaybackButtonRow(UiSystem&                                                  ui,
                                  BitmapFont*                                                font,
                                  UiHandle                                                   parent,
                                  const ToolRect&                                            rowRect,
                                  const std::vector<std::pair<PlaybackAction, std::string>>& rowButtons)
        {
            addButtonRow(ui, font, parent, rowRect, rowButtons, playbackButtons, 0.015f);
        }

        void addPreviewButtonRow(UiSystem&                                                 ui,
                                 BitmapFont*                                               font,
                                 UiHandle                                                  parent,
                                 const ToolRect&                                           rowRect,
                                 const std::vector<std::pair<PreviewAction, std::string>>& rowButtons)
        {
            addButtonRow(ui, font, parent, rowRect, rowButtons, previewButtons, 0.010f);
        }

        template <typename Action, typename Binding>
        static void addButtonRow(UiSystem&                                         ui,
                                 BitmapFont*                                       font,
                                 UiHandle                                          parent,
                                 const ToolRect&                                   rowRect,
                                 const std::vector<std::pair<Action, std::string>>& rowButtons,
                                 std::vector<Binding>&                             bindings,
                                 float                                             gap)
        {
            if (rowButtons.empty())
                return;

            const float width = (rowRect.width - gap * static_cast<float>(rowButtons.size() - 1)) /
                                static_cast<float>(rowButtons.size());

            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                const float x = rowRect.x + static_cast<float>(i) * (width + gap);
                bindings.push_back({rowButtons[i].first,
                                    createButtonRelative(ui,
                                                         parent,
                                                         {x, rowRect.y, width, rowRect.height},
                                                         font,
                                                         rowButtons[i].second,
                                                         ToolTheme::buttonTextScale)});
            }
        }

        void buildPreviewReferenceOverlay(UiSystem& ui, UiHandle parent, BitmapFont* font)
        {
            UiColor gridColor = DefaultEditorTheme.colors.graphGridMajor;
            UiColor originX   = ToolTheme::axisX;
            UiColor originZ   = ToolTheme::axisZ;
            UiColor reference = ToolTheme::statusText;
            gridColor.a       = 0.135f;
            originX.a         = 0.360f;
            originZ.a         = 0.360f;
            reference.a       = 0.620f;
            for (int i = 1; i < 6; ++i)
            {
                const float t = static_cast<float>(i) / 6.0f;
                previewGridLines.push_back(createEditorLineRelative(ui, parent, {t, 0.0f}, {t, 1.0f}, gridColor, 0.0008f));
                previewGridLines.push_back(createEditorLineRelative(ui, parent, {0.0f, t}, {1.0f, t}, gridColor, 0.0008f));
            }

            previewOriginLines.push_back(createEditorLineRelative(ui, parent, {0.500f, 0.0f}, {0.500f, 1.0f}, originZ, 0.0013f));
            previewOriginLines.push_back(createEditorLineRelative(ui, parent, {0.0f, 0.500f}, {1.0f, 0.500f}, originX, 0.0013f));
            previewScaleLines.push_back(createEditorLineRelative(ui, parent, {0.780f, 0.905f}, {0.940f, 0.905f}, reference, 0.0030f));
            previewScaleLines.push_back(createEditorLineRelative(ui, parent, {0.780f, 0.890f}, {0.780f, 0.920f}, reference, 0.0020f));
            previewScaleLines.push_back(createEditorLineRelative(ui, parent, {0.940f, 0.890f}, {0.940f, 0.920f}, reference, 0.0020f));
            (void)font;
        }

        void addSlider(UiSystem&          ui,
                       BitmapFont*        font,
                       UiHandle           parent,
                       float              y,
                       FloatField         field,
                       const std::string& name,
                       float              minValue,
                       float              maxValue,
                       bool               wholeNumber = false)
        {
            sliders.push_back({field, createSlider(ui, parent, y, name, minValue, maxValue, wholeNumber, font)});
        }

        void addInspectorSection(UiSystem&          ui,
                                 BitmapFont*        font,
                                 InspectorSection   section,
                                 const ToolRect&    rect,
                                 const std::string& label,
                                 const std::string& summary)
        {
            inspectorSections.push_back(
                {section, createSectionHeaderRelative(ui, inspectorFrame.background, rect, font, label, summary)});
        }

        void addWorkspaceTab(
            UiSystem& ui, BitmapFont* font, WorkspaceTab tab, const ToolRect& rect, const std::string& label)
        {
            workspaceTabs.push_back(
                {tab,
                 createButtonRelative(ui, workspaceFrame.background, rect, font, label, ToolTheme::buttonTextScale)});
        }

        static std::vector<std::string> collectEffectPaths(ECSWorld& world)
        {
            std::vector<std::string> paths;
            if (world.hasAny<ParticleEffectRegistryComponent>())
            {
                const ParticleEffectRegistryComponent& registry = world.getSingleton<ParticleEffectRegistryComponent>();
                for (const auto& registryEntry : registry.effects)
                {
                    const std::string& path = registryEntry.first;
                    if (!path.empty())
                        paths.push_back(path);
                }
            }

            world.forEach<ParticleEmitterComponent>(
                [&](Entity, ParticleEmitterComponent& emitter)
                {
                    if (!emitter.effectPath.empty())
                        paths.push_back(emitter.effectPath);
                });

            scanParticleEffectDirectory(paths, "resources/particles");
            scanParticleEffectDirectory(paths, "../../resources/particles");
            scanParticleEffectDirectory(paths, "../resources/particles");

            sortUnique(paths);
            return paths;
        }

        void includeCurrentPath(std::vector<std::string>& paths) const
        {
            if (!currentPath.empty())
            {
                paths.push_back(currentPath);
                sortUnique(paths);
            }
        }

        static void sortUnique(std::vector<std::string>& paths)
        {
            std::sort(paths.begin(), paths.end());
            paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
        }

        static void scanParticleEffectDirectory(std::vector<std::string>& paths, const std::filesystem::path& rootPath)
        {
            std::error_code ec;
            if (!std::filesystem::exists(rootPath, ec) || !std::filesystem::is_directory(rootPath, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator       it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
                    paths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        void applyEffectBrowser(ECSWorld&                       world,
                                EngineToolStateComponent&       state,
                                const std::vector<std::string>& paths,
                                const UiInteractionResult&      interaction)
        {
            if (!paths.empty() && interaction.scrollY != 0.0f && pointerOverEffectRows(interaction))
            {
                if (interaction.scrollY < 0.0f)
                    effectBrowserOffset = std::min(effectBrowserOffset + 1, maxEffectBrowserOffset(paths));
                else if (effectBrowserOffset > 0)
                    effectBrowserOffset -= 1;
            }

            for (size_t i = 0; i < effectRows.size() && effectBrowserOffset + i < paths.size(); ++i)
            {
                if (wasClicked(interaction, effectRows[i].rect))
                {
                    openEffect(world, paths[effectBrowserOffset + i], state, true);
                    return;
                }
            }
        }

        bool pointerOverEffectRows(const UiInteractionResult& interaction) const
        {
            for (const ToolButton& row : effectRows)
            {
                if (interaction.hovered == row.rect || interaction.pressed == row.rect)
                    return true;
            }
            return false;
        }

        bool pointerOverEmitterRows(const UiInteractionResult& interaction) const
        {
            for (const ToolButton& row : emitterRows)
            {
                if (interaction.hovered == row.rect || interaction.pressed == row.rect)
                    return true;
            }
            return false;
        }

        void selectOutlinerItem(ECSWorld&                 world,
                                EngineToolStateComponent& state,
                                const FxSelection&        selection,
                                bool                      changedAsset)
        {
            selectedObject = selection;
            if (selection.kind == FxSelectionKind::Effect)
            {
                selectedInspectorSection = InspectorSection::General;
                state.status = "SELECTED EFFECT";
                return;
            }

            if (currentAsset.emitters.empty())
                return;

            selectedEmitterIndex = std::min(selection.emitterIndex, currentAsset.emitters.size() - 1);
            syncEmitterNameDraft();
            revealSelectedEmitter();

            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return;

            if (selection.kind == FxSelectionKind::Emitter)
            {
                selectedModuleIndex = std::min(selectedModuleIndex, emitter->modules.empty() ? 0 : emitter->modules.size() - 1);
                selectedInspectorSection = InspectorSection::General;
                parameterControlOffset = 0;
                clearParameterSelectionState();
                if (!changedAsset)
                    applySelectedEmitterToLive(world, false);
                state.status = "SELECTED " + displayEmitterName(*emitter);
                return;
            }

            if (!emitter->modules.empty())
                selectedModuleIndex = std::min(selection.moduleIndex, emitter->modules.size() - 1);
            selectedInspectorSection = InspectorSection::Parameters;
            parameterControlOffset = 0;
            clearParameterSelectionState();

            if (selection.kind == FxSelectionKind::Curve || selection.kind == FxSelectionKind::Burst)
            {
                selectedRichParameterId = selection.parameterId;
                selectedWorkspaceTab =
                    selection.kind == FxSelectionKind::Burst ? WorkspaceTab::Timeline : WorkspaceTab::Curves;
            }

            if (!changedAsset)
                applySelectedEmitterToLive(world, false);

            const ParticleModuleInstance* module = selectedModule();
            state.status = module == nullptr ? "SELECTED MODULE" : selectedModuleStatus(*module);
        }

        bool selectionMatches(const FxSelection& selection) const
        {
            if (selectedObject.kind != selection.kind)
                return false;
            if (selection.kind == FxSelectionKind::Effect)
                return true;
            if (selectedObject.emitterIndex != selection.emitterIndex)
                return false;
            if (selection.kind == FxSelectionKind::Emitter)
                return true;
            if (selectedObject.moduleIndex != selection.moduleIndex)
                return false;
            if (selection.kind == FxSelectionKind::Module)
                return true;
            return selectedObject.parameterId == selection.parameterId;
        }

        std::vector<OutlinerItem> buildOutlinerItems() const
        {
            std::vector<OutlinerItem> items;
            if (!hasAsset)
                return items;

            const std::string effectName =
                currentAsset.metadata.name.empty() ? fileName(currentPath) : currentAsset.metadata.name;
            items.push_back({FxSelection{FxSelectionKind::Effect, 0, 0, {}},
                             "FX  " + compact(effectName, 30),
                             std::to_string(currentAsset.emitters.size()) + " emitters",
                             0,
                             true});

            const std::vector<size_t> visibleEmitters = filteredEmitterIndices();
            for (size_t emitterIndex : visibleEmitters)
            {
                const ParticleEffectEmitter& emitter = currentAsset.emitters[emitterIndex];
                const bool emitterSelected =
                    selectedObject.kind != FxSelectionKind::Effect && selectedObject.emitterIndex == emitterIndex;
                items.push_back({FxSelection{FxSelectionKind::Emitter, emitterIndex, 0, {}},
                                 "EM  " + displayEmitterName(emitter),
                                 emitter.descriptor.enabled ? std::to_string(emitter.modules.size()) + " modules"
                                                            : "disabled",
                                 1,
                                 emitter.descriptor.enabled});

                if (!emitterSelected)
                    continue;

                for (size_t moduleIndex = 0; moduleIndex < emitter.modules.size(); ++moduleIndex)
                {
                    const ParticleModuleInstance& module = emitter.modules[moduleIndex];
                    items.push_back({FxSelection{FxSelectionKind::Module, emitterIndex, moduleIndex, {}},
                                     "MD  " + moduleOutlinerLabel(module),
                                     module.enabled ? moduleStageShortLabel(module) : "off",
                                     2,
                                     module.enabled});

                    const bool moduleSelected = selectedObject.emitterIndex == emitterIndex &&
                                                selectedObject.moduleIndex == moduleIndex &&
                                                selectedObject.kind != FxSelectionKind::Emitter;
                    if (!moduleSelected)
                        continue;

                    const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
                    if (definition == nullptr)
                        continue;

                    for (const ParticleModuleParameterDefinition& parameter : definition->parameters)
                    {
                        if (parameter.type == ParticleModuleParameterType::FloatCurve ||
                            parameter.type == ParticleModuleParameterType::ColorGradient)
                        {
                            items.push_back({FxSelection{FxSelectionKind::Curve,
                                                         emitterIndex,
                                                         moduleIndex,
                                                         parameter.id},
                                             "CV  " + parameter.label,
                                             parameter.type == ParticleModuleParameterType::ColorGradient ? "gradient"
                                                                                                          : "curve",
                                             3,
                                             module.enabled});
                        }
                        else if (parameter.type == ParticleModuleParameterType::BurstTimeline)
                        {
                            items.push_back({FxSelection{FxSelectionKind::Burst,
                                                         emitterIndex,
                                                         moduleIndex,
                                                         parameter.id},
                                             "BT  " + parameter.label,
                                             "timeline",
                                             3,
                                             module.enabled});
                        }
                    }
                }
            }

            return items;
        }

        void clampEffectBrowserOffset(const std::vector<std::string>& paths)
        {
            effectBrowserOffset = std::min(effectBrowserOffset, maxEffectBrowserOffset(paths));
        }

        size_t maxEffectBrowserOffset(const std::vector<std::string>& paths) const
        {
            return paths.size() <= EffectRowCount ? 0 : paths.size() - EffectRowCount;
        }

        void revealEffectPath(const std::vector<std::string>& paths, const std::string& path)
        {
            const auto it = std::find(paths.begin(), paths.end(), path);
            if (it == paths.end())
                return;

            const size_t index = static_cast<size_t>(std::distance(paths.begin(), it));
            if (index < effectBrowserOffset)
                effectBrowserOffset = index;
            else if (index >= effectBrowserOffset + EffectRowCount)
                effectBrowserOffset = index - EffectRowCount + 1;
        }

        void
        applyTextFieldFocus(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (wasClicked(interaction, effectNameField.rect))
            {
                if (!hasAsset)
                {
                    state.status = "NO EFFECT";
                    return;
                }

                pushUndoSnapshot();
                activeTextField = ActiveTextField::EffectName;
                effectNameDraft = currentAsset.metadata.name;
                state.status    = "EDIT EFFECT NAME";
                claimKeyboardIfEditing(world);
                return;
            }

            if (wasClicked(interaction, emitterNameField.rect))
            {
                if (selectedEmitter() == nullptr)
                {
                    state.status = "NO EMITTER";
                    return;
                }

                pushUndoSnapshot();
                activeTextField = ActiveTextField::EmitterName;
                syncEmitterNameDraft();
                state.status = "EDIT EMITTER NAME";
                claimKeyboardIfEditing(world);
                return;
            }

            if (wasClicked(interaction, emitterSearchField.rect))
            {
                activeTextField = ActiveTextField::EmitterSearch;
                state.status    = "FILTER EMITTERS";
                claimKeyboardIfEditing(world);
                return;
            }

            if (interaction.clicked != UI_INVALID_HANDLE && activeTextField != ActiveTextField::None)
                finishActiveTextField(state);
        }

        void applyTextInput(EngineToolContext& ctx, EngineToolStateComponent& state)
        {
            if (activeTextField == ActiveTextField::None || ctx.input == nullptr)
                return;

            const std::optional<InputTrigger> trigger = ctx.input->getLastPressedTrigger();
            if (!trigger.has_value() || trigger->type != InputTrigger::Type::Key)
                return;
            if (has(trigger->modifiers, ModifierFlags::Ctrl) || has(trigger->modifiers, ModifierFlags::Alt) ||
                has(trigger->modifiers, ModifierFlags::Super))
            {
                return;
            }

            const GtsKey key = static_cast<GtsKey>(trigger->code);
            if (key == GtsKey::Escape)
            {
                cancelActiveTextField();
                state.status = "EDIT CANCELED";
                return;
            }
            if (key == GtsKey::Enter || key == GtsKey::Tab)
            {
                finishActiveTextField(state);
                return;
            }
            if (key == GtsKey::Backspace)
            {
                std::string& draft = activeTextDraft();
                if (!draft.empty())
                {
                    draft.pop_back();
                    applyActiveTextDraft(state);
                }
                return;
            }

            const bool shift = has(trigger->modifiers, ModifierFlags::Shift);
            const char typed = characterForKey(key, shift);
            if (typed == '\0')
                return;

            std::string& draft = activeTextDraft();
            if (draft.size() >= 48)
                return;
            draft.push_back(typed);
            applyActiveTextDraft(state);
        }

        void applyInspectorSections(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const InspectorSectionBinding& binding : inspectorSections)
            {
                if (!wasClicked(interaction, binding.header.rect))
                    continue;

                selectedInspectorSection = binding.section;
                state.status             = "INSPECT " + inspectorSectionLabel(binding.section);
                return;
            }
        }

        void applyWorkspaceTabs(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const WorkspaceTabBinding& binding : workspaceTabs)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                selectedWorkspaceTab = binding.tab;
                state.status         = "WORKSPACE " + workspaceTabLabel(binding.tab);
                return;
            }
        }

        void claimKeyboardIfEditing(ECSWorld& world)
        {
            if (activeTextField == ActiveTextField::None)
                return;

            EngineToolInputCaptureComponent* capture = nullptr;
            if (world.hasAny<EngineToolInputCaptureComponent>())
                capture = &world.getSingleton<EngineToolInputCaptureComponent>();
            else
                capture = &world.createSingleton<EngineToolInputCaptureComponent>();
            capture->keyboardCaptured = true;
        }

        std::string& activeTextDraft()
        {
            if (activeTextField == ActiveTextField::EffectName)
                return effectNameDraft;
            if (activeTextField == ActiveTextField::EmitterSearch)
                return emitterSearchDraft;
            return emitterNameDraft;
        }

        void applyActiveTextDraft(EngineToolStateComponent& state)
        {
            if (activeTextField == ActiveTextField::EmitterSearch)
            {
                emitterBrowserOffset = 0;
                state.status         = emitterSearchDraft.empty() ? "FILTER CLEARED" : "FILTER EMITTERS";
                return;
            }

            if (activeTextField == ActiveTextField::EffectName)
            {
                currentAsset.metadata.name = effectNameDraft;
                markDirty(state, "EFFECT NAME UPDATED");
                return;
            }

            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
                return;

            selected->name = emitterNameDraft;
            markDirty(state, "EMITTER NAME UPDATED");
        }

        void finishActiveTextField(EngineToolStateComponent& state)
        {
            if (activeTextField == ActiveTextField::EffectName && effectNameDraft.empty())
            {
                effectNameDraft            = std::filesystem::path(currentPath).stem().string();
                currentAsset.metadata.name = effectNameDraft;
                dirty                      = true;
            }
            else if (activeTextField == ActiveTextField::EmitterName && emitterNameDraft.empty())
            {
                emitterNameDraft = "Emitter " + std::to_string(selectedEmitterIndex + 1);
                if (ParticleEffectEmitter* selected = selectedEmitter())
                {
                    selected->name = emitterNameDraft;
                    dirty          = true;
                }
            }

            activeTextField = ActiveTextField::None;
            state.status    = dirty ? "EDIT APPLIED" : "EDIT DONE";
        }

        void cancelActiveTextField()
        {
            if (activeTextField == ActiveTextField::EffectName)
                effectNameDraft = currentAsset.metadata.name;
            else if (activeTextField == ActiveTextField::EmitterName)
                syncEmitterNameDraft();
            activeTextField = ActiveTextField::None;
        }

        static char characterForKey(GtsKey key, bool shift)
        {
            if (key >= GtsKey::A && key <= GtsKey::Z)
            {
                const int  offset = static_cast<int>(key) - static_cast<int>(GtsKey::A);
                const char base   = shift ? 'A' : 'a';
                return static_cast<char>(base + offset);
            }
            if (key >= GtsKey::Digit0 && key <= GtsKey::Digit9)
            {
                const int offset = static_cast<int>(key) - static_cast<int>(GtsKey::Digit0);
                return static_cast<char>('0' + offset);
            }
            if (key == GtsKey::Space)
                return ' ';
            return '\0';
        }

        bool openEffect(ECSWorld&                 world,
                        const std::string&        path,
                        EngineToolStateComponent& state,
                        bool                      protectDirty,
                        bool                      forceReload = false)
        {
            if (protectDirty && dirty && path != currentPath)
            {
                state.status = "SAVE OR RELOAD FIRST";
                return false;
            }
            if (!forceReload && path == currentPath && hasAsset)
                return true;

            clearPlaybackForPath(world, currentPath);

            ParticleEffectAsset loaded;
            if (!gts::particles::loadParticleEffectAsset(path, loaded))
            {
                state.status = "OPEN FAILED";
                return false;
            }

            currentAsset           = std::move(loaded);
            currentPath            = path;
            hasAsset               = true;
            dirty                  = false;
            emitterBrowserOffset   = 0;
            selectedEmitterIndex   = 0;
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            selectedEmitterIds.clear();
            focusEffectSelection();
            selectedRichParameterId.clear();
            selectedRichKeyIndex = 0;
            selectedRichField    = 0;
            selectedRangeParameterId.clear();
            selectedRangeMax = false;
            undoStack.clear();
            redoStack.clear();
            activeUndoHandle  = UI_INVALID_HANDLE;
            playbackPaused    = false;
            playbackTimeScale = 1.0f;
            backgroundPreset  = closestBackgroundPreset(currentAsset.preview.backgroundColor);
            activeTextField   = ActiveTextField::None;
            effectNameDraft   = currentAsset.metadata.name;
            emitterSearchDraft.clear();
            syncEmitterNameDraft();
            autoFrameOpenedEffect();
            state.status = "OPENED " + fileName(path);
            return true;
        }

        void
        applyEffectButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const EffectButtonBinding& binding : effectButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case EffectAction::Save:
                    saveCurrent(state, currentPath);
                    applySelectedEmitterToLive(world, false);
                    break;
                case EffectAction::SaveAs:
                    saveCurrentAs(world, state, "_edited");
                    break;
                case EffectAction::Duplicate:
                    saveCurrentAs(world, state, "_copy");
                    break;
                case EffectAction::Reload:
                    reloadCurrent(world, state);
                    break;
                }
                return;
            }
        }

        void saveCurrent(EngineToolStateComponent& state, const std::string& path)
        {
            if (!hasAsset || path.empty())
            {
                state.status = "NO EFFECT";
                return;
            }

            ParticleEffectAsset asset = assetForSave();
            if (!gts::particles::saveParticleEffectAsset(path, asset))
            {
                state.status = "SAVE FAILED";
                return;
            }

            currentAsset = std::move(asset);
            currentPath  = path;
            dirty        = false;
            state.status = "SAVED " + fileName(path);
        }

        void saveCurrentAs(ECSWorld& world, EngineToolStateComponent& state, const std::string& suffix)
        {
            if (!hasAsset)
            {
                state.status = "NO EFFECT";
                return;
            }

            const std::string oldPath = currentPath;
            const std::string path    = generatedSiblingPath(suffix);
            saveCurrent(state, path);
            if (state.status.rfind("SAVED", 0) == 0)
            {
                if (oldPath != path)
                    clearPlaybackForPath(world, oldPath);
                openEffect(world, path, state, false);
            }
        }

        void reloadCurrent(ECSWorld& world, EngineToolStateComponent& state)
        {
            if (currentPath.empty())
            {
                state.status = "NO EFFECT";
                return;
            }
            openEffect(world, currentPath, state, false, true);
            applySelectedEmitterToLive(world, true);
        }

        ParticleEffectAsset assetForSave() const
        {
            ParticleEffectAsset asset = currentAsset;
            asset.schemaVersion       = CurrentParticleEffectSchemaVersion;
            if (asset.metadata.name.empty())
                asset.metadata.name = std::filesystem::path(currentPath).stem().string();

            for (ParticleEffectEmitter& emitter : asset.emitters)
            {
                gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
                gts::particles::syncParticleGraphWithModules(emitter);
                emitter.compiledProgram = gts::particles::compileParticleEffectEmitter(emitter);
                if (emitter.compiledProgram.valid)
                    emitter.descriptor = emitter.compiledProgram.runtimeDescriptor;
                emitter.descriptor.effectPath.clear();
                emitter.descriptor.effectEmitterId.clear();
                emitter.descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
                emitter.compiledProgram.runtimeDescriptor.effectPath.clear();
                emitter.compiledProgram.runtimeDescriptor.effectEmitterId.clear();
            }
            return asset;
        }

        void applyEmitterRows(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (!hasAsset)
                return;

            visibleOutlinerItems = buildOutlinerItems();
            clampOutlinerOffset();
            if (interaction.scrollY != 0.0f && pointerOverEmitterRows(interaction))
            {
                const size_t maxOffset = maxOutlinerOffset();
                if (interaction.scrollY < 0.0f)
                    emitterBrowserOffset = std::min(emitterBrowserOffset + 1, maxOffset);
                else if (emitterBrowserOffset > 0)
                    emitterBrowserOffset -= 1;
            }

            const size_t rowOffset = outlinerRowOffset();
            if (interaction.pressed == UI_INVALID_HANDLE)
                emitterDragActive = false;
            for (size_t i = 0; i < emitterRows.size() && rowOffset + i < visibleOutlinerItems.size(); ++i)
            {
                const OutlinerItem& item = visibleOutlinerItems[rowOffset + i];
                if (item.selection.kind == FxSelectionKind::Emitter && interaction.pressed == emitterRows[i].rect)
                {
                    const size_t assetIndex = item.selection.emitterIndex;
                    if (!emitterDragActive || emitterDragIndex != assetIndex)
                    {
                        emitterDragActive = true;
                        emitterDragIndex  = assetIndex;
                        emitterDragLastY  = interaction.pointerY;
                    }

                    const float deltaY = interaction.pointerY - emitterDragLastY;
                    if (std::abs(deltaY) >= 0.035f)
                    {
                        selectedEmitterIndex = assetIndex;
                        moveEmitter(state, deltaY > 0.0f ? 1 : -1);
                        emitterDragIndex = selectedEmitterIndex;
                        emitterDragLastY = interaction.pointerY;
                        syncEmitterNameDraft();
                        applySelectedEmitterToLive(world, true);
                        return;
                    }
                }

                if (wasClicked(interaction, emitterRows[i].rect))
                {
                    selectOutlinerItem(world, state, item.selection, false);
                    return;
                }
            }
        }

        void
        applyEmitterButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const EmitterButtonBinding& binding : emitterButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case EmitterAction::Add:
                    addEmitter(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                case EmitterAction::Delete:
                    deleteEmitter(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                case EmitterAction::Duplicate:
                    duplicateEmitter(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                case EmitterAction::Rename:
                    renameEmitter(state);
                    break;
                case EmitterAction::MoveUp:
                    moveEmitter(state, -1);
                    break;
                case EmitterAction::MoveDown:
                    moveEmitter(state, 1);
                    break;
                case EmitterAction::ToggleEnabled:
                    toggleEmitterEnabled(state);
                    applySelectedEmitterToLive(world, false);
                    break;
                case EmitterAction::ToggleSelection:
                    toggleSelectedEmitterSelection(state);
                    break;
                case EmitterAction::Copy:
                    copyEmitters(state);
                    break;
                case EmitterAction::Paste:
                    pasteEmitters(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                }
                return;
            }
        }

        void applyModuleRows(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return;

            clampModuleSelection(*emitter);
            if (interaction.scrollY != 0.0f && pointerOverModuleRows(interaction))
            {
                const size_t maxOffset = maxModuleBrowserOffset(*emitter);
                if (interaction.scrollY < 0.0f)
                    moduleBrowserOffset = std::min(moduleBrowserOffset + 1, maxOffset);
                else if (moduleBrowserOffset > 0)
                    moduleBrowserOffset -= 1;
            }

            for (size_t i = 0; i < moduleRows.size() && moduleBrowserOffset + i < emitter->modules.size(); ++i)
            {
                if (!wasClicked(interaction, moduleRows[i].rect))
                    continue;

                selectedModuleIndex    = moduleBrowserOffset + i;
                parameterControlOffset = 0;
                clearParameterSelectionState();
                focusModuleSelection();
                state.status = selectedModuleStatus(emitter->modules[selectedModuleIndex]);
                return;
            }
        }

        void
        applyModuleButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const ModuleButtonBinding& binding : moduleButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case ModuleAction::Copy:
                    copyModule(state);
                    break;
                case ModuleAction::Paste:
                    pasteModule(world, state);
                    break;
                case ModuleAction::Undo:
                    undo(world, state);
                    break;
                case ModuleAction::Redo:
                    redo(world, state);
                    break;
                }
                return;
            }
        }

        void applyGraphButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const GraphButtonBinding& binding : graphButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case GraphAction::Search:
                    searchNextGraphNode(state);
                    break;
                case GraphAction::AddNode:
                    addSearchedGraphNode(world, state);
                    break;
                case GraphAction::Link:
                    toggleSelectedGraphLink(world, state);
                    break;
                case GraphAction::Frame:
                    frameSelectedGraphNode(state);
                    break;
                case GraphAction::Comment:
                    commentSelectedGraphNode(state);
                    break;
                }
                return;
            }
        }

        void searchNextGraphNode(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            gts::particles::syncParticleGraphWithModules(*emitter);
            const std::vector<ParticleModuleDefinition>& definitions = gts::particles::particleModuleDefinitions();
            if (definitions.empty())
            {
                state.status = "NO NODE TYPES";
                return;
            }

            graphSearchIndex                           = (graphSearchIndex + 1u) % definitions.size();
            const ParticleModuleDefinition& definition = definitions[graphSearchIndex];
            if (const ParticleGraphNode* node =
                    gts::particles::findParticleGraphNodeForType(emitter->graph, definition.typeId))
            {
                selectModuleByStableId(*emitter, node->moduleStableId);
                state.status = "FOUND " + definition.displayName;
                return;
            }

            state.status = "MISSING " + definition.displayName;
        }

        void addSearchedGraphNode(ECSWorld& world, EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            const std::vector<ParticleModuleDefinition>& definitions = gts::particles::particleModuleDefinitions();
            if (definitions.empty())
            {
                state.status = "NO NODE TYPES";
                return;
            }

            const ParticleModuleDefinition& definition = definitions[graphSearchIndex % definitions.size()];
            if (gts::particles::findParticleGraphNodeForType(emitter->graph, definition.typeId) != nullptr)
            {
                state.status = "NODE EXISTS";
                return;
            }

            pushUndoSnapshot();
            if (!gts::particles::addParticleGraphNode(*emitter, definition))
            {
                state.status = "NODE ADD FAILED";
                return;
            }
            if (const ParticleGraphNode* node =
                    gts::particles::findParticleGraphNodeForType(emitter->graph, definition.typeId))
                selectModuleByStableId(*emitter, node->moduleStableId);
            syncSelectedEmitterDescriptor(*emitter);
            markDirty(state, "NODE ADDED");
            applySelectedEmitterToLive(world, true);
        }

        void toggleSelectedGraphLink(ECSWorld& world, EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }
            ParticleGraphNode* node = selectedGraphNode(*emitter);
            if (node == nullptr)
            {
                state.status = "NO NODE";
                return;
            }

            pushUndoSnapshot();
            if (!gts::particles::toggleParticleGraphDependencyLink(*emitter, node->id))
            {
                state.status = "NO LINK TARGET";
                return;
            }
            syncSelectedEmitterDescriptor(*emitter);
            markDirty(state, "LINK TOGGLED");
            applySelectedEmitterToLive(world, false);
        }

        void frameSelectedGraphNode(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }
            ParticleGraphNode* node = selectedGraphNode(*emitter);
            if (node == nullptr)
            {
                state.status = "NO NODE";
                return;
            }

            pushUndoSnapshot();
            if (!gts::particles::addParticleGraphFrameForNode(*emitter, node->id))
            {
                state.status = "FRAME FAILED";
                return;
            }
            markDirty(state, "FRAME ADDED");
        }

        void commentSelectedGraphNode(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }
            ParticleGraphNode* node = selectedGraphNode(*emitter);
            if (node == nullptr)
            {
                state.status = "NO NODE";
                return;
            }

            pushUndoSnapshot();
            if (!gts::particles::addParticleGraphCommentForNode(*emitter, node->id))
            {
                state.status = "NOTE FAILED";
                return;
            }
            markDirty(state, "NOTE ADDED");
        }

        void applyModuleParameterControls(UiSystem&                  ui,
                                          ECSWorld&                  world,
                                          EngineToolStateComponent&  state,
                                          const UiInteractionResult& interaction)
        {
            ParticleEffectEmitter*  emitter = selectedEmitter();
            ParticleModuleInstance* module  = selectedModule();
            if (emitter == nullptr || module == nullptr)
                return;

            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module->typeId);
            if (definition == nullptr)
                return;

            const std::vector<ParameterView> parameters = editableParameters(*definition);
            clampParameterControlOffset(parameters);
            if (interaction.scrollY != 0.0f && pointerOverParameterControls(interaction))
            {
                const size_t maxOffset = maxParameterControlOffset(parameters);
                if (interaction.scrollY < 0.0f)
                    parameterControlOffset = std::min(parameterControlOffset + 1, maxOffset);
                else if (parameterControlOffset > 0)
                    parameterControlOffset -= 1;
            }

            for (size_t row = 0; row < parameterControls.size() && parameterControlOffset + row < parameters.size();
                 ++row)
            {
                ParameterControl&    control = parameterControls[row];
                const ParameterView& view    = parameters[parameterControlOffset + row];
                if (applyParameterViewControl(ui, world, state, interaction, *emitter, *module, view, control))
                    return;
            }
        }

        void addEmitter(EngineToolStateComponent& state)
        {
            pushUndoSnapshot();
            ParticleEffectEmitter emitter;
            emitter.stableId = uniqueEmitterId("emitter");
            emitter.name     = "Emitter " + std::to_string(currentAsset.emitters.size() + 1);
            emitter.descriptor.effectEmitterId.clear();
            emitter.descriptor.effectPath.clear();
            emitter.modules = buildModulesFromEmitterDescriptor(emitter.descriptor);
            currentAsset.emitters.push_back(std::move(emitter));
            selectedEmitterIndex = currentAsset.emitters.size() - 1;
            selectedEmitterIds.clear();
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTER ADDED");
        }

        void deleteEmitter(EngineToolStateComponent& state)
        {
            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            if (!targets.empty())
            {
                deleteSelectedEmitters(state, targets);
                return;
            }

            if (currentAsset.emitters.size() <= 1)
            {
                state.status = "KEEP ONE EMITTER";
                return;
            }
            pushUndoSnapshot();
            selectedEmitterIndex = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            currentAsset.emitters.erase(currentAsset.emitters.begin() +
                                        static_cast<std::ptrdiff_t>(selectedEmitterIndex));
            if (selectedEmitterIndex >= currentAsset.emitters.size())
                selectedEmitterIndex = currentAsset.emitters.size() - 1;
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTER DELETED");
        }

        void duplicateEmitter(EngineToolStateComponent& state)
        {
            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            if (!targets.empty())
            {
                duplicateSelectedEmitters(state, targets);
                return;
            }

            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            pushUndoSnapshot();
            ParticleEffectEmitter copy = *selected;
            copy.stableId              = uniqueEmitterId(selected->stableId.empty() ? "emitter" : selected->stableId);
            copy.name                  = selected->name + " Copy";
            currentAsset.emitters.insert(
                currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(selectedEmitterIndex + 1), std::move(copy));
            selectedEmitterIndex += 1;
            selectedEmitterIds.clear();
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTER COPIED");
        }

        void renameEmitter(EngineToolStateComponent& state)
        {
            if (selectedEmitter() == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            pushUndoSnapshot();
            syncEmitterNameDraft();
            activeTextField = ActiveTextField::EmitterName;
            state.status    = "EDIT EMITTER NAME";
        }

        void moveEmitter(EngineToolStateComponent& state, int delta)
        {
            if (currentAsset.emitters.empty())
                return;

            const int current = static_cast<int>(selectedEmitterIndex);
            const int next    = std::clamp(current + delta, 0, static_cast<int>(currentAsset.emitters.size()) - 1);
            if (next == current)
            {
                state.status = delta < 0 ? "ALREADY FIRST" : "ALREADY LAST";
                return;
            }

            if (!selectedEmitterIds.empty())
            {
                state.status = "SELECT ONE TO MOVE";
                return;
            }

            pushUndoSnapshot();
            std::swap(currentAsset.emitters[static_cast<size_t>(current)],
                      currentAsset.emitters[static_cast<size_t>(next)]);
            selectedEmitterIndex = static_cast<size_t>(next);
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTER MOVED");
        }

        void toggleEmitterEnabled(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            pushUndoSnapshot();
            const bool enabled = !selected->descriptor.enabled;
            if (targets.empty())
            {
                selected->descriptor.enabled = enabled;
                setEmitterEnabledModuleParameter(*selected, selected->descriptor.enabled);
            }
            else
            {
                for (size_t index : targets)
                {
                    currentAsset.emitters[index].descriptor.enabled = enabled;
                    setEmitterEnabledModuleParameter(currentAsset.emitters[index], enabled);
                }
            }
            markDirty(state, enabled ? "EMITTER ENABLED" : "EMITTER DISABLED");
        }

        void
        applyPlaybackButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const PlaybackButtonBinding& binding : playbackButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case PlaybackAction::PlayPause:
                    playbackPaused = !playbackPaused;
                    state.status   = playbackPaused ? "PREVIEW PAUSED" : "PREVIEW PLAYING";
                    applyPlaybackToLive(world);
                    break;
                case PlaybackAction::Restart:
                    restartLiveEffect(world);
                    state.status = "PREVIEW RESTARTED";
                    break;
                case PlaybackAction::Background:
                    cycleBackground(state);
                    break;
                case PlaybackAction::CameraReset:
                    resetPreviewCamera(state);
                    break;
                }
                return;
            }
        }

        void
        applyPreviewButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const PreviewButtonBinding& binding : previewButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case PreviewAction::TogglePan:
                    previewPanMode = !previewPanMode;
                    state.status   = previewPanMode ? "PREVIEW PAN MODE" : "PREVIEW ORBIT MODE";
                    break;
                case PreviewAction::FrameSelection:
                    frameSelectedEmitterCamera(state, true);
                    break;
                case PreviewAction::FrameEffect:
                    frameEntireEffectCamera(state, true);
                    break;
                case PreviewAction::ResetCamera:
                    resetPreviewCamera(state);
                    break;
                case PreviewAction::ToggleGrid:
                    previewGridVisible = !previewGridVisible;
                    state.status       = previewGridVisible ? "GRID SHOWN" : "GRID HIDDEN";
                    break;
                case PreviewAction::ToggleOrigin:
                    previewOriginVisible = !previewOriginVisible;
                    state.status         = previewOriginVisible ? "ORIGIN SHOWN" : "ORIGIN HIDDEN";
                    break;
                case PreviewAction::ToggleScale:
                    previewScaleVisible = !previewScaleVisible;
                    state.status        = previewScaleVisible ? "SCALE SHOWN" : "SCALE HIDDEN";
                    break;
                case PreviewAction::ToggleBounds:
                    previewBoundsVisible = !previewBoundsVisible;
                    state.status         = previewBoundsVisible ? "BOUNDS SHOWN" : "BOUNDS HIDDEN";
                    break;
                case PreviewAction::ToggleGizmos:
                    previewGizmosVisible = !previewGizmosVisible;
                    state.status         = previewGizmosVisible ? "GIZMOS SHOWN" : "GIZMOS HIDDEN";
                    break;
                case PreviewAction::ToggleCollision:
                    previewCollisionVisible = !previewCollisionVisible;
                    state.status            = previewCollisionVisible ? "COLLISION SHOWN" : "COLLISION HIDDEN";
                    break;
                case PreviewAction::ToggleCulling:
                    previewCullingVisible = !previewCullingVisible;
                    state.status          = previewCullingVisible ? "CULLING SHOWN" : "CULLING HIDDEN";
                    break;
                }

                applyPlaybackToLive(world);
                return;
            }
        }

        void applyPreviewOrbitDrag(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (interaction.pressed != previewSwatch)
            {
                previewOrbitDragActive = false;
                return;
            }

            if (!previewOrbitDragActive)
            {
                previewOrbitDragActive = true;
                previewOrbitDragLastX  = interaction.pointerX;
                previewOrbitDragLastY  = interaction.pointerY;
                return;
            }

            const float deltaX    = interaction.pointerX - previewOrbitDragLastX;
            const float deltaY    = interaction.pointerY - previewOrbitDragLastY;
            previewOrbitDragLastX = interaction.pointerX;
            previewOrbitDragLastY = interaction.pointerY;
            if (std::abs(deltaX) < 0.0001f && std::abs(deltaY) < 0.0001f)
                return;

            beginUndoForHandle(previewSwatch);
            if (previewPanMode)
            {
                panPreviewCamera(deltaX, deltaY);
                markDirty(state, "PREVIEW PAN");
            }
            else
            {
                setPreviewOrbitYawDegrees(previewOrbitYawDegrees() + deltaX * 240.0f);
                markDirty(state, "PREVIEW ORBIT");
            }
        }

        void applyPreviewZoom(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (interaction.scrollY == 0.0f || !pointerOverPreview(interaction))
                return;

            beginUndoForHandle(previewSwatch);
            const float direction = interaction.scrollY > 0.0f ? -1.0f : 1.0f;
            setPreviewOrbitDistance(currentAsset.preview.orbitDistance * (1.0f + direction * 0.10f));
            markDirty(state, "PREVIEW ZOOM");
        }

        void applySliders(UiSystem&                  ui,
                          ECSWorld&                  world,
                          EngineToolStateComponent&  state,
                          const UiInteractionResult& interaction)
        {
            for (const SliderBinding& binding : sliders)
            {
                if (!isPressed(interaction, binding.slider.track))
                    continue;

                const float value = valueFromSliderPointer(ui, binding.slider, interaction.pointerX);
                if (binding.field == FloatField::TimeScale)
                {
                    playbackTimeScale = std::max(0.0f, value);
                    applyPlaybackToLive(world);
                    state.status = "TIME SCALE";
                    return;
                }
                if (binding.field == FloatField::OrbitYaw)
                {
                    beginUndoForHandle(binding.slider.track);
                    setPreviewOrbitYawDegrees(value);
                    markDirty(state, "PREVIEW CAMERA");
                    return;
                }
                if (binding.field == FloatField::OrbitDistance)
                {
                    beginUndoForHandle(binding.slider.track);
                    setPreviewOrbitDistance(value);
                    markDirty(state, "PREVIEW DISTANCE");
                    return;
                }
            }
        }

        void cycleBackground(EngineToolStateComponent& state)
        {
            pushUndoSnapshot();
            backgroundPreset = (backgroundPreset + 1u) % static_cast<uint32_t>(backgroundPresets().size());
            currentAsset.preview.backgroundColor = backgroundPresets()[backgroundPreset];
            markDirty(state, "BACKGROUND UPDATED");
        }

        void resetPreviewCamera(EngineToolStateComponent& state)
        {
            pushUndoSnapshot();
            currentAsset.preview.cameraPosition = {0.0f, 1.0f, 4.0f};
            currentAsset.preview.cameraTarget   = {0.0f, 0.6f, 0.0f};
            currentAsset.preview.orbitDistance  = 4.0f;
            markDirty(state, "CAMERA RESET");
        }

        void applyPlaybackToLive(ECSWorld& world)
        {
            (void)world;
        }

        void clearPlaybackForPath(ECSWorld& world, const std::string& path)
        {
            (void)world;
            (void)path;
        }

        void restartLiveEffect(ECSWorld& world)
        {
            (void)world;
            previewWorld.resetSimulation();
        }

        void applySelectedEmitterToLive(ECSWorld& world, bool restart)
        {
            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr || currentPath.empty())
                return;

            syncSelectedEmitterDescriptor(*selected);
            (void)world;
            if (restart)
                previewWorld.resetSimulation();
        }

        bool assetHasEmitter(const std::string& stableId) const
        {
            for (const ParticleEffectEmitter& emitter : currentAsset.emitters)
            {
                if (emitter.stableId == stableId)
                    return true;
            }
            return false;
        }

        void updateDisplay(EngineToolContext&              ctx,
                           EngineToolStateComponent&       state,
                           const std::vector<std::string>& effectPaths)
        {
            updatePanelFrame(
                ctx.ui, toolbarFrame, "FX Editor", hasAsset ? graphStatusLabel() : std::string("Asset Authoring"));
            updatePanelFrame(ctx.ui,
                             hierarchyFrame,
                             "Hierarchy",
                             hasAsset ? std::to_string(currentAsset.emitters.size()) + " emitters"
                                      : "No effect loaded");
            updatePanelFrame(ctx.ui, previewFrame, "Viewport", playbackPaused ? "Paused" : "Playing");
            updatePanelFrame(ctx.ui, inspectorFrame, "Inspector", selectionContextLabel());
            updatePanelFrame(ctx.ui, workspaceFrame, "Workspace", workspaceTabLabel(selectedWorkspaceTab));

            setText(ctx.ui, header, toolbarTitleText());
            setText(ctx.ui, summary, effectSummary());
            setRectColor(ctx.ui, previewSwatch, previewColor());
            ECSWorld& previewStatsWorld = previewWorld.ecsWorld();
            const std::string previewMessage = previewStateMessage(previewStatsWorld);
            setText(ctx.ui, previewText, previewMessage);
            setTextColor(ctx.ui, previewText, previewStateColor(previewStatsWorld));
            setVisibleRecursive(ctx.ui, previewText, !previewMessage.empty());
            setText(ctx.ui, previewCameraText, previewCameraLabel());
            setText(ctx.ui, inspectorHeader, inspectorObjectTitle());
            setText(ctx.ui, footer, statusBarText(ctx, state));
            updateNotification(ctx, state.status);
            syncTextDraftsForDisplay();
            updateSectionHeader(
                ctx.ui, effectListHeader, "Assets", std::to_string(effectPaths.size()) + " files", false);
            updateTextField(
                ctx.ui,
                effectNameField,
                textFieldDisplay(hasAsset ? effectNameDraft : "--", activeTextField == ActiveTextField::EffectName),
                activeTextField == ActiveTextField::EffectName);
            updateTextField(ctx.ui,
                            emitterNameField,
                            textFieldDisplay(selectedEmitter() == nullptr ? "--" : emitterNameDraft,
                                             activeTextField == ActiveTextField::EmitterName),
                            activeTextField == ActiveTextField::EmitterName);

            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                const size_t      effectIndex = effectBrowserOffset + i;
                const bool        hasRow      = effectIndex < effectPaths.size();
                const std::string label       = hasRow ? "Open  " + fileName(effectPaths[effectIndex])
                                                       : (i == 0 && effectPaths.empty() ? "No particle assets" : "");
                updateButton(ctx.ui, effectRows[i], label);
                setVisibleRecursive(ctx.ui, effectRows[i].rect, hasRow || i == 0);
                if (hasRow && effectPaths[effectIndex] == currentPath)
                    setRectColor(ctx.ui, effectRows[i].rect, ToolTheme::buttonActive);
                else if (!hasRow)
                    setRectColor(ctx.ui, effectRows[i].rect, ToolTheme::disabled);
            }

            visibleOutlinerItems = buildOutlinerItems();
            clampOutlinerOffset();
            updateSectionHeader(ctx.ui,
                                emitterListHeader,
                                "Outliner",
                                hasAsset ? selectionContextLabel() : "Load an effect",
                                false);
            updateTextField(ctx.ui,
                            emitterSearchField,
                            textFieldDisplay(emitterSearchDraft.empty() ? std::string{"All"} : emitterSearchDraft,
                                             activeTextField == ActiveTextField::EmitterSearch),
                            activeTextField == ActiveTextField::EmitterSearch);
            setVisibleRecursive(ctx.ui, emitterNameField.label, false);
            setVisibleRecursive(ctx.ui, emitterNameField.rect, false);
            const size_t rowOffset = outlinerRowOffset();
            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                const bool        hasRow = rowOffset + i < visibleOutlinerItems.size();
                const OutlinerItem* item =
                    hasRow ? &visibleOutlinerItems[rowOffset + i] : nullptr;
                const std::string label = item != nullptr ? outlinerRowLabel(*item)
                                                          : (i == 0 ? outlinerEmptyStateLabel() : "");
                updateButton(ctx.ui, emitterRows[i], label);
                setVisibleRecursive(ctx.ui, emitterRows[i].rect, hasRow || i == 0);
                if (item != nullptr && selectionMatches(item->selection))
                    setRectColor(ctx.ui, emitterRows[i].rect, ToolTheme::buttonActive);
                else if (item != nullptr && !item->enabled)
                    setRectColor(ctx.ui, emitterRows[i].rect, ToolTheme::disabled);
                else if (item != nullptr && item->depth >= 2)
                    setRectColor(ctx.ui, emitterRows[i].rect, ToolTheme::panelInset);
                else if (!hasRow)
                    setRectColor(ctx.ui, emitterRows[i].rect, ToolTheme::disabled);
            }

            updateModuleRows(ctx.ui);
            updateParameterControls(ctx.ui);
            updateInspectorSections(ctx.ui);
            updateInspectorStatusRows(ctx.ui, previewStatsWorld);
            updateWorkspace(ctx.ui, previewStatsWorld);

            for (const EffectButtonBinding& binding : effectButtons)
                updateButton(ctx.ui, binding.button, effectButtonLabel(binding.action));
            for (const EmitterButtonBinding& binding : emitterButtons)
                updateButton(ctx.ui, binding.button, emitterButtonLabel(binding.action));
            for (const ModuleButtonBinding& binding : moduleButtons)
                updateButton(ctx.ui, binding.button, moduleButtonLabel(binding.action));
            for (const GraphButtonBinding& binding : graphButtons)
                updateButton(ctx.ui, binding.button, graphButtonLabel(binding.action));
            for (const PlaybackButtonBinding& binding : playbackButtons)
                updateButton(ctx.ui, binding.button, playbackButtonLabel(binding.action));
            updatePreviewControls(ctx.ui, previewStatsWorld);
            applyActionHierarchy(ctx.ui);

            for (const SliderBinding& binding : sliders)
                updateSlider(ctx.ui, binding.slider, readSliderValue(binding.field), sliderColor(binding.field));

            syncInspectorSectionVisibility(ctx.ui);
        }

        std::string statusBarText(EngineToolContext& ctx, const EngineToolStateComponent& state)
        {
            const LiveEffectStats stats = liveEffectStats(previewWorld.ecsWorld());
            const float fps = ctx.time == nullptr || ctx.time->unscaledDeltaTime <= 0.0001f
                                  ? 0.0f
                                  : 1.0f / ctx.time->unscaledDeltaTime;
            const UiSystem::Metrics uiMetrics = ctx.ui.getLastMetrics();

            std::ostringstream out;
            out << "Effect: ";
            out << (hasAsset ? compact(currentAsset.metadata.name.empty() ? fileName(currentPath)
                                                                           : currentAsset.metadata.name,
                                       28)
                             : "--");
            out << "  |  Asset: " << (currentPath.empty() ? "--" : compact(fileName(currentPath), 30));
            out << "  |  Auto Reload: On";
            out << "  |  Mem: -- MB";
            out << "  |  Draw Calls: " << uiMetrics.commandCount;
            out << "  |  Particles: " << stats.liveParticles;
            out << "  |  " << (fps <= 0.0f ? std::string{"--"} : formatFloat(fps)) << " fps";
            out << "  |  " << graphStatusLabel();
            if (!state.status.empty())
                out << "  |  " << compact(displayStatusText(state.status), 34);
            return out.str();
        }

        void applyActionHierarchy(UiSystem& ui)
        {
            for (const EffectButtonBinding& binding : effectButtons)
            {
                if (binding.action == EffectAction::Save)
                    setRectColor(ui, binding.button.rect, dirty ? ToolTheme::accent : ToolTheme::buttonActive);
                else if (binding.action == EffectAction::SaveAs)
                    setRectColor(ui, binding.button.rect, ToolTheme::accentSoft);
            }

            for (const PlaybackButtonBinding& binding : playbackButtons)
            {
                if (binding.action == PlaybackAction::PlayPause)
                    setRectColor(ui, binding.button.rect, playbackPaused ? ToolTheme::success : ToolTheme::buttonActive);
                else if (binding.action == PlaybackAction::Restart)
                    setRectColor(ui, binding.button.rect, ToolTheme::accentSoft);
            }

            for (const EmitterButtonBinding& binding : emitterButtons)
            {
                const bool emitterSelected = selectedObject.kind == FxSelectionKind::Emitter;
                const bool visible = binding.action == EmitterAction::Add ||
                                     (emitterSelected &&
                                      (binding.action == EmitterAction::ToggleEnabled ||
                                       binding.action == EmitterAction::Delete ||
                                       binding.action == EmitterAction::Duplicate));
                setVisibleRecursive(ui, binding.button.rect, visible);
                if (!visible)
                    continue;

                if (binding.action == EmitterAction::Add)
                    setRectColor(ui, binding.button.rect, ToolTheme::buttonActive);
                else if (binding.action == EmitterAction::Duplicate)
                    setRectColor(ui, binding.button.rect, ToolTheme::accentSoft);
                else if (binding.action == EmitterAction::Delete)
                    setRectColor(ui, binding.button.rect, ToolTheme::error);
                else if (binding.action == EmitterAction::Paste && emitterClipboard.empty())
                    setRectColor(ui, binding.button.rect, ToolTheme::disabled);
            }

            for (const ModuleButtonBinding& binding : moduleButtons)
            {
                if ((binding.action == ModuleAction::Undo && undoStack.empty()) ||
                    (binding.action == ModuleAction::Redo && redoStack.empty()) ||
                    (binding.action == ModuleAction::Paste && !moduleClipboard.has_value()))
                {
                    setRectColor(ui, binding.button.rect, ToolTheme::disabled);
                }
            }
        }

        void updateModuleRows(UiSystem& ui)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                for (size_t i = 0; i < moduleRows.size(); ++i)
                {
                    updateButton(ui, moduleRows[i], i == 0 ? "No emitter selected" : "");
                    setRectColor(ui, moduleRows[i].rect, ToolTheme::disabled);
                }
                return;
            }
            if (emitter->modules.empty())
            {
                for (size_t i = 0; i < moduleRows.size(); ++i)
                {
                    updateButton(ui, moduleRows[i], i == 0 ? "No modules on emitter" : "");
                    setRectColor(ui, moduleRows[i].rect, ToolTheme::disabled);
                }
                return;
            }

            clampModuleSelection(*emitter);
            const size_t rowOffset = moduleRowOffset(*emitter);
            for (size_t i = 0; i < moduleRows.size(); ++i)
            {
                const size_t      moduleIndex = rowOffset + i;
                const bool        hasRow      = moduleIndex < emitter->modules.size();
                const std::string label       = hasRow ? moduleDisplayName(emitter->modules[moduleIndex]) : "";
                updateButton(ui, moduleRows[i], label);
                if (hasRow && moduleIndex == selectedModuleIndex)
                    setRectColor(ui, moduleRows[i].rect, ToolTheme::buttonActive);
            }
        }

        void updateParameterControls(UiSystem& ui)
        {
            ParticleModuleInstance*         module = selectedModule();
            const ParticleModuleDefinition* definition =
                module == nullptr ? nullptr : findParticleModuleDefinition(module->typeId);
            if (module == nullptr || definition == nullptr)
            {
                showParameterEmptyState(ui, module == nullptr ? "No module selected" : "Unknown module type");
                return;
            }

            const std::vector<ParameterView> parameters = editableParameters(*definition);
            if (parameters.empty())
            {
                showParameterEmptyState(ui, "No editable parameters");
                return;
            }
            clampParameterControlOffset(parameters);
            for (size_t row = 0; row < parameterControls.size(); ++row)
            {
                const size_t      parameterIndex = parameterControlOffset + row;
                ParameterControl& control        = parameterControls[row];
                if (parameterIndex >= parameters.size())
                {
                    setParameterControlVisible(ui, control, false, false, false, false, false, false, false);
                    continue;
                }

                updateParameterViewControl(ui, *module, parameters[parameterIndex], control);
            }
        }

        void showParameterEmptyState(UiSystem& ui, const std::string& message)
        {
            for (size_t i = 0; i < parameterControls.size(); ++i)
            {
                ParameterControl& control = parameterControls[i];
                setParameterControlVisible(ui, control, false, false, false, false, false, false, i == 0);
                if (i == 0)
                {
                    updateButton(ui, control.button, message);
                    setRectColor(ui, control.button.rect, ToolTheme::disabled);
                    setTextColor(ui, control.button.label, ToolTheme::mutedText);
                }
            }
        }

        std::string effectSummary() const
        {
            if (!hasAsset)
                return "Open from scene";

            std::ostringstream out;
            out << currentAsset.emitters.size() << " emitter" << (currentAsset.emitters.size() == 1 ? "" : "s") << "  "
                << graphStatusLabel();
            return compact(out.str(), 26);
        }

        std::string toolbarTitleText() const
        {
            if (!hasAsset)
                return "Particle FX";

            const std::string name =
                currentAsset.metadata.name.empty() ? fileName(currentPath) : currentAsset.metadata.name;
            return compact(name + (dirty ? " *" : ""), 26);
        }

        struct GraphDiagnosticCounts
        {
            size_t errors   = 0;
            size_t warnings = 0;
        };

        std::string graphStatusLabel() const
        {
            const GraphDiagnosticCounts counts = selectedGraphDiagnosticCounts();
            if (counts.errors > 0)
                return "Graph " + std::to_string(counts.errors) + " errors";
            if (counts.warnings > 0)
                return "Graph " + std::to_string(counts.warnings) + " warnings";
            return "Graph OK";
        }

        GraphDiagnosticCounts selectedGraphDiagnosticCounts() const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return {};

            std::vector<gts::particles::ParticleModuleGraphDiagnostic> diagnostics;
            gts::particles::validateParticleEffectGraph(*emitter, &diagnostics);

            GraphDiagnosticCounts counts;
            for (const gts::particles::ParticleModuleGraphDiagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.severity == gts::particles::ParticleModuleGraphDiagnosticSeverity::Error)
                    counts.errors += 1;
                else
                    counts.warnings += 1;
            }
            return counts;
        }

        struct LiveEffectStats
        {
            size_t              liveEmitters                  = 0;
            size_t              liveParticles                 = 0;
            uint32_t            spawnedThisFrame              = 0;
            uint32_t            diedThisFrame                 = 0;
            uint32_t            collisionEventsThisFrame      = 0;
            uint32_t            eventSpawnsThisFrame          = 0;
            uint32_t            budgetSkippedSpawnsThisFrame  = 0;
            bool                selectedRuntimeFound          = false;
            bool                selectedRuntimeVisible        = true;
            ParticleCullReason  selectedCullReason            = ParticleCullReason::None;
            float               selectedEmitterAge            = 0.0f;
            float               selectedDistanceToCamera      = 0.0f;
            float               selectedLodSpawnScale         = 1.0f;
            float               selectedLodRenderScale        = 1.0f;
            uint32_t            selectedBudgetedMaxParticles  = 0;
            uint32_t            selectedBudgetedSpawnPerFrame = 0;
            bool                hasFrameData                  = false;
            ParticleFrameData   frameData;
            bool                hasBudget = false;
            ParticleBudgetState budget;
        };

        LiveEffectStats liveEffectStats(ECSWorld& world) const
        {
            LiveEffectStats              stats;
            const ParticleEffectEmitter* selected         = selectedEmitter();
            const std::string            selectedStableId = selected == nullptr ? std::string{} : selected->stableId;

            world.forEach<ParticleEmitterComponent, ParticleEmitterRuntimeComponent>(
                [&](Entity entity, ParticleEmitterComponent& emitter, ParticleEmitterRuntimeComponent& runtime)
                {
                    (void)entity;
                    if (emitter.effectPath != currentPath)
                        return;

                    stats.liveEmitters += 1;
                    stats.liveParticles += runtime.particles.size();
                    stats.spawnedThisFrame += runtime.spawnedThisFrame;
                    stats.diedThisFrame += runtime.diedThisFrame;
                    stats.collisionEventsThisFrame += runtime.collisionEventsThisFrame;
                    stats.eventSpawnsThisFrame += runtime.eventSpawnsThisFrame;
                    stats.budgetSkippedSpawnsThisFrame += runtime.budgetSkippedSpawnsThisFrame;

                    const bool selectedRuntime =
                        !selectedStableId.empty() && (emitter.effectEmitterId == selectedStableId ||
                                                      (emitter.effectEmitterId.empty() && selectedEmitterIndex == 0));
                    if (!selectedRuntime)
                        return;
                    if (stats.selectedRuntimeFound)
                        return;

                    stats.selectedRuntimeFound          = true;
                    stats.selectedRuntimeVisible        = runtime.visible;
                    stats.selectedCullReason            = runtime.cullReason;
                    stats.selectedEmitterAge            = runtime.emitterAge;
                    stats.selectedDistanceToCamera      = runtime.distanceToCamera;
                    stats.selectedLodSpawnScale         = runtime.lodSpawnScale;
                    stats.selectedLodRenderScale        = runtime.lodRenderScale;
                    stats.selectedBudgetedMaxParticles  = runtime.budgetedMaxParticles;
                    stats.selectedBudgetedSpawnPerFrame = runtime.budgetedSpawnPerFrame;
                });

            if (world.hasAny<ParticleFrameDataComponent>())
            {
                stats.hasFrameData = true;
                stats.frameData    = world.getSingleton<ParticleFrameDataComponent>().frameData;
            }
            if (world.hasAny<ParticleBudgetComponent>())
            {
                stats.hasBudget = true;
                stats.budget    = world.getSingleton<ParticleBudgetComponent>().state;
            }
            return stats;
        }

        std::string previewStateMessage(ECSWorld& world) const
        {
            if (!hasAsset)
                return "No effect loaded.\nSelect an effect asset from the hierarchy.";
            if (assetFileMissing())
                return "Asset missing.\nSave As or reload from a valid path.";
            if (currentAsset.emitters.empty())
                return "No emitters.\nAdd an emitter in the hierarchy.";
            if (!hasEnabledEmitter())
                return "No emitters enabled.\nEnable an emitter to preview the effect.";
            if (const ParticleEffectEmitter* emitter = firstInvalidEmitter())
                return "Emitter failed compilation.\n" +
                       compact(emitter->name.empty() ? emitter->stableId : emitter->name, 44);
            if (playbackPaused)
                return "Simulation paused.";

            const LiveEffectStats stats = liveEffectStats(world);
            if (stats.liveEmitters == 0)
                return "No live emitter instance.\nRestart or reload the preview.";
            if (stats.liveParticles == 0)
                return "Particle count is zero.\nRaise spawn rate, add a burst, or restart.";
            return "";
        }

        UiColor previewStateColor(ECSWorld& world) const
        {
            if (!hasAsset)
                return ToolTheme::mutedText;
            if (assetFileMissing() || firstInvalidEmitter() != nullptr)
                return ToolTheme::error;
            if (!hasEnabledEmitter())
                return ToolTheme::warning;

            const LiveEffectStats stats = liveEffectStats(world);
            if (playbackPaused || stats.liveParticles == 0 || stats.liveEmitters == 0)
                return ToolTheme::statusText;
            return ToolTheme::mutedText;
        }

        void updatePreviewControls(UiSystem& ui, ECSWorld& world)
        {
            for (size_t i = 0; i < previewOverlayLines.size(); ++i)
                setText(ui, previewOverlayLines[i], previewOverlayLine(i, world));

            for (UiHandle line : previewGridLines)
                setVisibleRecursive(ui, line, previewGridVisible);
            for (UiHandle line : previewOriginLines)
                setVisibleRecursive(ui, line, previewOriginVisible);
            for (UiHandle line : previewScaleLines)
                setVisibleRecursive(ui, line, previewScaleVisible);
            setVisibleRecursive(ui, previewScaleText, previewScaleVisible);

            for (const PreviewButtonBinding& binding : previewButtons)
            {
                const bool visible = binding.action == PreviewAction::TogglePan ||
                                     binding.action == PreviewAction::FrameSelection ||
                                     binding.action == PreviewAction::FrameEffect ||
                                     binding.action == PreviewAction::ResetCamera ||
                                     binding.action == PreviewAction::ToggleGrid ||
                                     binding.action == PreviewAction::ToggleOrigin ||
                                     binding.action == PreviewAction::ToggleBounds;
                setVisibleRecursive(ui, binding.button.rect, visible);
                if (!visible)
                    continue;

                const bool isToggle = previewActionIsToggle(binding.action);
                if (isToggle)
                {
                    updateToggleButton(ui,
                                       binding.button,
                                       previewButtonLabel(binding.action),
                                       previewActionEnabled(binding.action));
                }
                else
                {
                    updateButton(ui, binding.button, previewButtonLabel(binding.action));
                }
            }
        }

        std::string previewOverlayLine(size_t index, ECSWorld& world) const
        {
            const LiveEffectStats stats = liveEffectStats(world);
            switch (index)
            {
            case 0:
                return stats.hasFrameData
                           ? "Particles  " + std::to_string(stats.frameData.simulatedParticleCount) + " sim / " +
                                 std::to_string(stats.frameData.renderedParticleCount) + " render"
                           : "Particles  " + std::to_string(stats.liveParticles) + " alive";
            case 1:
                return "Emitters   " + std::to_string(stats.liveEmitters) + " live / " +
                       std::to_string(currentAsset.emitters.size()) + " authored";
            case 2:
                return "Time       " +
                       (stats.selectedRuntimeFound ? formatSeconds(stats.selectedEmitterAge) : std::string{"--"}) +
                       "  Speed " + formatFloat(playbackPaused ? 0.0f : playbackTimeScale) + "x";
            case 3:
                return stats.selectedRuntimeFound
                           ? "LOD        " + formatPercent(stats.selectedLodSpawnScale) + " spawn / " +
                                 formatPercent(stats.selectedLodRenderScale) + " render"
                           : "LOD        --";
            case 4:
                return previewCompileOverlay(stats);
            }
            return "";
        }

        std::string previewCompileOverlay(const LiveEffectStats& stats) const
        {
            std::string line = "Status     ";
            if (const ParticleEffectEmitter* emitter = firstInvalidEmitter())
                line += "compile failed: " + compact(emitter->name.empty() ? emitter->stableId : emitter->name, 20);
            else
                line += graphStatusLabel();

            if (stats.hasBudget)
            {
                line += "  Budget ";
                line += std::to_string(stats.budget.activeParticles);
                line += "/";
                line += std::to_string(stats.budget.requestedSimulatedParticles);
            }
            return compact(line, 64);
        }

        bool previewActionIsToggle(PreviewAction action) const
        {
            switch (action)
            {
            case PreviewAction::TogglePan:
            case PreviewAction::ToggleGrid:
            case PreviewAction::ToggleOrigin:
            case PreviewAction::ToggleScale:
            case PreviewAction::ToggleBounds:
            case PreviewAction::ToggleGizmos:
            case PreviewAction::ToggleCollision:
            case PreviewAction::ToggleCulling:
                return true;
            case PreviewAction::FrameSelection:
            case PreviewAction::FrameEffect:
            case PreviewAction::ResetCamera:
                return false;
            }
            return false;
        }

        bool previewActionEnabled(PreviewAction action) const
        {
            switch (action)
            {
            case PreviewAction::TogglePan:
                return previewPanMode;
            case PreviewAction::ToggleGrid:
                return previewGridVisible;
            case PreviewAction::ToggleOrigin:
                return previewOriginVisible;
            case PreviewAction::ToggleScale:
                return previewScaleVisible;
            case PreviewAction::ToggleBounds:
                return previewBoundsVisible;
            case PreviewAction::ToggleGizmos:
                return previewGizmosVisible;
            case PreviewAction::ToggleCollision:
                return previewCollisionVisible;
            case PreviewAction::ToggleCulling:
                return previewCullingVisible;
            case PreviewAction::FrameSelection:
            case PreviewAction::FrameEffect:
            case PreviewAction::ResetCamera:
                return false;
            }
            return false;
        }

        static std::string previewButtonLabel(PreviewAction action)
        {
            switch (action)
            {
            case PreviewAction::TogglePan:
                return "Pan";
            case PreviewAction::FrameSelection:
                return "Frame Sel";
            case PreviewAction::FrameEffect:
                return "Frame All";
            case PreviewAction::ResetCamera:
                return "Reset";
            case PreviewAction::ToggleGrid:
                return "Grid";
            case PreviewAction::ToggleOrigin:
                return "Origin";
            case PreviewAction::ToggleScale:
                return "Scale";
            case PreviewAction::ToggleBounds:
                return "Bounds";
            case PreviewAction::ToggleGizmos:
                return "Gizmos";
            case PreviewAction::ToggleCollision:
                return "Coll";
            case PreviewAction::ToggleCulling:
                return "Cull";
            }
            return "";
        }

        bool assetFileMissing() const
        {
            if (!hasAsset || currentPath.empty())
                return false;
            std::error_code ec;
            return !std::filesystem::exists(currentPath, ec);
        }

        bool hasEnabledEmitter() const
        {
            return std::any_of(currentAsset.emitters.begin(),
                               currentAsset.emitters.end(),
                               [](const ParticleEffectEmitter& emitter)
                               {
                                   return emitter.descriptor.enabled;
                               });
        }

        const ParticleEffectEmitter* firstInvalidEmitter() const
        {
            const auto it = std::find_if(currentAsset.emitters.begin(),
                                         currentAsset.emitters.end(),
                                         [](const ParticleEffectEmitter& emitter)
                                         {
                                             return emitter.descriptor.enabled && !emitter.compiledProgram.valid;
                                         });
            return it == currentAsset.emitters.end() ? nullptr : &*it;
        }

        void updateNotification(EngineToolContext& ctx, const std::string& status)
        {
            if (!status.empty() && status != lastStatusText)
            {
                lastStatusText      = status;
                notificationMessage = status;
                notificationSeconds = 3.0f;
            }

            if (notificationSeconds > 0.0f)
            {
                const float dt = ctx.time == nullptr ? 0.0f : std::max(0.0f, ctx.time->unscaledDeltaTime);
                notificationSeconds = std::max(0.0f, notificationSeconds - dt);
            }

            const bool visible = notificationSeconds > 0.0f && !notificationMessage.empty();
            setVisibleRecursive(ctx.ui, notificationText, visible);
            if (!visible)
                return;

            setText(ctx.ui, notificationText, displayStatusText(notificationMessage));
            setTextColor(ctx.ui, notificationText, notificationColor(notificationMessage));
        }

        static std::string displayStatusText(const std::string& message)
        {
            if (message.empty())
                return {};

            const auto prefix = [&](const char* raw, const char* display) -> std::optional<std::string>
            {
                const std::string rawPrefix(raw);
                if (message.rfind(rawPrefix, 0) != 0)
                    return std::nullopt;
                return std::string(display) + message.substr(rawPrefix.size());
            };

            if (std::optional<std::string> opened = prefix("OPENED ", "Opened "))
                return *opened;
            if (std::optional<std::string> saved = prefix("SAVED ", "Saved "))
                return *saved;
            if (std::optional<std::string> selected = prefix("SELECTED ", "Selected "))
                return *selected;
            if (std::optional<std::string> found = prefix("FOUND ", "Found "))
                return *found;
            if (std::optional<std::string> missing = prefix("MISSING ", "Missing "))
                return *missing;

            bool hasLetter = false;
            bool hasLower  = false;
            for (unsigned char c : message)
            {
                if (!std::isalpha(c))
                    continue;
                hasLetter = true;
                if (std::islower(c))
                    hasLower = true;
            }

            if (!hasLetter || hasLower)
                return message;

            std::string display = message;
            bool        word    = true;
            for (char& ch : display)
            {
                const unsigned char c = static_cast<unsigned char>(ch);
                if (std::isalpha(c))
                {
                    ch   = static_cast<char>(word ? std::toupper(c) : std::tolower(c));
                    word = false;
                    continue;
                }
                word = !std::isdigit(c);
            }
            return display;
        }

        static UiColor notificationColor(const std::string& message)
        {
            if (containsInsensitive(message, "FAIL") || containsInsensitive(message, "ERROR") ||
                containsInsensitive(message, "INVALID"))
            {
                return ToolTheme::error;
            }
            if (containsInsensitive(message, "WARN") || containsInsensitive(message, "BUDGET"))
                return ToolTheme::warning;
            if (containsInsensitive(message, "SAVED") || containsInsensitive(message, "OPENED") ||
                containsInsensitive(message, "RELOAD") || containsInsensitive(message, "UNDO") ||
                containsInsensitive(message, "REDO"))
            {
                return ToolTheme::success;
            }
            return ToolTheme::statusText;
        }

        void syncPreviewWorld(EngineToolContext& ctx)
        {
            if (!hasAsset || currentPath.empty())
                return;

            previewWorld.ensure(ctx.resources);
            previewWorld.syncAsset(currentPath, currentAsset, playbackTimeScale, playbackPaused);
        }

        void clearPreviewRender(ECSWorld& world)
        {
            if (world.hasAny<EditorPreviewRenderComponent>())
                world.getSingleton<EditorPreviewRenderComponent>().data.enabled = false;
        }

        void updatePreviewTextureImage(UiSystem& ui, texture_id_type textureID)
        {
            if (previewTextureImage == UI_INVALID_HANDLE)
                return;

            UiImageData image;
            image.textureID = textureID;
            image.tint = {1.0f, 1.0f, 1.0f, textureID == 0 ? 0.0f : 1.0f};
            ui.setPayload(previewTextureImage, image);
            setVisibleRecursive(ui, previewTextureImage, textureID != 0);
        }

        void publishPreviewRender(EngineToolContext& ctx)
        {
            if (!hasAsset || currentPath.empty())
            {
                clearPreviewRender(ctx.world);
                updatePreviewTextureImage(ctx.ui, 0);
                return;
            }

            const UiNode* node = ctx.ui.findNode(previewSwatch);
            if (node == nullptr)
                return;

            const UiRect& bounds = node->computedLayout.bounds;
            const uint32_t width = std::max(
                1u,
                static_cast<uint32_t>(std::round(bounds.width * std::max(1.0f, ctx.windowPixelWidth))));
            const uint32_t height = std::max(
                1u,
                static_cast<uint32_t>(std::round(bounds.height * std::max(1.0f, ctx.windowPixelHeight))));

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data =
                previewWorld.buildFrame(currentAsset, dt, !playbackPaused, playbackTimeScale, width, height);
            data.colorTextureID = previousTexture;

            EditorPreviewRenderComponent* component = nullptr;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                component = &ctx.world.getSingleton<EditorPreviewRenderComponent>();
            else
                component = &ctx.world.createSingleton<EditorPreviewRenderComponent>();
            component->data = std::move(data);
            updatePreviewTextureImage(ctx.ui, component->data.colorTextureID);
        }

        void updateInspectorSections(UiSystem& ui)
        {
            for (const InspectorSectionBinding& binding : inspectorSections)
            {
                updateSectionHeader(ui,
                                    binding.header,
                                    inspectorSectionLabel(binding.section),
                                    inspectorSectionSummary(binding.section),
                                    selectedInspectorSection == binding.section);
            }
        }

        void updateInspectorStatusRows(UiSystem& ui, ECSWorld& world)
        {
            for (size_t i = 0; i < inspectorStatusRows.size(); ++i)
                updateStatusRow(ui, inspectorStatusRows[i], inspectorStatusValue(i, world));
        }

        void updateWorkspace(UiSystem& ui, ECSWorld& world)
        {
            for (const WorkspaceTabBinding& binding : workspaceTabs)
            {
                updateButton(ui, binding.button, workspaceTabLabel(binding.tab));
                if (binding.tab == selectedWorkspaceTab)
                {
                    setRectColor(ui, binding.button.rect, ToolTheme::buttonActive);
                    setTextColor(ui, binding.button.label, ToolTheme::text);
                }
                else
                {
                    setRectColor(ui, binding.button.rect, ToolTheme::panelInset);
                    setTextColor(ui, binding.button.label, ToolTheme::mutedText);
                }
            }

            setText(ui, workspaceHeadline, workspaceHeadlineText());
            setTextColor(ui, workspaceHeadline, ToolTheme::text);
            for (size_t i = 0; i < workspaceInfoLines.size(); ++i)
                setText(ui, workspaceInfoLines[i], workspaceLineText(i, world));

            const bool graphVisible = selectedWorkspaceTab == WorkspaceTab::Graph;
            for (const GraphButtonBinding& binding : graphButtons)
                setVisibleRecursive(ui, binding.button.rect, graphVisible);
        }

        void syncInspectorSectionVisibility(UiSystem& ui)
        {
            const bool moduleSelection = selectedObject.kind == FxSelectionKind::Module ||
                                         selectedObject.kind == FxSelectionKind::Curve ||
                                         selectedObject.kind == FxSelectionKind::Burst;
            const bool emitterSelection = selectedObject.kind == FxSelectionKind::Emitter;
            const bool effectSelection = selectedObject.kind == FxSelectionKind::Effect;
            const bool showParameters = moduleSelection;
            const bool showStatus = !moduleSelection;

            for (const InspectorSectionBinding& binding : inspectorSections)
            {
                bool visible = false;
                if (effectSelection)
                    visible = binding.section == InspectorSection::General;
                else if (emitterSelection)
                    visible = binding.section == InspectorSection::General ||
                              binding.section == InspectorSection::Runtime;
                else if (moduleSelection)
                    visible = binding.section == InspectorSection::Parameters;
                setVisibleRecursive(ui, binding.header.rect, visible);
            }

            for (const ToolStatusRow& row : inspectorStatusRows)
            {
                setVisibleRecursive(ui, row.label, showStatus);
                setVisibleRecursive(ui, row.value, showStatus);
            }
            for (const ToolButton& row : moduleRows)
                setVisibleRecursive(ui, row.rect, false);
            for (const ModuleButtonBinding& binding : moduleButtons)
                setVisibleRecursive(ui, binding.button.rect, false);
            if (!showParameters)
            {
                for (const ParameterControl& control : parameterControls)
                    setParameterControlVisible(ui, control, false, false, false, false, false, false, false);
            }
        }

        std::string inspectorObjectTitle() const
        {
            if (!hasAsset)
                return "No Effect Selected";
            const ParticleEffectEmitter*  emitter = selectedEmitter();
            const ParticleModuleInstance* module  = selectedModule();
            switch (selectedObject.kind)
            {
            case FxSelectionKind::Effect:
                return "Effect  " +
                       compact(currentAsset.metadata.name.empty() ? fileName(currentPath) : currentAsset.metadata.name,
                               34);
            case FxSelectionKind::Emitter:
                return emitter == nullptr ? "Emitter" : "Emitter  " + displayEmitterName(*emitter);
            case FxSelectionKind::Module:
                return module == nullptr ? "Module" : "Module  " + moduleDisplayName(*module);
            case FxSelectionKind::Curve:
                return "Curve  " + compact(selectedRichParameterId.empty() ? "Parameter" : selectedRichParameterId, 34);
            case FxSelectionKind::Burst:
                return "Burst  " + compact(selectedRichParameterId.empty() ? "Timeline" : selectedRichParameterId, 34);
            case FxSelectionKind::GraphNode:
                return "Graph Node";
            }
            return "Selection";
        }

        std::string inspectorSectionSummary(InspectorSection section) const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            switch (section)
            {
            case InspectorSection::General:
                return dirty ? "Dirty" : "Clean";
            case InspectorSection::Modules:
                return emitter == nullptr ? "0" : std::to_string(emitter->modules.size());
            case InspectorSection::Parameters:
            {
                const ParticleModuleInstance*   module = selectedModule();
                const ParticleModuleDefinition* definition =
                    module == nullptr ? nullptr : findParticleModuleDefinition(module->typeId);
                return definition == nullptr ? "0" : std::to_string(editableParameters(*definition).size());
            }
            case InspectorSection::Runtime:
                return graphStatusLabel();
            }
            return "";
        }

        std::string inspectorStatusLabel(size_t index) const
        {
            static const char* effectLabels[] = {"Name", "Asset", "Emitters", "Modules", "State", "Preview", "Next"};
            static const char* emitterLabels[] = {"Name", "Enabled", "Modules", "Runtime", "Budget", "LOD", "Next"};
            static const char* labels[]   = {"Effect", "Emitter", "Module", "Graph", "Runtime", "Budget", "LOD"};
            if (selectedObject.kind == FxSelectionKind::Effect)
                return index < 7 ? effectLabels[index] : "";
            if (selectedObject.kind == FxSelectionKind::Emitter)
                return index < 7 ? emitterLabels[index] : "";
            constexpr size_t   labelCount = sizeof(labels) / sizeof(labels[0]);
            return index < labelCount ? labels[index] : "";
        }

        std::string inspectorStatusValue(size_t index, ECSWorld& world) const
        {
            const LiveEffectStats         stats   = liveEffectStats(world);
            const ParticleEffectEmitter*  emitter = selectedEmitter();
            const ParticleModuleInstance* module  = selectedModule();
            if (selectedObject.kind == FxSelectionKind::Effect)
            {
                switch (index)
                {
                case 0:
                    return hasAsset ? compact(currentAsset.metadata.name.empty() ? fileName(currentPath)
                                                                                 : currentAsset.metadata.name,
                                              28)
                                    : "--";
                case 1:
                    return currentPath.empty() ? "--" : compact(fileName(currentPath), 30);
                case 2:
                    return std::to_string(currentAsset.emitters.size());
                case 3:
                {
                    size_t moduleCount = 0;
                    for (const ParticleEffectEmitter& item : currentAsset.emitters)
                        moduleCount += item.modules.size();
                    return std::to_string(moduleCount);
                }
                case 4:
                    return dirty ? "Unsaved changes" : "Saved";
                case 5:
                    return playbackPaused ? "Paused" : "Playing";
                case 6:
                    return "Select an emitter in the hierarchy.";
                }
                return "";
            }

            if (selectedObject.kind == FxSelectionKind::Emitter)
            {
                switch (index)
                {
                case 0:
                    return emitter == nullptr ? "--" : displayEmitterName(*emitter);
                case 1:
                    return emitter == nullptr ? "--" : formatBool(emitter->descriptor.enabled);
                case 2:
                    return emitter == nullptr ? "--" : std::to_string(emitter->modules.size());
                case 3:
                    return std::to_string(stats.liveEmitters) + " emitters / " +
                           std::to_string(stats.liveParticles) + " alive";
                case 4:
                    return stats.hasBudget ? std::to_string(stats.budget.activeParticles) + "/" +
                                                 std::to_string(stats.budget.requestedSimulatedParticles) + " simulated"
                                           : "No budget frame";
                case 5:
                    return stats.selectedRuntimeFound ? formatPercent(stats.selectedLodSpawnScale) + " spawn / " +
                                                            formatPercent(stats.selectedLodRenderScale) + " render"
                                                      : "No selected runtime";
                case 6:
                    return "Select a module to edit parameters.";
                }
                return "";
            }

            switch (index)
            {
            case 0:
                return hasAsset ? compact(currentAsset.metadata.name.empty() ? fileName(currentPath)
                                                                             : currentAsset.metadata.name,
                                          28)
                                : "--";
            case 1:
                return emitter == nullptr ? "--" : compact(emitterRowLabel(*emitter), 28);
            case 2:
                return module == nullptr ? "--" : compact(moduleDisplayName(*module), 28);
            case 3:
                return graphStatusLabel();
            case 4:
                return std::to_string(stats.liveEmitters) + " emitters / " + std::to_string(stats.liveParticles) +
                       " alive";
            case 5:
                return stats.hasBudget ? std::to_string(stats.budget.activeParticles) + "/" +
                                             std::to_string(stats.budget.requestedSimulatedParticles) + " simulated"
                                       : "No budget frame";
            case 6:
                return stats.selectedRuntimeFound ? formatPercent(stats.selectedLodSpawnScale) + " spawn / " +
                                                        formatPercent(stats.selectedLodRenderScale) + " render"
                                                  : "No selected runtime";
            }
            return "";
        }

        std::string selectionContextLabel() const
        {
            if (!hasAsset)
                return "No Selection";

            switch (selectedObject.kind)
            {
            case FxSelectionKind::Effect:
                return "Effect";
            case FxSelectionKind::Emitter:
                return "Emitter";
            case FxSelectionKind::Module:
                return "Module";
            case FxSelectionKind::Curve:
                return "Curve";
            case FxSelectionKind::Burst:
                return "Burst";
            case FxSelectionKind::GraphNode:
                return "Graph Node";
            }
            return "Selection";
        }

        std::string previewCameraLabel() const
        {
            return std::string("Camera  ") + (previewPanMode ? "Pan" : "Orbit") + "  Perspective";
        }

        static std::string indentForDepth(int depth)
        {
            switch (std::max(0, depth))
            {
            case 0:
                return "";
            case 1:
                return "  ";
            case 2:
                return "    ";
            default:
                return "      ";
            }
        }

        std::string outlinerRowLabel(const OutlinerItem& item) const
        {
            std::string label = indentForDepth(item.depth) + item.label;
            if (!item.detail.empty())
                label += "  " + item.detail;
            return compact(label, 42);
        }

        std::string outlinerEmptyStateLabel() const
        {
            if (!hasAsset)
                return "No effect loaded. Open an asset above.";
            if (currentAsset.emitters.empty())
                return "No emitters. Use Add to create one.";
            if (!emitterSearchDraft.empty())
                return "No matching emitters.";
            return "Select an emitter or module.";
        }

        static std::string displayEmitterName(const ParticleEffectEmitter& emitter)
        {
            if (!emitter.name.empty())
                return compact(emitter.name, 30);
            if (!emitter.stableId.empty())
                return compact(emitter.stableId, 30);
            return "Emitter";
        }

        static std::string moduleOutlinerLabel(const ParticleModuleInstance& module)
        {
            if (!module.displayName.empty())
                return compact(module.displayName, 26);
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            return definition == nullptr ? "Unknown Module" : compact(definition->displayName, 26);
        }

        static std::string moduleStageShortLabel(const ParticleModuleInstance& module)
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            if (definition == nullptr)
                return "--";
            return std::string(executionStageShortLabel(definition->executionStage));
        }

        std::string workspaceHeadlineText() const
        {
            switch (selectedWorkspaceTab)
            {
            case WorkspaceTab::Curves:
                return "Curves";
            case WorkspaceTab::Timeline:
                return "Timeline";
            case WorkspaceTab::Graph:
                return "Graph Compatibility";
            case WorkspaceTab::Diagnostics:
                return "Diagnostics";
            case WorkspaceTab::Preview:
                return "Preview Settings";
            case WorkspaceTab::Output:
                return "Compiled Output";
            }
            return "";
        }

        std::string workspaceLineText(size_t index, ECSWorld& world) const
        {
            switch (selectedWorkspaceTab)
            {
            case WorkspaceTab::Curves:
                return curveWorkspaceLine(index);
            case WorkspaceTab::Timeline:
                return timelineWorkspaceLine(index);
            case WorkspaceTab::Graph:
                return graphWorkspaceLine(index);
            case WorkspaceTab::Diagnostics:
                return diagnosticsWorkspaceLine(index, world);
            case WorkspaceTab::Preview:
                return previewWorkspaceLine(index);
            case WorkspaceTab::Output:
                return outputWorkspaceLine(index);
            }
            return "";
        }

        std::string curveWorkspaceLine(size_t index) const
        {
            const ParticleEffectEmitter*  emitter = selectedEmitter();
            const ParticleModuleInstance* module  = selectedModule();
            if (emitter == nullptr)
                return index == 0 ? "No emitter selected" : "";
            switch (index)
            {
            case 0:
                return module == nullptr ? "Selected module: --" : "Selected module: " + moduleDisplayName(*module);
            case 1:
                return "Color keys: " + std::to_string(emitter->descriptor.colorOverLifetime.size()) +
                       "  Alpha keys: " + std::to_string(emitter->descriptor.alphaOverLifetime.size());
            case 2:
                return "Size keys: " + std::to_string(emitter->descriptor.sizeOverLifetime.size()) +
                       "  Bursts: " + std::to_string(emitter->descriptor.bursts.size());
            case 3:
                return richSelectionSummary();
            case 4:
                return "Selected field: " +
                       (selectedRichParameterId.empty() ? std::string{"--"} : selectedRichParameterId);
            case 5:
                return "Asset curves: live preview synchronized";
            }
            return "";
        }

        std::string timelineWorkspaceLine(size_t index) const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return index == 0 ? "No emitter selected" : "";

            const ParticleEmitterComponent& descriptor = emitter->descriptor;
            switch (index)
            {
            case 0:
                return "Emitter: " + compact(emitter->name, 36);
            case 1:
                return "Duration: " + formatSeconds(descriptor.duration) +
                       "  Delay: " + formatSeconds(descriptor.startDelay) + "  Loop: " + formatBool(descriptor.looping);
            case 2:
                return "Spawn rate: " + formatFloat(descriptor.emissionRate) +
                       "/s  Lifetime: " + formatSeconds(descriptor.lifetimeMin) + ".." +
                       formatSeconds(descriptor.lifetimeMax);
            case 3:
                return "Bursts: " + burstSummary(descriptor.bursts);
            case 4:
                return "Max particles: " + std::to_string(descriptor.maxParticles) +
                       "  Intensity: " + formatFloat(descriptor.intensity);
            case 5:
                return "Playback cursor: live runtime preview";
            }
            return "";
        }

        std::string graphWorkspaceLine(size_t index) const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return index == 0 ? "No emitter selected" : "";

            const ParticleEffectGraph& graph = emitter->graph;
            switch (index)
            {
            case 0:
                return "Nodes: " + std::to_string(graph.nodes.size()) +
                       "  Links: " + std::to_string(graph.links.size()) +
                       "  Frames: " + std::to_string(graph.frames.size());
            case 1:
                return "Comments: " + std::to_string(graph.comments.size()) +
                       "  Modules: " + std::to_string(emitter->modules.size());
            case 2:
                return selectedGraphNodeSummary(*emitter);
            case 3:
                return graphStatusLabel();
            case 4:
                return firstGraphDiagnosticLine(*emitter);
            case 5:
                return "Search target: " + graphSearchTargetLabel();
            }
            return "";
        }

        std::string diagnosticsWorkspaceLine(size_t index, ECSWorld& world) const
        {
            const LiveEffectStats stats = liveEffectStats(world);
            switch (index)
            {
            case 0:
                return "Live emitters: " + std::to_string(stats.liveEmitters) +
                       "  Alive particles: " + std::to_string(stats.liveParticles);
            case 1:
                return stats.hasFrameData
                           ? "Frame emitters: " + std::to_string(stats.frameData.visibleEmitterCount) + " visible / " +
                                 std::to_string(stats.frameData.culledEmitterCount) + " culled"
                           : "Frame emitters: --";
            case 2:
                return stats.hasFrameData
                           ? "Particles: " + std::to_string(stats.frameData.simulatedParticleCount) + " simulated / " +
                                 std::to_string(stats.frameData.renderedParticleCount) + " rendered"
                           : "Particles: --";
            case 3:
                return stats.hasFrameData
                           ? "Budget clipped: " + std::to_string(stats.frameData.budgetClippedParticleCount) +
                                 " particles  Render budget: " + std::to_string(stats.frameData.renderBudget)
                           : "Budget clipped: --";
            case 4:
                return "Events: " + std::to_string(stats.collisionEventsThisFrame) + " collisions / " +
                       std::to_string(stats.diedThisFrame) + " deaths / " + std::to_string(stats.eventSpawnsThisFrame) +
                       " spawned";
            case 5:
                return stats.selectedRuntimeFound
                           ? "Selected runtime: " + std::string(stats.selectedRuntimeVisible ? "visible" : "culled") +
                                 "  " + cullReasonLabel(stats.selectedCullReason) + "  distance " +
                                 formatFloat(stats.selectedDistanceToCamera)
                           : "Selected runtime: not active";
            }
            return "";
        }

        std::string previewWorkspaceLine(size_t index) const
        {
            switch (index)
            {
            case 0:
                return "Background preset: " + std::to_string(backgroundPreset + 1u);
            case 1:
                return "Camera position: " + formatVec3(currentAsset.preview.cameraPosition);
            case 2:
                return "Camera target: " + formatVec3(currentAsset.preview.cameraTarget);
            case 3:
                return "Orbit distance: " + formatFloat(currentAsset.preview.orbitDistance) +
                       "  Orbit yaw: " + formatFloat(previewOrbitYawDegrees());
            case 4:
                return "Time scale: " + formatFloat(playbackTimeScale) +
                       "  Playback: " + std::string(playbackPaused ? "paused" : "playing");
            case 5:
                return "Preview asset state: " + std::string(dirty ? "modified" : "saved");
            }
            return "";
        }

        static std::string inspectorSectionLabel(InspectorSection section)
        {
            switch (section)
            {
            case InspectorSection::General:
                return "General";
            case InspectorSection::Modules:
                return "Modules";
            case InspectorSection::Parameters:
                return "Parameters";
            case InspectorSection::Runtime:
                return "Runtime";
            }
            return "";
        }

        static std::string workspaceTabLabel(WorkspaceTab tab)
        {
            switch (tab)
            {
            case WorkspaceTab::Curves:
                return "Curves";
            case WorkspaceTab::Timeline:
                return "Timeline";
            case WorkspaceTab::Graph:
                return "Graph";
            case WorkspaceTab::Diagnostics:
                return "Diagnostics";
            case WorkspaceTab::Preview:
                return "Preview";
            case WorkspaceTab::Output:
                return "Output";
            }
            return "";
        }

        std::string richSelectionSummary() const
        {
            const ParticleModuleInstance* module = selectedModule();
            if (module == nullptr || selectedRichParameterId.empty())
                return "Rich parameter: --";

            const ParticleModuleParameter* parameter = gts::particles::findParameter(*module, selectedRichParameterId);
            if (parameter == nullptr)
                return "Rich parameter: --";

            size_t keyCount = 0;
            if (parameter->type == ParticleModuleParameterType::FloatCurve)
                keyCount = parameter->floatCurveValue.size();
            else if (parameter->type == ParticleModuleParameterType::ColorGradient)
                keyCount = parameter->colorGradientValue.size();
            else if (parameter->type == ParticleModuleParameterType::BurstTimeline)
                keyCount = parameter->burstTimelineValue.size();

            return "Rich parameter: " + selectedRichParameterId +
                   "  Key: " + std::to_string(keyCount == 0 ? 0 : selectedRichKeyIndex + 1) + "/" +
                   std::to_string(keyCount);
        }

        static std::string burstSummary(const std::vector<ParticleBurst>& bursts)
        {
            if (bursts.empty())
                return "0";

            const ParticleBurst& first = bursts.front();
            return std::to_string(bursts.size()) + "  First @" + formatSeconds(first.time) + "  " +
                   std::to_string(first.countMin) + ".." + std::to_string(first.countMax);
        }

        std::string selectedGraphNodeSummary(const ParticleEffectEmitter& emitter) const
        {
            const ParticleModuleInstance* module = selectedModule();
            if (module == nullptr)
                return "Selected node: --";

            const ParticleGraphNode* node =
                gts::particles::findParticleGraphNodeForModule(emitter.graph, module->stableId);
            if (node == nullptr)
                return "Selected node: missing";

            return "Selected node: " + compact(node->displayName.empty() ? node->id : node->displayName, 42) +
                   "  Stage: " + moduleStageLabel(*module);
        }

        std::string firstGraphDiagnosticLine(const ParticleEffectEmitter& emitter) const
        {
            std::vector<gts::particles::ParticleModuleGraphDiagnostic> diagnostics;
            gts::particles::validateParticleEffectGraph(emitter, &diagnostics);
            if (diagnostics.empty())
                return "Validation: no warnings";

            const auto&       diagnostic = diagnostics.front();
            const std::string severity =
                diagnostic.severity == gts::particles::ParticleModuleGraphDiagnosticSeverity::Error ? "Error"
                                                                                                    : "Warning";
            return severity + ": " + compact(diagnostic.message, 76);
        }

        std::string graphSearchTargetLabel() const
        {
            const std::vector<ParticleModuleDefinition>& definitions = gts::particles::particleModuleDefinitions();
            if (definitions.empty())
                return "--";
            return compact(definitions[graphSearchIndex % definitions.size()].displayName, 36);
        }

        static std::string moduleStageLabel(const ParticleModuleInstance& module)
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            return definition == nullptr ? std::string{"Unknown"}
                                         : std::string(executionStageLabel(definition->executionStage));
        }

        static std::string cullReasonLabel(ParticleCullReason reason)
        {
            switch (reason)
            {
            case ParticleCullReason::None:
                return "none";
            case ParticleCullReason::Frustum:
                return "frustum";
            case ParticleCullReason::Distance:
                return "distance";
            case ParticleCullReason::Budget:
                return "budget";
            }
            return "unknown";
        }

        static std::string formatBool(bool value)
        {
            return value ? "on" : "off";
        }

        static std::string formatSeconds(float value)
        {
            return formatFloat(value) + "s";
        }

        static std::string formatPercent(float value)
        {
            return formatFloat(value * 100.0f) + "%";
        }

        static std::string formatFloat(float value)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(std::abs(value) >= 10.0f ? 1 : 2) << value;
            return out.str();
        }

        static std::string formatVec3(const glm::vec3& value)
        {
            return formatFloat(value.x) + ", " + formatFloat(value.y) + ", " + formatFloat(value.z);
        }

        std::string outputWorkspaceLine(size_t index) const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return index == 0 ? "No emitter selected" : "";

            const auto& program = emitter->compiledProgram;
            switch (index)
            {
            case 0:
                return std::string("Compilation: ") + (program.valid ? "valid" : "invalid") +
                       "  Backend: CPU descriptor";
            case 1:
                return "Modules: " + std::to_string(program.modules.size()) +
                       "  Static params: " + std::to_string(program.staticParametersEvaluated);
            case 2:
                return "Curves baked: " + std::to_string(program.curvesBaked) +
                       "  Modules fused: " + std::to_string(program.modulesFused);
            case 3:
                return "Dead nodes eliminated: " + std::to_string(program.deadNodesEliminated);
            case 4:
                return "Diagnostics: " + std::to_string(program.diagnostics.size()) + "  " + graphStatusLabel();
            case 5:
                return "Runtime descriptor: " + std::to_string(program.runtimeDescriptor.maxParticles) +
                       " max particles";
            }
            return "";
        }

        std::string emitterRowLabel(const ParticleEffectEmitter& emitter) const
        {
            std::string label = emitter.name.empty() ? emitter.stableId : emitter.name;
            label             = std::string(isEmitterSelected(emitter) ? "> " : "  ") +
                    (emitter.descriptor.enabled ? "[x] " : "[ ] ") + label;
            return compact(label, 30);
        }

        std::string emitterEmptyStateLabel() const
        {
            if (!hasAsset)
                return "No effect loaded";
            if (!emitterSearchDraft.empty())
                return "No emitters match search";
            if (currentAsset.emitters.empty())
                return "No emitters";
            return "";
        }

        std::string moduleDisplayName(const ParticleModuleInstance& module) const
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            std::string label = module.displayName.empty()
                                    ? (definition == nullptr ? module.typeId : definition->displayName)
                                    : module.displayName;
            if (definition != nullptr && !definition->category.empty())
                label = std::string(executionStageShortLabel(definition->executionStage)) + " " + definition->category +
                        " " + label;
            if (!module.enabled)
                label = "Off " + label;
            return compact(label, 24);
        }

        std::string selectedModuleStatus(const ParticleModuleInstance& module) const
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            if (definition == nullptr)
                return "Module unknown";

            std::string label = "Module ";
            label += executionStageLabel(definition->executionStage);
            label += " ";
            label += gts::particles::executionCategoryLabel(definition->executionCategory);
            label += " ";
            label += module.displayName.empty() ? definition->displayName : module.displayName;
            return compact(label, 48);
        }

        bool pointerOverModuleRows(const UiInteractionResult& interaction) const
        {
            for (const ToolButton& row : moduleRows)
            {
                if (interaction.hovered == row.rect || interaction.pressed == row.rect)
                    return true;
            }
            return false;
        }

        bool pointerOverPreview(const UiInteractionResult& interaction) const
        {
            if (interaction.hovered == previewSwatch || interaction.pressed == previewSwatch ||
                interaction.hovered == previewOverlay || interaction.pressed == previewOverlay)
            {
                return true;
            }

            for (const UiHandle line : previewGridLines)
            {
                if (interaction.hovered == line || interaction.pressed == line)
                    return true;
            }
            for (const UiHandle line : previewOriginLines)
            {
                if (interaction.hovered == line || interaction.pressed == line)
                    return true;
            }
            for (const UiHandle line : previewScaleLines)
            {
                if (interaction.hovered == line || interaction.pressed == line)
                    return true;
            }
            return false;
        }

        bool pointerOverParameterControls(const UiInteractionResult& interaction) const
        {
            for (const ParameterControl& control : parameterControls)
            {
                if (interaction.hovered == control.button.rect || interaction.pressed == control.button.rect ||
                    interaction.hovered == control.selector.rect || interaction.pressed == control.selector.rect ||
                    interaction.hovered == control.reset.rect || interaction.pressed == control.reset.rect ||
                    interaction.hovered == control.decrement.rect || interaction.pressed == control.decrement.rect ||
                    interaction.hovered == control.increment.rect || interaction.pressed == control.increment.rect ||
                    interaction.hovered == control.slider.track || interaction.pressed == control.slider.track)
                {
                    return true;
                }
            }
            return false;
        }

        void clampModuleSelection(ParticleEffectEmitter& emitter)
        {
            gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
            gts::particles::syncParticleGraphWithModules(emitter);
            if (emitter.modules.empty())
            {
                selectedModuleIndex    = 0;
                moduleBrowserOffset    = 0;
                parameterControlOffset = 0;
                return;
            }

            selectedModuleIndex = std::min(selectedModuleIndex, emitter.modules.size() - 1);
            moduleBrowserOffset = std::min(moduleBrowserOffset, maxModuleBrowserOffset(emitter));
            if (selectedModuleIndex < moduleBrowserOffset)
                moduleBrowserOffset = selectedModuleIndex;
            else if (selectedModuleIndex >= moduleBrowserOffset + ModuleRowCount)
                moduleBrowserOffset = selectedModuleIndex - ModuleRowCount + 1;
        }

        size_t moduleRowOffset(const ParticleEffectEmitter& emitter) const
        {
            if (emitter.modules.size() <= ModuleRowCount || selectedModuleIndex < ModuleRowCount)
                return 0;
            return std::min(selectedModuleIndex - ModuleRowCount + 1, emitter.modules.size() - ModuleRowCount);
        }

        size_t maxModuleBrowserOffset(const ParticleEffectEmitter& emitter) const
        {
            return emitter.modules.size() <= ModuleRowCount ? 0 : emitter.modules.size() - ModuleRowCount;
        }

        std::vector<ParameterView> editableParameters(const ParticleModuleDefinition& definition) const
        {
            std::vector<ParameterView> parameters;
            std::vector<std::string>   consumed;
            parameters.reserve(definition.parameters.size());
            for (const ParticleModuleParameterDefinition& parameter : definition.parameters)
            {
                if (std::find(consumed.begin(), consumed.end(), parameter.id) != consumed.end())
                    continue;

                const ParticleModuleParameterDefinition* partner = findRangePartner(definition, parameter);
                if (partner != nullptr)
                {
                    parameters.push_back({&parameter, partner});
                    consumed.push_back(partner->id);
                    continue;
                }

                parameters.push_back({&parameter, nullptr});
            }
            return parameters;
        }

        void clampParameterControlOffset(const std::vector<ParameterView>& parameters)
        {
            parameterControlOffset = std::min(parameterControlOffset, maxParameterControlOffset(parameters));
        }

        size_t maxParameterControlOffset(const std::vector<ParameterView>& parameters) const
        {
            return parameters.size() <= ParameterControlRowCount ? 0 : parameters.size() - ParameterControlRowCount;
        }

        const ParticleModuleParameterDefinition*
        findRangePartner(const ParticleModuleDefinition&          definition,
                         const ParticleModuleParameterDefinition& parameter) const
        {
            const std::string partnerId = rangePartnerId(parameter.id);
            if (partnerId.empty())
                return nullptr;

            const auto it = std::find_if(definition.parameters.begin(),
                                         definition.parameters.end(),
                                         [&](const ParticleModuleParameterDefinition& candidate)
                                         {
                                             return candidate.id == partnerId && candidate.type == parameter.type &&
                                                    isNumericType(candidate.type);
                                         });
            return it == definition.parameters.end() ? nullptr : &*it;
        }

        static std::string rangePartnerId(const std::string& id)
        {
            if (!endsWith(id, "Min"))
                return {};
            return id.substr(0, id.size() - 3) + "Max";
        }

        static bool endsWith(const std::string& value, const std::string& suffix)
        {
            return value.size() >= suffix.size() &&
                   value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        ParticleModuleParameter* ensureModuleParameter(ParticleModuleInstance&                  module,
                                                       const ParticleModuleParameterDefinition& definition) const
        {
            if (ParticleModuleParameter* parameter = gts::particles::findParameter(module, definition.id))
            {
                parameter->type = definition.type;
                return parameter;
            }

            module.parameters.push_back(gts::particles::makeDefaultParameter(definition));
            return &module.parameters.back();
        }

        static void resetParameterToDefault(ParticleModuleParameter&                 parameter,
                                            const ParticleModuleParameterDefinition& definition)
        {
            parameter = gts::particles::makeDefaultParameter(definition);
        }

        bool applyParameterViewControl(UiSystem&                  ui,
                                       ECSWorld&                  world,
                                       EngineToolStateComponent&  state,
                                       const UiInteractionResult& interaction,
                                       ParticleEffectEmitter&     emitter,
                                       ParticleModuleInstance&    module,
                                       const ParameterView&       view,
                                       ParameterControl&          control)
        {
            if (view.primary == nullptr)
                return false;
            if (view.isRange())
                return applyRangeControl(ui, world, state, interaction, emitter, module, view, control);

            const ParticleModuleParameterDefinition& definition = *view.primary;
            if (definition.type == ParticleModuleParameterType::Float ||
                definition.type == ParticleModuleParameterType::UInt)
            {
                return applyNumericControl(ui, world, state, interaction, emitter, module, definition, control);
            }
            if (definition.type == ParticleModuleParameterType::FloatCurve ||
                definition.type == ParticleModuleParameterType::ColorGradient ||
                definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                return applyRichControl(ui, world, state, interaction, emitter, module, definition, control);
            }
            return applyButtonControl(world, state, interaction, emitter, module, definition, control);
        }

        bool applyNumericControl(UiSystem&                                ui,
                                 ECSWorld&                                world,
                                 EngineToolStateComponent&                state,
                                 const UiInteractionResult&               interaction,
                                 ParticleEffectEmitter&                   emitter,
                                 ParticleModuleInstance&                  module,
                                 const ParticleModuleParameterDefinition& definition,
                                 const ParameterControl&                  control)
        {
            ParticleModuleParameter* parameter = ensureModuleParameter(module, definition);
            bool                     changed   = false;
            if (wasClicked(interaction, control.reset.rect))
            {
                pushUndoSnapshot();
                resetParameterToDefault(*parameter, definition);
                changed = true;
            }
            else if (isPressed(interaction, control.slider.track))
            {
                beginUndoForHandle(control.slider.track);
                setNumericParameterValue(
                    *parameter, definition, valueFromSliderPointer(ui, control.slider, interaction.pointerX));
                changed = true;
            }
            else if (wasClicked(interaction, control.decrement.rect) || wasClicked(interaction, control.increment.rect))
            {
                pushUndoSnapshot();
                const float direction = wasClicked(interaction, control.increment.rect) ? 1.0f : -1.0f;
                setNumericParameterValue(*parameter,
                                         definition,
                                         numericParameterValue(*parameter, definition) +
                                             direction * numericStep(definition));
                changed = true;
            }

            if (!changed)
                return false;

            commitModuleEdit(world, state, emitter, "MODULE UPDATED");
            return true;
        }

        bool applyRangeControl(UiSystem&                  ui,
                               ECSWorld&                  world,
                               EngineToolStateComponent&  state,
                               const UiInteractionResult& interaction,
                               ParticleEffectEmitter&     emitter,
                               ParticleModuleInstance&    module,
                               const ParameterView&       view,
                               const ParameterControl&    control)
        {
            const std::string key = rangeViewKey(view);
            if (selectedRangeParameterId != key)
            {
                selectedRangeParameterId = key;
                selectedRangeMax         = false;
            }

            if (wasClicked(interaction, control.selector.rect))
            {
                selectedRangeMax = !selectedRangeMax;
                state.status     = selectedRangeMax ? "RANGE MAX" : "RANGE MIN";
                return true;
            }

            ParticleModuleParameter*                 minParameter = ensureModuleParameter(module, *view.primary);
            ParticleModuleParameter*                 maxParameter = ensureModuleParameter(module, *view.secondary);
            const ParticleModuleParameterDefinition& activeDefinition =
                selectedRangeMax ? *view.secondary : *view.primary;
            ParticleModuleParameter& activeParameter = selectedRangeMax ? *maxParameter : *minParameter;

            bool changed = false;
            if (wasClicked(interaction, control.reset.rect))
            {
                pushUndoSnapshot();
                resetParameterToDefault(*minParameter, *view.primary);
                resetParameterToDefault(*maxParameter, *view.secondary);
                changed = true;
            }
            else if (isPressed(interaction, control.slider.track))
            {
                beginUndoForHandle(control.slider.track);
                setNumericParameterValue(activeParameter,
                                         activeDefinition,
                                         valueFromSliderPointer(ui, control.slider, interaction.pointerX));
                changed = true;
            }
            else if (wasClicked(interaction, control.decrement.rect) || wasClicked(interaction, control.increment.rect))
            {
                pushUndoSnapshot();
                const float direction = wasClicked(interaction, control.increment.rect) ? 1.0f : -1.0f;
                setNumericParameterValue(activeParameter,
                                         activeDefinition,
                                         numericParameterValue(activeParameter, activeDefinition) +
                                             direction * numericStep(activeDefinition));
                changed = true;
            }

            if (!changed)
                return false;

            normalizeRange(*minParameter, *maxParameter, *view.primary, *view.secondary);
            commitModuleEdit(world, state, emitter, "RANGE UPDATED");
            return true;
        }

        bool applyButtonControl(ECSWorld&                                world,
                                EngineToolStateComponent&                state,
                                const UiInteractionResult&               interaction,
                                ParticleEffectEmitter&                   emitter,
                                ParticleModuleInstance&                  module,
                                const ParticleModuleParameterDefinition& definition,
                                const ParameterControl&                  control)
        {
            const bool buttonClicked    = wasClicked(interaction, control.button.rect);
            const bool selectorClicked  = wasClicked(interaction, control.selector.rect);
            const bool resetClicked     = wasClicked(interaction, control.reset.rect);
            const bool decrementClicked = wasClicked(interaction, control.decrement.rect);
            const bool incrementClicked = wasClicked(interaction, control.increment.rect);
            if (!buttonClicked && !selectorClicked && !resetClicked && !decrementClicked && !incrementClicked)
                return false;

            ParticleModuleParameter* parameter = ensureModuleParameter(module, definition);
            std::string              status    = "MODULE UPDATED";
            if (resetClicked)
            {
                pushUndoSnapshot();
                resetParameterToDefault(*parameter, definition);
                commitModuleEdit(world, state, emitter, "PARAMETER RESET");
                return true;
            }

            if (definition.type == ParticleModuleParameterType::Bool)
            {
                if (!buttonClicked)
                    return false;
                pushUndoSnapshot();
                parameter->boolValue = !parameter->boolValue;
            }
            else if (definition.type == ParticleModuleParameterType::Enum)
            {
                if (!buttonClicked)
                    return false;
                pushUndoSnapshot();
                parameter->uintValue = nextEnumValue(definition, parameter->uintValue);
            }
            else if (definition.type == ParticleModuleParameterType::String)
            {
                if (definition.assetPicker == gts::particles::ParticleModuleAssetPicker::None)
                {
                    if (!buttonClicked)
                        return false;
                    state.status = "NO PICKER";
                    return true;
                }

                pushUndoSnapshot();
                status = "ASSET PICKED";
                if (selectorClicked)
                {
                    parameter->stringValue = parameter->stringValue.empty()
                                                 ? assetPathAtOffset(definition.assetPicker, parameter->stringValue, 1)
                                                 : std::string{};
                }
                else
                {
                    parameter->stringValue =
                        assetPathAtOffset(definition.assetPicker, parameter->stringValue, incrementClicked ? 1 : -1);
                }
            }
            else
            {
                return false;
            }

            commitModuleEdit(world, state, emitter, status);
            return true;
        }

        bool applyRichControl(UiSystem&                                ui,
                              ECSWorld&                                world,
                              EngineToolStateComponent&                state,
                              const UiInteractionResult&               interaction,
                              ParticleEffectEmitter&                   emitter,
                              ParticleModuleInstance&                  module,
                              const ParticleModuleParameterDefinition& definition,
                              const ParameterControl&                  control)
        {
            ParticleModuleParameter* parameter = ensureModuleParameter(module, definition);
            ensureRichParameterValue(*parameter, definition);
            clampRichSelection(*parameter, definition);

            if (wasClicked(interaction, control.selector.rect))
            {
                cycleRichSelection(*parameter, definition);
                state.status = richStatusLabel(definition);
                return true;
            }

            if (wasClicked(interaction, control.reset.rect))
            {
                pushUndoSnapshot();
                resetParameterToDefault(*parameter, definition);
                ensureRichParameterValue(*parameter, definition);
                commitModuleEdit(world, state, emitter, "PARAMETER RESET");
                return true;
            }

            if (wasClicked(interaction, control.decrement.rect))
            {
                pushUndoSnapshot();
                deleteRichKey(*parameter, definition);
                commitModuleEdit(world, state, emitter, "KEY DELETED");
                return true;
            }

            if (wasClicked(interaction, control.increment.rect))
            {
                pushUndoSnapshot();
                addRichKey(*parameter, definition);
                commitModuleEdit(world, state, emitter, "KEY ADDED");
                return true;
            }

            if (!isPressed(interaction, control.slider.track))
                return false;

            beginUndoForHandle(control.slider.track);
            setRichSliderValue(
                *parameter, definition, valueFromSliderPointer(ui, control.slider, interaction.pointerX));
            commitModuleEdit(world, state, emitter, "CURVE UPDATED");
            return true;
        }

        void commitModuleEdit(ECSWorld&                 world,
                              EngineToolStateComponent& state,
                              ParticleEffectEmitter&    emitter,
                              const std::string&        status)
        {
            syncSelectedEmitterDescriptor(emitter);
            markDirty(state, status);
            applySelectedEmitterToLive(world, false);
        }

        void updateParameterViewControl(UiSystem&               ui,
                                        ParticleModuleInstance& module,
                                        const ParameterView&    view,
                                        ParameterControl&       control)
        {
            if (view.primary == nullptr)
            {
                setParameterControlVisible(ui, control, false, false, false, false, false, false, false);
                return;
            }
            if (view.isRange())
            {
                updateRangeControl(ui, module, view, control);
                return;
            }

            const ParticleModuleParameterDefinition& definition = *view.primary;
            ParticleModuleParameter*                 parameter  = ensureModuleParameter(module, definition);
            if (definition.type == ParticleModuleParameterType::Float ||
                definition.type == ParticleModuleParameterType::UInt)
            {
                updateNumericControl(ui, *parameter, definition, control);
            }
            else if (definition.type == ParticleModuleParameterType::FloatCurve ||
                     definition.type == ParticleModuleParameterType::ColorGradient ||
                     definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                updateRichControl(ui, *parameter, definition, control);
            }
            else if (definition.type == ParticleModuleParameterType::String &&
                     definition.assetPicker != gts::particles::ParticleModuleAssetPicker::None)
            {
                setParameterControlVisible(ui, control, false, false, false, true, true, true, false);
                updateButton(ui, control.selector, assetPickerButtonLabel(*parameter, definition));
                updateButton(ui, control.decrement, "<");
                updateButton(ui, control.increment, ">");
            }
            else
            {
                setParameterControlVisible(ui, control, false, false, false, false, false, false, true);
                updateButton(ui, control.button, parameterButtonLabel(*parameter, definition));
            }
        }

        void updateNumericControl(UiSystem&                                ui,
                                  const ParticleModuleParameter&           parameter,
                                  const ParticleModuleParameterDefinition& definition,
                                  ParameterControl&                        control)
        {
            configureSlider(control.slider, definition, false);
            const float              value = numericParameterValue(parameter, definition);
            const EditorPropertySpec spec  = parameterPropertySpec(parameter, definition);
            setParameterControlVisible(ui, control, true, true, true, false, true, true, false);
            setText(ui, control.slider.label, spec.displayName);
            setText(ui, control.slider.value, formatValue(value, control.slider.wholeNumber));
            updateSlider(ui, control.slider, value, parameterSliderColor(definition));
            updateButton(ui, control.reset, "Reset");
            setRectColor(ui, control.reset.rect, spec.modified ? ToolTheme::accentSoft : ToolTheme::panelInset);
            setTextColor(ui, control.reset.label, spec.modified ? ToolTheme::text : ToolTheme::mutedText);
            updateButton(ui, control.decrement, "-");
            updateButton(ui, control.increment, "+");
        }

        void updateRangeControl(UiSystem&               ui,
                                ParticleModuleInstance& module,
                                const ParameterView&    view,
                                ParameterControl&       control)
        {
            ParticleModuleParameter* minParameter = ensureModuleParameter(module, *view.primary);
            ParticleModuleParameter* maxParameter = ensureModuleParameter(module, *view.secondary);
            normalizeRange(*minParameter, *maxParameter, *view.primary, *view.secondary);

            const std::string key = rangeViewKey(view);
            if (selectedRangeParameterId != key)
            {
                selectedRangeParameterId = key;
                selectedRangeMax         = false;
            }

            const ParticleModuleParameterDefinition& activeDefinition =
                selectedRangeMax ? *view.secondary : *view.primary;
            const ParticleModuleParameter& activeParameter = selectedRangeMax ? *maxParameter : *minParameter;
            configureSlider(control.slider, activeDefinition, false);
            setParameterControlVisible(ui, control, true, true, true, true, true, true, false);
            setText(ui, control.slider.label, view.primary->label);
            setText(ui, control.slider.value, formatValue(numericParameterValue(activeParameter, activeDefinition),
                                                          activeDefinition.wholeNumber));
            updateSlider(ui,
                         control.slider,
                         numericParameterValue(activeParameter, activeDefinition),
                         parameterSliderColor(activeDefinition));
            updateButton(
                ui, control.selector, rangeButtonLabel(*minParameter, *maxParameter, *view.primary, *view.secondary));
            updateButton(ui, control.reset, "Reset");
            setRectColor(ui, control.reset.rect, ToolTheme::accentSoft);
            updateButton(ui, control.decrement, "-");
            updateButton(ui, control.increment, "+");
        }

        void updateRichControl(UiSystem&                                ui,
                               ParticleModuleParameter&                 parameter,
                               const ParticleModuleParameterDefinition& definition,
                               ParameterControl&                        control)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);
            configureRichSlider(control.slider, definition);
            setParameterControlVisible(ui, control, true, true, true, true, true, true, false);
            setText(ui, control.slider.label, definition.label);
            setText(ui, control.slider.value, richFieldLabel(definition));
            updateSlider(ui, control.slider, richSliderValue(parameter, definition), richSliderColor(definition));
            updateButton(ui, control.selector, richButtonLabel(parameter, definition));
            updateButton(ui, control.reset, "Reset");
            setRectColor(ui, control.reset.rect, ToolTheme::accentSoft);
            updateButton(ui, control.decrement, "-");
            updateButton(ui, control.increment, "+");
        }

        static void configureSlider(ToolSlider&                              slider,
                                    const ParticleModuleParameterDefinition& definition,
                                    bool                                     wholeNumberOverride)
        {
            slider.minValue = definition.minValue;
            slider.maxValue = definition.maxValue;
            slider.wholeNumber =
                wholeNumberOverride || definition.wholeNumber || definition.type == ParticleModuleParameterType::UInt;
        }

        static void setParameterControlVisible(UiSystem&               ui,
                                               const ParameterControl& control,
                                               bool                    sliderVisible,
                                               bool                    sliderLabelVisible,
                                               bool                    sliderValueVisible,
                                               bool                    selectorVisible,
                                               bool                    resetVisible,
                                               bool                    stepVisible,
                                               bool                    buttonVisible)
        {
            setVisibleRecursive(ui, control.slider.label, sliderVisible && sliderLabelVisible);
            setVisibleRecursive(ui, control.slider.value, sliderVisible && sliderValueVisible);
            setVisibleRecursive(ui, control.slider.track, sliderVisible);
            setVisibleRecursive(ui, control.selector.rect, selectorVisible);
            setVisibleRecursive(ui, control.reset.rect, resetVisible);
            setVisibleRecursive(ui, control.decrement.rect, stepVisible);
            setVisibleRecursive(ui, control.increment.rect, stepVisible);
            setVisibleRecursive(ui, control.button.rect, buttonVisible);
        }

        static bool isNumericType(ParticleModuleParameterType type)
        {
            return type == ParticleModuleParameterType::Float || type == ParticleModuleParameterType::UInt;
        }

        static float numericParameterValue(const ParticleModuleParameter&           parameter,
                                           const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::UInt)
                return static_cast<float>(parameter.uintValue);
            return parameter.floatValue;
        }

        static void setNumericParameterValue(ParticleModuleParameter&                 parameter,
                                             const ParticleModuleParameterDefinition& definition,
                                             float                                    value)
        {
            value = std::clamp(value, definition.minValue, definition.maxValue);
            if (definition.type == ParticleModuleParameterType::UInt)
                parameter.uintValue = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
            else
                parameter.floatValue = value;
        }

        static float numericStep(const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::UInt || definition.wholeNumber)
                return 1.0f;
            return std::max(0.01f, (definition.maxValue - definition.minValue) * 0.025f);
        }

        static void normalizeRange(ParticleModuleParameter&                 minParameter,
                                   ParticleModuleParameter&                 maxParameter,
                                   const ParticleModuleParameterDefinition& minDefinition,
                                   const ParticleModuleParameterDefinition& maxDefinition)
        {
            const float minValue = numericParameterValue(minParameter, minDefinition);
            const float maxValue = numericParameterValue(maxParameter, maxDefinition);
            if (maxValue < minValue)
                setNumericParameterValue(maxParameter, maxDefinition, minValue);
        }

        std::string rangeViewKey(const ParameterView& view) const
        {
            if (view.primary == nullptr || view.secondary == nullptr)
                return {};
            return view.primary->id + ":" + view.secondary->id;
        }

        std::string rangeButtonLabel(const ParticleModuleParameter&           minParameter,
                                     const ParticleModuleParameter&           maxParameter,
                                     const ParticleModuleParameterDefinition& minDefinition,
                                     const ParticleModuleParameterDefinition& maxDefinition) const
        {
            std::string  label = minDefinition.label;
            const size_t pos   = label.rfind(" MIN");
            if (pos != std::string::npos)
                label = label.substr(0, pos);
            label += " " + formatValue(numericParameterValue(minParameter, minDefinition), minDefinition.wholeNumber);
            label += ".." + formatValue(numericParameterValue(maxParameter, maxDefinition), maxDefinition.wholeNumber);
            label += selectedRangeMax ? " MAX" : " MIN";
            return compact(label, 24);
        }

        void ensureRichParameterValue(ParticleModuleParameter&                 parameter,
                                      const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                if (parameter.floatCurveValue.empty())
                    parameter.floatCurveValue =
                        definition.defaultFloatCurve.empty()
                            ? ParticleFloatCurve{{0.0f, definition.minValue}, {1.0f, definition.maxValue}}
                            : definition.defaultFloatCurve;
                normalizeFloatCurve(parameter.floatCurveValue, definition.minValue, definition.maxValue);
            }
            else if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                if (parameter.colorGradientValue.empty())
                    parameter.colorGradientValue =
                        definition.defaultColorGradient.empty()
                            ? ParticleColorCurve{{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}}
                            : definition.defaultColorGradient;
                normalizeColorGradient(parameter.colorGradientValue);
            }
        }

        void clampRichSelection(const ParticleModuleParameter&           parameter,
                                const ParticleModuleParameterDefinition& definition)
        {
            if (selectedRichParameterId != definition.id)
            {
                selectedRichParameterId = definition.id;
                selectedRichKeyIndex    = 0;
                selectedRichField       = 0;
            }

            const size_t keyCount = richKeyCount(parameter, definition);
            if (keyCount == 0)
                selectedRichKeyIndex = 0;
            else
                selectedRichKeyIndex = std::min(selectedRichKeyIndex, keyCount - 1);
            selectedRichField = selectedRichField % richFieldCount(definition);
        }

        void cycleRichSelection(const ParticleModuleParameter&           parameter,
                                const ParticleModuleParameterDefinition& definition)
        {
            clampRichSelection(parameter, definition);
            selectedRichField += 1;
            if (selectedRichField < richFieldCount(definition))
                return;

            selectedRichField     = 0;
            const size_t keyCount = richKeyCount(parameter, definition);
            selectedRichKeyIndex  = keyCount == 0 ? 0 : (selectedRichKeyIndex + 1) % keyCount;
        }

        void configureRichSlider(ToolSlider& slider, const ParticleModuleParameterDefinition& definition) const
        {
            slider.wholeNumber = false;
            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                slider.minValue = selectedRichField == 0 ? 0.0f : definition.minValue;
                slider.maxValue = selectedRichField == 0 ? 1.0f : definition.maxValue;
                return;
            }
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                slider.minValue = 0.0f;
                slider.maxValue = 1.0f;
                return;
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                slider.minValue = 0.0f;
                slider.maxValue = selectedRichField == 4 ? 64.0f : (selectedRichField == 3 ? 12.0f : 512.0f);
                if (selectedRichField == 0)
                    slider.maxValue = 12.0f;
                slider.wholeNumber = selectedRichField == 1 || selectedRichField == 2 || selectedRichField == 4;
            }
        }

        float richSliderValue(const ParticleModuleParameter&           parameter,
                              const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                if (parameter.floatCurveValue.empty())
                    return 0.0f;
                const ParticleFloatKey& key = parameter.floatCurveValue[selectedRichKeyIndex];
                return selectedRichField == 0 ? key.t : key.value;
            }
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                if (parameter.colorGradientValue.empty())
                    return 0.0f;
                const ParticleColorKey& key = parameter.colorGradientValue[selectedRichKeyIndex];
                if (selectedRichField == 0)
                    return key.t;
                return key.color[static_cast<int>(selectedRichField - 1)];
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                if (parameter.burstTimelineValue.empty())
                    return 0.0f;
                const ParticleBurst& burst = parameter.burstTimelineValue[selectedRichKeyIndex];
                switch (selectedRichField)
                {
                case 0:
                    return burst.time;
                case 1:
                    return static_cast<float>(burst.countMin);
                case 2:
                    return static_cast<float>(burst.countMax);
                case 3:
                    return burst.repeatInterval;
                case 4:
                    return static_cast<float>(burst.repeatCount);
                }
            }
            return 0.0f;
        }

        void setRichSliderValue(ParticleModuleParameter&                 parameter,
                                const ParticleModuleParameterDefinition& definition,
                                float                                    value)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);

            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                ParticleFloatKey& key = parameter.floatCurveValue[selectedRichKeyIndex];
                if (selectedRichField == 0)
                    key.t = std::clamp(value, 0.0f, 1.0f);
                else
                    key.value = std::clamp(value, definition.minValue, definition.maxValue);
                normalizeFloatCurve(parameter.floatCurveValue, definition.minValue, definition.maxValue);
            }
            else if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                ParticleColorKey& key = parameter.colorGradientValue[selectedRichKeyIndex];
                if (selectedRichField == 0)
                    key.t = std::clamp(value, 0.0f, 1.0f);
                else
                    key.color[static_cast<int>(selectedRichField - 1)] = std::clamp(value, 0.0f, 1.0f);
                normalizeColorGradient(parameter.colorGradientValue);
            }
            else if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                if (parameter.burstTimelineValue.empty())
                    parameter.burstTimelineValue.push_back({0.0f, 8u, 8u, 0.0f, 0u});
                ParticleBurst& burst = parameter.burstTimelineValue[selectedRichKeyIndex];
                switch (selectedRichField)
                {
                case 0:
                    burst.time = std::clamp(value, 0.0f, 12.0f);
                    break;
                case 1:
                    burst.countMin = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
                    break;
                case 2:
                    burst.countMax = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
                    break;
                case 3:
                    burst.repeatInterval = std::clamp(value, 0.0f, 12.0f);
                    break;
                case 4:
                    burst.repeatCount = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
                    break;
                }
                normalizeBursts(parameter.burstTimelineValue);
            }
        }

        void addRichKey(ParticleModuleParameter& parameter, const ParticleModuleParameterDefinition& definition)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);

            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                const ParticleFloatKey base = parameter.floatCurveValue[selectedRichKeyIndex];
                ParticleFloatKey       key  = base;
                key.t                       = nextInsertedTime(parameter.floatCurveValue, selectedRichKeyIndex);
                parameter.floatCurveValue.push_back(key);
                normalizeFloatCurve(parameter.floatCurveValue, definition.minValue, definition.maxValue);
                selectedRichKeyIndex = nearestFloatKey(parameter.floatCurveValue, key.t);
            }
            else if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                const ParticleColorKey base = parameter.colorGradientValue[selectedRichKeyIndex];
                ParticleColorKey       key  = base;
                key.t                       = nextInsertedTime(parameter.colorGradientValue, selectedRichKeyIndex);
                parameter.colorGradientValue.push_back(key);
                normalizeColorGradient(parameter.colorGradientValue);
                selectedRichKeyIndex = nearestColorKey(parameter.colorGradientValue, key.t);
            }
            else if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                ParticleBurst burst = parameter.burstTimelineValue.empty()
                                          ? ParticleBurst{0.0f, 8u, 8u, 0.0f, 0u}
                                          : parameter.burstTimelineValue[selectedRichKeyIndex];
                burst.time          = std::min(12.0f, burst.time + 0.1f);
                parameter.burstTimelineValue.push_back(burst);
                normalizeBursts(parameter.burstTimelineValue);
                selectedRichKeyIndex = nearestBurst(parameter.burstTimelineValue, burst.time);
            }
        }

        void deleteRichKey(ParticleModuleParameter& parameter, const ParticleModuleParameterDefinition& definition)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);

            if (definition.type == ParticleModuleParameterType::FloatCurve && parameter.floatCurveValue.size() > 2)
                parameter.floatCurveValue.erase(parameter.floatCurveValue.begin() +
                                                static_cast<std::ptrdiff_t>(selectedRichKeyIndex));
            else if (definition.type == ParticleModuleParameterType::ColorGradient &&
                     parameter.colorGradientValue.size() > 2)
                parameter.colorGradientValue.erase(parameter.colorGradientValue.begin() +
                                                   static_cast<std::ptrdiff_t>(selectedRichKeyIndex));
            else if (definition.type == ParticleModuleParameterType::BurstTimeline &&
                     !parameter.burstTimelineValue.empty())
                parameter.burstTimelineValue.erase(parameter.burstTimelineValue.begin() +
                                                   static_cast<std::ptrdiff_t>(selectedRichKeyIndex));

            clampRichSelection(parameter, definition);
        }

        std::string richButtonLabel(const ParticleModuleParameter&           parameter,
                                    const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::BurstTimeline && parameter.burstTimelineValue.empty())
            {
                return compact(definition.label + " Empty", 24);
            }

            std::string label = definition.label + " K" + std::to_string(selectedRichKeyIndex + 1) + " ";
            label += richFieldLabel(definition);
            label += " " + formatValue(richSliderValue(parameter, definition), isRichFieldWholeNumber(definition));
            return compact(label, 24);
        }

        std::string richStatusLabel(const ParticleModuleParameterDefinition& definition) const
        {
            return definition.label + " " + richFieldLabel(definition);
        }

        std::string richFieldLabel(const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
                return selectedRichField == 0 ? "T" : "V";
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                static const char* labels[] = {"T", "R", "G", "B", "A"};
                return labels[std::min<uint32_t>(selectedRichField, 4u)];
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                static const char* labels[] = {"Time", "Min", "Max", "Repeat", "Loops"};
                return labels[std::min<uint32_t>(selectedRichField, 4u)];
            }
            return "Value";
        }

        static size_t richKeyCount(const ParticleModuleParameter&           parameter,
                                   const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
                return parameter.floatCurveValue.size();
            if (definition.type == ParticleModuleParameterType::ColorGradient)
                return parameter.colorGradientValue.size();
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
                return parameter.burstTimelineValue.size();
            return 0;
        }

        static uint32_t richFieldCount(const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
                return 2u;
            if (definition.type == ParticleModuleParameterType::ColorGradient)
                return 5u;
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
                return 5u;
            return 1u;
        }

        bool isRichFieldWholeNumber(const ParticleModuleParameterDefinition& definition) const
        {
            return definition.type == ParticleModuleParameterType::BurstTimeline &&
                   (selectedRichField == 1 || selectedRichField == 2 || selectedRichField == 4);
        }

        UiColor richSliderColor(const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                if (selectedRichField == 1)
                    return ToolTheme::error;
                if (selectedRichField == 2)
                    return ToolTheme::success;
                if (selectedRichField == 3)
                    return ToolTheme::axisZ;
                if (selectedRichField == 4)
                    return ToolTheme::alpha;
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
                return ToolTheme::warning;
            return parameterSliderColor(definition);
        }

        template <typename Curve> static float nextInsertedTime(const Curve& curve, size_t index)
        {
            if (curve.empty())
                return 0.0f;
            if (index + 1 < curve.size())
                return std::clamp((curve[index].t + curve[index + 1].t) * 0.5f, 0.0f, 1.0f);
            return std::clamp((curve[index].t + 1.0f) * 0.5f, 0.0f, 1.0f);
        }

        static void normalizeFloatCurve(ParticleFloatCurve& curve, float minValue, float maxValue)
        {
            for (ParticleFloatKey& key : curve)
            {
                key.t     = std::clamp(key.t, 0.0f, 1.0f);
                key.value = std::clamp(key.value, minValue, maxValue);
            }
            std::sort(curve.begin(),
                      curve.end(),
                      [](const ParticleFloatKey& lhs, const ParticleFloatKey& rhs)
                      {
                          return lhs.t < rhs.t;
                      });
        }

        static void normalizeColorGradient(ParticleColorCurve& curve)
        {
            for (ParticleColorKey& key : curve)
            {
                key.t     = std::clamp(key.t, 0.0f, 1.0f);
                key.color = glm::clamp(key.color, glm::vec4(0.0f), glm::vec4(1.0f));
            }
            std::sort(curve.begin(),
                      curve.end(),
                      [](const ParticleColorKey& lhs, const ParticleColorKey& rhs)
                      {
                          return lhs.t < rhs.t;
                      });
        }

        static void normalizeBursts(std::vector<ParticleBurst>& bursts)
        {
            for (ParticleBurst& burst : bursts)
            {
                burst.time           = std::clamp(burst.time, 0.0f, 12.0f);
                burst.countMax       = std::max(burst.countMin, burst.countMax);
                burst.repeatInterval = std::max(0.0f, burst.repeatInterval);
            }
            std::sort(bursts.begin(),
                      bursts.end(),
                      [](const ParticleBurst& lhs, const ParticleBurst& rhs)
                      {
                          return lhs.time < rhs.time;
                      });
        }

        static size_t nearestFloatKey(const ParticleFloatCurve& curve, float t)
        {
            return nearestByTime(curve, t);
        }

        static size_t nearestColorKey(const ParticleColorCurve& curve, float t)
        {
            return nearestByTime(curve, t);
        }

        static size_t nearestBurst(const std::vector<ParticleBurst>& bursts, float time)
        {
            if (bursts.empty())
                return 0;
            size_t best         = 0;
            float  bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < bursts.size(); ++i)
            {
                const float distance = std::abs(bursts[i].time - time);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best         = i;
                }
            }
            return best;
        }

        template <typename Curve> static size_t nearestByTime(const Curve& curve, float t)
        {
            if (curve.empty())
                return 0;
            size_t best         = 0;
            float  bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < curve.size(); ++i)
            {
                const float distance = std::abs(curve[i].t - t);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best         = i;
                }
            }
            return best;
        }

        static UiColor parameterSliderColor(const ParticleModuleParameterDefinition& definition)
        {
            if (definition.id.find("TintR") != std::string::npos ||
                definition.id.find("baseTintR") != std::string::npos)
                return ToolTheme::error;
            if (definition.id.find("TintG") != std::string::npos ||
                definition.id.find("baseTintG") != std::string::npos)
                return ToolTheme::success;
            if (definition.id.find("TintB") != std::string::npos ||
                definition.id.find("baseTintB") != std::string::npos)
                return ToolTheme::axisZ;
            if (definition.id.find("alpha") != std::string::npos || definition.id.find("Alpha") != std::string::npos)
                return ToolTheme::alpha;
            return ToolTheme::accent;
        }

        static EditorPropertyValue defaultPropertyValue(const ParticleModuleParameterDefinition& definition)
        {
            switch (definition.type)
            {
            case ParticleModuleParameterType::Float:
            {
                EditorPropertyValue value;
                value.type       = EditorPropertyValueType::Float;
                value.floatValue = definition.defaultFloat;
                return value;
            }
            case ParticleModuleParameterType::UInt:
            {
                EditorPropertyValue value;
                value.type      = EditorPropertyValueType::UInt;
                value.uintValue = definition.defaultUInt;
                return value;
            }
            case ParticleModuleParameterType::Bool:
            {
                EditorPropertyValue value;
                value.type      = EditorPropertyValueType::Bool;
                value.boolValue = definition.defaultBool;
                return value;
            }
            case ParticleModuleParameterType::Enum:
                return textPropertyValue(EditorPropertyValueType::Enum, enumLabel(definition, definition.defaultUInt));
            case ParticleModuleParameterType::String:
                return textPropertyValue(definition.assetPicker == gts::particles::ParticleModuleAssetPicker::None
                                             ? EditorPropertyValueType::Text
                                             : EditorPropertyValueType::Asset,
                                         definition.defaultString);
            case ParticleModuleParameterType::FloatCurve:
                return textPropertyValue(EditorPropertyValueType::Curve, "Curve");
            case ParticleModuleParameterType::ColorGradient:
                return textPropertyValue(EditorPropertyValueType::Gradient, "Gradient");
            case ParticleModuleParameterType::BurstTimeline:
                return textPropertyValue(EditorPropertyValueType::Text, "Bursts");
            }
            return {};
        }

        static EditorPropertyValue parameterPropertyValue(const ParticleModuleParameter&           parameter,
                                                          const ParticleModuleParameterDefinition& definition)
        {
            switch (definition.type)
            {
            case ParticleModuleParameterType::Float:
            {
                EditorPropertyValue value;
                value.type       = EditorPropertyValueType::Float;
                value.floatValue = parameter.floatValue;
                return value;
            }
            case ParticleModuleParameterType::UInt:
            {
                EditorPropertyValue value;
                value.type      = EditorPropertyValueType::UInt;
                value.uintValue = parameter.uintValue;
                return value;
            }
            case ParticleModuleParameterType::Bool:
            {
                EditorPropertyValue value;
                value.type      = EditorPropertyValueType::Bool;
                value.boolValue = parameter.boolValue;
                return value;
            }
            case ParticleModuleParameterType::Enum:
                return textPropertyValue(EditorPropertyValueType::Enum, enumLabel(definition, parameter.uintValue));
            case ParticleModuleParameterType::String:
                return textPropertyValue(definition.assetPicker == gts::particles::ParticleModuleAssetPicker::None
                                             ? EditorPropertyValueType::Text
                                             : EditorPropertyValueType::Asset,
                                         parameter.stringValue);
            case ParticleModuleParameterType::FloatCurve:
                return textPropertyValue(EditorPropertyValueType::Curve, "Curve");
            case ParticleModuleParameterType::ColorGradient:
                return textPropertyValue(EditorPropertyValueType::Gradient, "Gradient");
            case ParticleModuleParameterType::BurstTimeline:
                return textPropertyValue(EditorPropertyValueType::Text, "Bursts");
            }
            return {};
        }

        static EditorPropertyDescriptor parameterDescriptor(const ParticleModuleParameter&           parameter,
                                                            const ParticleModuleParameterDefinition& definition)
        {
            EditorPropertyDescriptor descriptor;
            descriptor.metadata.id             = definition.id;
            descriptor.metadata.displayName    = definition.label;
            descriptor.metadata.description    = definition.id;
            descriptor.metadata.tooltip        = definition.id;
            descriptor.metadata.defaultValue   = defaultPropertyValue(definition);
            descriptor.metadata.limits.hasMin  = definition.type == ParticleModuleParameterType::Float ||
                                                 definition.type == ParticleModuleParameterType::UInt;
            descriptor.metadata.limits.hasMax  = descriptor.metadata.limits.hasMin;
            descriptor.metadata.limits.min     = definition.minValue;
            descriptor.metadata.limits.max     = definition.maxValue;
            descriptor.metadata.limits.softMin = definition.minValue;
            descriptor.metadata.limits.softMax = definition.maxValue;
            descriptor.metadata.limits.step    = definition.wholeNumber ? 1.0f : 0.01f;

            switch (definition.type)
            {
            case ParticleModuleParameterType::Float:
                descriptor.metadata.type = EditorPropertyValueType::Float;
                break;
            case ParticleModuleParameterType::UInt:
                descriptor.metadata.type = EditorPropertyValueType::UInt;
                break;
            case ParticleModuleParameterType::Bool:
                descriptor.metadata.type = EditorPropertyValueType::Bool;
                break;
            case ParticleModuleParameterType::Enum:
                descriptor.metadata.type = EditorPropertyValueType::Enum;
                for (const ParticleModuleEnumOption& option : definition.enumOptions)
                    descriptor.metadata.enumOptions.push_back({std::to_string(option.value), option.label});
                break;
            case ParticleModuleParameterType::String:
                descriptor.metadata.type = definition.assetPicker == gts::particles::ParticleModuleAssetPicker::None
                                               ? EditorPropertyValueType::Text
                                               : EditorPropertyValueType::Asset;
                if (definition.assetPicker == gts::particles::ParticleModuleAssetPicker::Texture)
                    descriptor.metadata.assetType = "Texture";
                else if (definition.assetPicker == gts::particles::ParticleModuleAssetPicker::Mesh)
                    descriptor.metadata.assetType = "Mesh";
                break;
            case ParticleModuleParameterType::FloatCurve:
                descriptor.metadata.type = EditorPropertyValueType::Curve;
                break;
            case ParticleModuleParameterType::ColorGradient:
                descriptor.metadata.type = EditorPropertyValueType::Gradient;
                break;
            case ParticleModuleParameterType::BurstTimeline:
                descriptor.metadata.type  = EditorPropertyValueType::Text;
                descriptor.metadata.group = "Timeline";
                break;
            }

            descriptor.value = parameterPropertyValue(parameter, definition);
            return descriptor;
        }

        static EditorPropertySpec parameterPropertySpec(const ParticleModuleParameter&           parameter,
                                                        const ParticleModuleParameterDefinition& definition)
        {
            const EditorPropertyDescriptor descriptor = parameterDescriptor(parameter, definition);
            return toPropertySpec(descriptor, {descriptor});
        }

        std::string parameterButtonLabel(const ParticleModuleParameter&           parameter,
                                         const ParticleModuleParameterDefinition& definition) const
        {
            const EditorPropertySpec spec = parameterPropertySpec(parameter, definition);
            if (definition.type == ParticleModuleParameterType::Bool)
                return spec.displayName + (parameter.boolValue ? " On" : " Off");
            if (definition.type == ParticleModuleParameterType::Enum)
                return spec.displayName + " " + spec.value;
            if (definition.type == ParticleModuleParameterType::String)
                return spec.displayName + " " +
                       compact(parameter.stringValue.empty() ? "-" : fileName(parameter.stringValue), 14);
            return spec.displayName;
        }

        std::string assetPickerButtonLabel(const ParticleModuleParameter&           parameter,
                                           const ParticleModuleParameterDefinition& definition) const
        {
            return definition.label + " " +
                   compact(parameter.stringValue.empty() ? "None" : fileName(parameter.stringValue), 22);
        }

        std::string assetPathAtOffset(gts::particles::ParticleModuleAssetPicker picker,
                                      const std::string&                        current,
                                      int                                       offset) const
        {
            std::vector<std::string> paths = pickerChoices(picker, current);
            if (paths.empty())
                return current;

            auto it = std::find(paths.begin(), paths.end(), current);
            if (it == paths.end())
                it = paths.begin();

            const int count = static_cast<int>(paths.size());
            int       index = static_cast<int>(std::distance(paths.begin(), it));
            if (current.empty() && offset > 0 && paths.size() > 1u)
                index = 0;
            index = (index + offset) % count;
            if (index < 0)
                index += count;
            return paths[static_cast<size_t>(index)];
        }

        static std::vector<std::string> pickerChoices(gts::particles::ParticleModuleAssetPicker picker,
                                                      const std::string&                        current)
        {
            std::vector<std::string> paths = collectPickerPaths(picker);
            if (!current.empty())
                paths.push_back(current);
            paths.insert(paths.begin(), {});
            sortUnique(paths);
            return paths;
        }

        static std::vector<std::string> collectPickerPaths(gts::particles::ParticleModuleAssetPicker picker)
        {
            std::vector<std::string> paths;
            if (picker == gts::particles::ParticleModuleAssetPicker::None)
                return paths;

            const std::vector<std::string> extensions =
                picker == gts::particles::ParticleModuleAssetPicker::Texture
                    ? std::vector<std::string>{".png", ".jpg", ".jpeg", ".ktx", ".ktx2"}
                    : std::vector<std::string>{".obj", ".gltf", ".glb"};
            const std::vector<std::filesystem::path> roots = {
                "resources",
                "../resources",
                "../../resources",
                "../../../resources",
                "engine/resources",
                "../engine/resources",
                "../../engine/resources",
            };

            for (const std::filesystem::path& rootPath : roots)
                scanPickerDirectory(paths, rootPath, extensions);
            sortUnique(paths);
            return paths;
        }

        static void scanPickerDirectory(std::vector<std::string>&       paths,
                                        const std::filesystem::path&    rootPath,
                                        const std::vector<std::string>& extensions)
        {
            std::error_code ec;
            if (!std::filesystem::exists(rootPath, ec) || !std::filesystem::is_directory(rootPath, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator       it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && extensionAllowed(entry.path().extension().string(), extensions))
                    paths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        static bool extensionAllowed(std::string extension, const std::vector<std::string>& allowed)
        {
            std::transform(extension.begin(),
                           extension.end(),
                           extension.begin(),
                           [](unsigned char ch)
                           {
                               return static_cast<char>(std::tolower(ch));
                           });
            return std::find(allowed.begin(), allowed.end(), extension) != allowed.end();
        }

        static std::string enumLabel(const ParticleModuleParameterDefinition& definition, uint32_t value)
        {
            for (const ParticleModuleEnumOption& option : definition.enumOptions)
            {
                if (option.value == value)
                    return option.label;
            }
            return std::to_string(value);
        }

        static uint32_t nextEnumValue(const ParticleModuleParameterDefinition& definition, uint32_t current)
        {
            if (definition.enumOptions.empty())
                return current;

            for (size_t i = 0; i < definition.enumOptions.size(); ++i)
            {
                if (definition.enumOptions[i].value != current)
                    continue;
                return definition.enumOptions[(i + 1) % definition.enumOptions.size()].value;
            }
            return definition.enumOptions.front().value;
        }

        void syncSelectedEmitterDescriptor(ParticleEffectEmitter& emitter)
        {
            gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
            gts::particles::syncParticleGraphWithModules(emitter);
            emitter.compiledProgram = gts::particles::compileParticleEffectEmitter(emitter);
            if (emitter.compiledProgram.valid)
                emitter.descriptor = emitter.compiledProgram.runtimeDescriptor;
        }

        static void setEmitterEnabledModuleParameter(ParticleEffectEmitter& emitter, bool enabled)
        {
            for (ParticleModuleInstance& module : emitter.modules)
            {
                if (module.typeId != "spawn.basic")
                    continue;
                if (ParticleModuleParameter* parameter = gts::particles::findParameter(module, "emitterEnabled"))
                    parameter->boolValue = enabled;
                return;
            }
        }

        void applyKeyboardShortcuts(EngineToolContext& ctx, EngineToolStateComponent& state)
        {
            if (activeTextField != ActiveTextField::None || ctx.input == nullptr)
                return;

            const std::optional<InputTrigger> trigger = ctx.input->getLastPressedTrigger();
            if (!trigger.has_value() || trigger->type != InputTrigger::Type::Key)
                return;

            const GtsKey key   = static_cast<GtsKey>(trigger->code);
            const bool   ctrl  = has(trigger->modifiers, ModifierFlags::Ctrl);
            const bool   shift = has(trigger->modifiers, ModifierFlags::Shift);

            if (ctrl)
            {
                if (key == GtsKey::Z)
                    undo(ctx.world, state);
                else if (key == GtsKey::Y)
                    redo(ctx.world, state);
                else if (key == GtsKey::C)
                    copyModule(state);
                else if (key == GtsKey::V)
                    pasteModule(ctx.world, state);
                else if (key == GtsKey::S)
                {
                    saveCurrent(state, currentPath);
                    applySelectedEmitterToLive(ctx.world, false);
                }
                else if (key == GtsKey::R)
                    reloadCurrent(ctx.world, state);
                else if (key == GtsKey::D)
                    duplicateEmitter(state);
                else if (key == GtsKey::ArrowUp)
                    moveEmitter(state, -1);
                else if (key == GtsKey::ArrowDown)
                    moveEmitter(state, 1);
                return;
            }

            if (key == GtsKey::F2)
                renameEmitter(state);
            else if (key == GtsKey::Backspace)
                deleteEmitter(state);
            else if (key == GtsKey::F)
            {
                if (shift)
                    frameEntireEffectCamera(state, true);
                else
                    frameSelectedEmitterCamera(state, true);
            }
        }

        void releaseUndoCapture(const UiInteractionResult& interaction)
        {
            if (interaction.pressed == UI_INVALID_HANDLE)
                activeUndoHandle = UI_INVALID_HANDLE;
        }

        void beginUndoForHandle(UiHandle handle)
        {
            if (handle == UI_INVALID_HANDLE || activeUndoHandle == handle)
                return;
            pushUndoSnapshot();
            activeUndoHandle = handle;
        }

        void pushUndoSnapshot()
        {
            if (!hasAsset)
                return;
            undoStack.push_back(currentAsset);
            if (undoStack.size() > MaxUndoSnapshots)
                undoStack.erase(undoStack.begin());
            redoStack.clear();
        }

        void undo(ECSWorld& world, EngineToolStateComponent& state)
        {
            if (undoStack.empty())
            {
                state.status = "NOTHING TO UNDO";
                return;
            }

            redoStack.push_back(currentAsset);
            currentAsset = undoStack.back();
            undoStack.pop_back();
            restoreAfterHistory(world, state, "UNDO");
        }

        void redo(ECSWorld& world, EngineToolStateComponent& state)
        {
            if (redoStack.empty())
            {
                state.status = "NOTHING TO REDO";
                return;
            }

            undoStack.push_back(currentAsset);
            currentAsset = redoStack.back();
            redoStack.pop_back();
            restoreAfterHistory(world, state, "REDO");
        }

        void restoreAfterHistory(ECSWorld& world, EngineToolStateComponent& state, const std::string& status)
        {
            dirty = true;
            selectedEmitterIndex =
                std::min(selectedEmitterIndex, currentAsset.emitters.empty() ? 0 : currentAsset.emitters.size() - 1);
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            selectedEmitterIds.clear();
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            state.status = status;
            applySelectedEmitterToLive(world, true);
        }

        void copyModule(EngineToolStateComponent& state)
        {
            const ParticleModuleInstance* module = selectedModule();
            if (module == nullptr)
            {
                state.status = "NO MODULE";
                return;
            }

            moduleClipboard = *module;
            state.status    = "MODULE COPIED";
        }

        void pasteModule(ECSWorld& world, EngineToolStateComponent& state)
        {
            ParticleEffectEmitter*  emitter = selectedEmitter();
            ParticleModuleInstance* module  = selectedModule();
            if (emitter == nullptr || module == nullptr)
            {
                state.status = "NO MODULE";
                return;
            }
            if (!moduleClipboard.has_value())
            {
                state.status = "NO MODULE COPY";
                return;
            }
            if (moduleClipboard->typeId != module->typeId)
            {
                state.status = "MODULE TYPE MISMATCH";
                return;
            }

            pushUndoSnapshot();
            const std::string stableId = module->stableId;
            *module                    = *moduleClipboard;
            module->stableId           = stableId;
            syncSelectedEmitterDescriptor(*emitter);
            markDirty(state, "MODULE PASTED");
            applySelectedEmitterToLive(world, false);
        }

        void copyEmitters(EngineToolStateComponent& state)
        {
            emitterClipboard.clear();
            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            if (!targets.empty())
            {
                for (size_t index : targets)
                    emitterClipboard.push_back(currentAsset.emitters[index]);
            }
            else if (const ParticleEffectEmitter* emitter = selectedEmitter())
            {
                emitterClipboard.push_back(*emitter);
            }

            state.status = emitterClipboard.empty() ? "NO EMITTER" : "EMITTER COPIED";
        }

        void pasteEmitters(EngineToolStateComponent& state)
        {
            if (emitterClipboard.empty())
            {
                state.status = "NO EMITTER COPY";
                return;
            }

            pushUndoSnapshot();
            size_t insertIndex = std::min(selectedEmitterIndex + 1, currentAsset.emitters.size());
            selectedEmitterIds.clear();
            for (const ParticleEffectEmitter& copied : emitterClipboard)
            {
                ParticleEffectEmitter pasted = copied;
                pasted.stableId              = uniqueEmitterId(copied.stableId.empty() ? "emitter" : copied.stableId);
                pasted.name                  = copied.name.empty() ? "Emitter Copy" : copied.name + " Copy";
                currentAsset.emitters.insert(currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                                             std::move(pasted));
                selectedEmitterIndex = insertIndex;
                insertIndex += 1;
            }
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTER PASTED");
        }

        void toggleSelectedEmitterSelection(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            const auto it = std::find(selectedEmitterIds.begin(), selectedEmitterIds.end(), emitter->stableId);
            if (it == selectedEmitterIds.end())
            {
                selectedEmitterIds.push_back(emitter->stableId);
                state.status = "EMITTER SELECTED";
            }
            else
            {
                selectedEmitterIds.erase(it);
                state.status = "EMITTER UNSELECTED";
            }
        }

        bool isEmitterSelected(const ParticleEffectEmitter& emitter) const
        {
            return std::find(selectedEmitterIds.begin(), selectedEmitterIds.end(), emitter.stableId) !=
                   selectedEmitterIds.end();
        }

        std::vector<size_t> selectedEmitterTargetIndices() const
        {
            std::vector<size_t> targets;
            for (size_t i = 0; i < currentAsset.emitters.size(); ++i)
            {
                if (isEmitterSelected(currentAsset.emitters[i]))
                    targets.push_back(i);
            }
            return targets;
        }

        void deleteSelectedEmitters(EngineToolStateComponent& state, std::vector<size_t> targets)
        {
            if (targets.empty())
                return;
            if (targets.size() >= currentAsset.emitters.size())
            {
                state.status = "KEEP ONE EMITTER";
                return;
            }

            pushUndoSnapshot();
            std::sort(targets.rbegin(), targets.rend());
            for (size_t index : targets)
                currentAsset.emitters.erase(currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(index));
            selectedEmitterIds.clear();
            selectedEmitterIndex   = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTERS DELETED");
        }

        void duplicateSelectedEmitters(EngineToolStateComponent& state, const std::vector<size_t>& targets)
        {
            if (targets.empty())
                return;

            pushUndoSnapshot();
            size_t insertIndex = std::min(selectedEmitterIndex + 1, currentAsset.emitters.size());
            std::vector<ParticleEffectEmitter> copies;
            copies.reserve(targets.size());
            for (size_t index : targets)
                copies.push_back(currentAsset.emitters[index]);

            selectedEmitterIds.clear();
            for (ParticleEffectEmitter& copy : copies)
            {
                copy.stableId = uniqueEmitterId(copy.stableId.empty() ? "emitter" : copy.stableId);
                copy.name     = copy.name.empty() ? "Emitter Copy" : copy.name + " Copy";
                currentAsset.emitters.insert(currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                                             std::move(copy));
                selectedEmitterIndex = insertIndex;
                insertIndex += 1;
            }
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            focusEmitterSelection();
            syncEmitterNameDraft();
            revealSelectedEmitter();
            markDirty(state, "EMITTERS COPIED");
        }

        std::string effectButtonLabel(EffectAction action) const
        {
            switch (action)
            {
            case EffectAction::Save:
                return dirty ? "Save *" : "Save";
            case EffectAction::SaveAs:
                return "Save As";
            case EffectAction::Duplicate:
                return "Copy";
            case EffectAction::Reload:
                return "Reload";
            }
            return "";
        }

        std::string emitterButtonLabel(EmitterAction action) const
        {
            switch (action)
            {
            case EmitterAction::ToggleEnabled:
            {
                const ParticleEffectEmitter* selected = selectedEmitter();
                return selected != nullptr && selected->descriptor.enabled ? "Enabled" : "Disabled";
            }
            case EmitterAction::Add:
                return "+ Add";
            case EmitterAction::Delete:
                return "- Delete";
            case EmitterAction::Duplicate:
                return "Duplicate";
            case EmitterAction::Rename:
                return "Rename";
            case EmitterAction::MoveUp:
                return "Up";
            case EmitterAction::MoveDown:
                return "Down";
            case EmitterAction::ToggleSelection:
                return selectedEmitter() != nullptr && isEmitterSelected(*selectedEmitter()) ? "Selected" : "Select";
            case EmitterAction::Copy:
                return selectedEmitterIds.empty() ? "Copy" : "Copy " + std::to_string(selectedEmitterIds.size());
            case EmitterAction::Paste:
                return emitterClipboard.empty() ? "Paste" : "Paste *";
            }
            return "";
        }

        std::string moduleButtonLabel(ModuleAction action) const
        {
            switch (action)
            {
            case ModuleAction::Copy:
                return "Copy";
            case ModuleAction::Paste:
                return moduleClipboard.has_value() ? "Paste *" : "Paste";
            case ModuleAction::Undo:
                return undoStack.empty() ? "Undo" : "Undo *";
            case ModuleAction::Redo:
                return redoStack.empty() ? "Redo" : "Redo *";
            }
            return "";
        }

        std::string graphButtonLabel(GraphAction action) const
        {
            switch (action)
            {
            case GraphAction::Search:
                return "Search";
            case GraphAction::AddNode:
                return "Add";
            case GraphAction::Link:
                return "Link";
            case GraphAction::Frame:
                return "Frame";
            case GraphAction::Comment:
                return "Note";
            }
            return "";
        }

        std::string playbackButtonLabel(PlaybackAction action) const
        {
            switch (action)
            {
            case PlaybackAction::PlayPause:
                return playbackPaused ? "Play" : "Pause";
            case PlaybackAction::Restart:
                return "Restart";
            case PlaybackAction::Background:
                return "Bg";
            case PlaybackAction::CameraReset:
                return "Camera";
            }
            return "";
        }

        float readSliderValue(FloatField field) const
        {
            switch (field)
            {
            case FloatField::TimeScale:
                return playbackTimeScale;
            case FloatField::OrbitYaw:
                return previewOrbitYawDegrees();
            case FloatField::OrbitDistance:
                return currentAsset.preview.orbitDistance;
            }
            return 0.0f;
        }

        float previewOrbitYawDegrees() const
        {
            const glm::vec3 offset = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            return glm::degrees(std::atan2(offset.x, offset.z));
        }

        void setPreviewOrbitYawDegrees(float degrees)
        {
            const float     yawRadians     = glm::radians(degrees);
            const glm::vec3 offset         = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            const float     planarDistance = std::sqrt(offset.x * offset.x + offset.z * offset.z);
            const float     radius =
                std::max(0.1f, planarDistance > 0.001f ? planarDistance : currentAsset.preview.orbitDistance);
            currentAsset.preview.orbitDistance = radius;
            currentAsset.preview.cameraPosition =
                currentAsset.preview.cameraTarget +
                glm::vec3(std::sin(yawRadians) * radius, offset.y, std::cos(yawRadians) * radius);
        }

        void setPreviewOrbitDistance(float distance)
        {
            const float clampedDistance = std::clamp(distance, 0.75f, 80.0f);
            glm::vec3   offset          = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            if (glm::dot(offset, offset) <= 0.0001f)
                offset = {0.0f, 0.25f, 1.0f};
            offset = glm::normalize(offset) * clampedDistance;
            currentAsset.preview.orbitDistance  = clampedDistance;
            currentAsset.preview.cameraPosition = currentAsset.preview.cameraTarget + offset;
        }

        void panPreviewCamera(float deltaX, float deltaY)
        {
            const glm::vec3 forward =
                glm::normalize(currentAsset.preview.cameraTarget - currentAsset.preview.cameraPosition);
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            const glm::vec3 up    = glm::normalize(glm::cross(right, forward));
            const float     scale = std::max(0.25f, currentAsset.preview.orbitDistance) * 1.35f;
            const glm::vec3 delta = (-right * deltaX + up * deltaY) * scale;
            currentAsset.preview.cameraPosition += delta;
            currentAsset.preview.cameraTarget += delta;
        }

        void frameSelectedEmitterCamera(EngineToolStateComponent& state, bool markAsDirty)
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER SELECTED";
                return;
            }

            frameCameraForDescriptor(emitter->descriptor, markAsDirty);
            state.status = "FRAMED EMITTER";
        }

        void frameEntireEffectCamera(EngineToolStateComponent& state, bool markAsDirty)
        {
            if (!hasAsset || currentAsset.emitters.empty())
            {
                state.status = "NO EFFECT TO FRAME";
                return;
            }

            float radius = 1.0f;
            for (const ParticleEffectEmitter& emitter : currentAsset.emitters)
                radius = std::max(radius, estimatedEmitterRadius(emitter.descriptor));

            frameCameraForRadius(radius, markAsDirty);
            state.status = "FRAMED EFFECT";
        }

        void autoFrameOpenedEffect()
        {
            if (!hasAsset || currentAsset.emitters.empty())
                return;

            float radius = 1.0f;
            for (const ParticleEffectEmitter& emitter : currentAsset.emitters)
                radius = std::max(radius, estimatedEmitterRadius(emitter.descriptor));
            frameCameraForRadius(radius, false);
        }

        void frameCameraForDescriptor(const ParticleEmitterComponent& descriptor, bool markAsDirty)
        {
            frameCameraForRadius(estimatedEmitterRadius(descriptor), markAsDirty);
        }

        void frameCameraForRadius(float radius, bool markAsDirty)
        {
            if (markAsDirty)
            {
                pushUndoSnapshot();
                dirty = true;
            }

            const float safeRadius = std::clamp(radius, 0.75f, 30.0f);
            currentAsset.preview.cameraTarget   = {0.0f, safeRadius * 0.35f, 0.0f};
            currentAsset.preview.orbitDistance  = std::max(3.0f, safeRadius * 2.65f);
            currentAsset.preview.cameraPosition = currentAsset.preview.cameraTarget +
                                                  glm::vec3(0.0f,
                                                            currentAsset.preview.orbitDistance * 0.28f,
                                                            currentAsset.preview.orbitDistance);
        }

        static float estimatedEmitterRadius(const ParticleEmitterComponent& descriptor)
        {
            float shapeRadius = 1.0f;
            switch (descriptor.shape)
            {
            case ParticleEmitterShape::Sphere:
                shapeRadius = descriptor.sphereRadius;
                break;
            case ParticleEmitterShape::Box:
                shapeRadius = glm::length(descriptor.boxExtents);
                break;
            case ParticleEmitterShape::Disc:
                shapeRadius = descriptor.discRadius;
                break;
            case ParticleEmitterShape::Cylinder:
                shapeRadius = std::max(descriptor.cylinderRadius, descriptor.cylinderHeight * 0.5f);
                break;
            case ParticleEmitterShape::Ring:
                shapeRadius = descriptor.ringOuterRadius;
                break;
            }

            const float velocityRadius =
                glm::length(descriptor.initialVelocity) * std::max(descriptor.lifetimeMax, descriptor.lifetimeMin);
            const float sizeRadius =
                descriptor.sizeOverLifetime.empty() ? 0.25f : std::max(descriptor.sizeOverLifetime.back().value, 0.25f);
            return std::max(0.75f,
                            shapeRadius + velocityRadius + sizeRadius);
        }

        UiColor previewColor() const
        {
            if (!hasAsset)
            {
                UiColor background = ToolTheme::viewportBackground;
                background.a       = 0.060f;
                return background;
            }
            const glm::vec4& colorValue = currentAsset.preview.backgroundColor;
            return {colorValue.r, colorValue.g, colorValue.b, std::max(0.020f, std::min(colorValue.a, 0.060f))};
        }

        static UiColor sliderColor(FloatField field)
        {
            switch (field)
            {
            case FloatField::TimeScale:
                return ToolTheme::info;
            case FloatField::OrbitYaw:
                return ToolTheme::secondaryAccent;
            case FloatField::OrbitDistance:
                return ToolTheme::accent;
            }
            return ToolTheme::accent;
        }

        ParticleEffectEmitter* selectedEmitter()
        {
            if (!hasAsset || currentAsset.emitters.empty())
                return nullptr;
            selectedEmitterIndex = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            return &currentAsset.emitters[selectedEmitterIndex];
        }

        const ParticleEffectEmitter* selectedEmitter() const
        {
            if (!hasAsset || currentAsset.emitters.empty())
                return nullptr;
            return &currentAsset.emitters[std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1)];
        }

        ParticleModuleInstance* selectedModule()
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || emitter->modules.empty())
                return nullptr;
            clampModuleSelection(*emitter);
            return &emitter->modules[selectedModuleIndex];
        }

        ParticleGraphNode* selectedGraphNode(ParticleEffectEmitter& emitter)
        {
            ParticleModuleInstance* module = selectedModule();
            if (module == nullptr)
                return nullptr;
            gts::particles::syncParticleGraphWithModules(emitter);
            return gts::particles::findParticleGraphNodeForModule(emitter.graph, module->stableId);
        }

        void selectModuleByStableId(ParticleEffectEmitter& emitter, const std::string& stableId)
        {
            const auto it = std::find_if(emitter.modules.begin(),
                                         emitter.modules.end(),
                                         [&](const ParticleModuleInstance& module)
                                         {
                                             return module.stableId == stableId;
                                         });
            if (it == emitter.modules.end())
                return;

            selectedModuleIndex = static_cast<size_t>(std::distance(emitter.modules.begin(), it));
            moduleBrowserOffset = std::min(moduleBrowserOffset, maxModuleBrowserOffset(emitter));
            if (selectedModuleIndex < moduleBrowserOffset)
                moduleBrowserOffset = selectedModuleIndex;
            else if (selectedModuleIndex >= moduleBrowserOffset + ModuleRowCount)
                moduleBrowserOffset = selectedModuleIndex - ModuleRowCount + 1;
            clearParameterSelectionState();
            focusModuleSelection();
        }

        const ParticleModuleInstance* selectedModule() const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || emitter->modules.empty())
                return nullptr;
            return &emitter->modules[std::min(selectedModuleIndex, emitter->modules.size() - 1)];
        }

        void syncTextDraftsForDisplay()
        {
            if (!hasAsset)
            {
                effectNameDraft.clear();
                emitterNameDraft.clear();
                return;
            }

            if (activeTextField != ActiveTextField::EffectName)
                effectNameDraft = currentAsset.metadata.name;
            if (activeTextField != ActiveTextField::EmitterName)
                syncEmitterNameDraft();
        }

        void syncEmitterNameDraft()
        {
            const ParticleEffectEmitter* selected = selectedEmitter();
            emitterNameDraft                      = selected == nullptr ? std::string{} : selected->name;
        }

        void clearParameterSelectionState()
        {
            selectedRichParameterId.clear();
            selectedRichKeyIndex = 0;
            selectedRichField    = 0;
            selectedRangeParameterId.clear();
            selectedRangeMax = false;
        }

        void focusEffectSelection()
        {
            selectedObject = FxSelection{FxSelectionKind::Effect, 0, 0, {}};
            selectedInspectorSection = InspectorSection::General;
        }

        void focusEmitterSelection()
        {
            selectedObject = FxSelection{FxSelectionKind::Emitter, selectedEmitterIndex, 0, {}};
            selectedInspectorSection = InspectorSection::General;
        }

        void focusModuleSelection()
        {
            selectedObject =
                FxSelection{FxSelectionKind::Module, selectedEmitterIndex, selectedModuleIndex, {}};
            selectedInspectorSection = InspectorSection::Parameters;
        }

        static std::string textFieldDisplay(const std::string& text, bool focused)
        {
            std::string display = compact(text, focused ? 23 : 24);
            if (focused)
                display += "|";
            return display;
        }

        size_t emitterRowOffset(const std::vector<size_t>& visibleEmitters) const
        {
            return std::min(emitterBrowserOffset, maxEmitterBrowserOffset(visibleEmitters));
        }

        size_t outlinerRowOffset() const
        {
            return std::min(emitterBrowserOffset, maxOutlinerOffset());
        }

        void clampOutlinerOffset()
        {
            emitterBrowserOffset = std::min(emitterBrowserOffset, maxOutlinerOffset());
        }

        size_t maxOutlinerOffset() const
        {
            return visibleOutlinerItems.size() <= EmitterRowCount ? 0 : visibleOutlinerItems.size() - EmitterRowCount;
        }

        void clampEmitterBrowserOffset(const std::vector<size_t>& visibleEmitters)
        {
            emitterBrowserOffset = std::min(emitterBrowserOffset, maxEmitterBrowserOffset(visibleEmitters));
        }

        size_t maxEmitterBrowserOffset(const std::vector<size_t>& visibleEmitters) const
        {
            return visibleEmitters.size() <= EmitterRowCount ? 0 : visibleEmitters.size() - EmitterRowCount;
        }

        void revealSelectedEmitter()
        {
            const std::vector<size_t> visibleEmitters = filteredEmitterIndices();
            const auto it = std::find(visibleEmitters.begin(), visibleEmitters.end(), selectedEmitterIndex);
            if (it == visibleEmitters.end())
            {
                clampEmitterBrowserOffset(visibleEmitters);
                return;
            }

            const size_t visibleIndex = static_cast<size_t>(std::distance(visibleEmitters.begin(), it));
            if (visibleIndex < emitterBrowserOffset)
                emitterBrowserOffset = visibleIndex;
            else if (visibleIndex >= emitterBrowserOffset + EmitterRowCount)
                emitterBrowserOffset = visibleIndex - EmitterRowCount + 1;
            clampEmitterBrowserOffset(visibleEmitters);
        }

        std::vector<size_t> filteredEmitterIndices() const
        {
            std::vector<size_t> indices;
            if (!hasAsset)
                return indices;

            indices.reserve(currentAsset.emitters.size());
            for (size_t i = 0; i < currentAsset.emitters.size(); ++i)
            {
                if (emitterMatchesSearch(currentAsset.emitters[i]))
                    indices.push_back(i);
            }
            return indices;
        }

        bool emitterMatchesSearch(const ParticleEffectEmitter& emitter) const
        {
            if (emitterSearchDraft.empty())
                return true;

            const std::string query = lowerCopy(emitterSearchDraft);
            std::string haystack    = emitter.name + " " + emitter.stableId + " " + emitter.descriptor.effectEmitterId;
            haystack                = lowerCopy(haystack);
            return haystack.find(query) != std::string::npos;
        }

        void markDirty(EngineToolStateComponent& state, const std::string& status)
        {
            dirty        = true;
            state.status = status;
        }

        std::string uniqueEmitterId(const std::string& base) const
        {
            std::string stem = base.empty() ? "emitter" : base;
            for (char& c : stem)
            {
                if (c == ' ')
                    c = '_';
            }

            for (uint32_t i = 1; i < 1000u; ++i)
            {
                const std::string candidate = stem + "_" + std::to_string(i);
                if (!assetHasEmitter(candidate))
                    return candidate;
            }
            return stem + "_new";
        }

        std::string generatedSiblingPath(const std::string& suffix) const
        {
            std::filesystem::path       source = currentPath.empty() ? std::filesystem::path("particle_effect.json")
                                                                     : std::filesystem::path(currentPath);
            const std::filesystem::path parent = source.parent_path();
            const std::string stem      = source.stem().string().empty() ? "particle_effect" : source.stem().string();
            const std::string extension = source.extension().string().empty() ? ".json" : source.extension().string();

            for (uint32_t i = 1; i < 1000u; ++i)
            {
                const std::string           index     = i == 1u ? "" : std::to_string(i);
                const std::filesystem::path candidate = parent / (stem + suffix + index + extension);
                if (candidate.string() != currentPath && !std::filesystem::exists(candidate))
                    return candidate.string();
            }
            return (parent / (stem + suffix + "_new" + extension)).string();
        }

        static std::string fileName(const std::string& path)
        {
            const std::string name = std::filesystem::path(path).filename().string();
            return compact(name.empty() ? path : name, 24);
        }

        static std::string compact(const std::string& text, size_t limit)
        {
            if (text.size() <= limit)
                return text;
            if (limit <= 3)
                return text.substr(0, limit);
            return text.substr(0, limit - 3) + "...";
        }

        static std::string lowerCopy(std::string text)
        {
            std::transform(text.begin(),
                           text.end(),
                           text.begin(),
                           [](unsigned char ch)
                           {
                               return static_cast<char>(std::tolower(ch));
                           });
            return text;
        }

        static bool containsInsensitive(const std::string& text, const std::string& needle)
        {
            if (needle.empty())
                return true;
            return lowerCopy(text).find(lowerCopy(needle)) != std::string::npos;
        }

        static const std::vector<glm::vec4>& backgroundPresets()
        {
            static const std::vector<glm::vec4> presets = {
                {0.02f, 0.025f, 0.030f, 1.0f},
                {0.08f, 0.09f, 0.10f, 1.0f},
                {0.18f, 0.17f, 0.14f, 1.0f},
                {0.48f, 0.54f, 0.60f, 1.0f},
            };
            return presets;
        }

        static uint32_t closestBackgroundPreset(const glm::vec4& value)
        {
            const std::vector<glm::vec4>& presets      = backgroundPresets();
            size_t                        best         = 0;
            float                         bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < presets.size(); ++i)
            {
                const glm::vec4 delta    = presets[i] - value;
                const float     distance = delta.r * delta.r + delta.g * delta.g + delta.b * delta.b;
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best         = i;
                }
            }
            return static_cast<uint32_t>(best);
        }
    };
} // namespace gts::tools
