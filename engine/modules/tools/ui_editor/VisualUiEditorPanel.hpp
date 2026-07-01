#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "EngineToolPanel.h"
#include "EditorPropertySystem.h"
#include "EditorWidgets.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"
#include "VisualUiEditorDocument.h"
#include "VisualUiEditorSamples.h"

namespace gts::tools
{
    class VisualUiEditorPanel final : public EngineToolPanel
    {
    public:
        std::string_view id() const override { return "visual_ui_editor"; }
        std::string_view title() const override { return "UI Editor"; }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            fontPtr = font;
            root = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            createRectRelative(ctx.ui, root, {0.0f, 0.0f, 1.0f, 1.0f}, ToolTheme::paneBackground);

            createTextRelative(ctx.ui,
                               root,
                               {0.020f, 0.018f, 0.620f, 0.040f},
                               fontPtr,
                               "Visual UI Editor",
                               ToolTheme::text,
                               ToolTheme::titleTextScale);
            statusLabel = createTextRelative(ctx.ui,
                                             root,
                                             {0.650f, 0.020f, 0.330f, 0.034f},
                                             fontPtr,
                                             "No asset open",
                                             ToolTheme::mutedText,
                                             ToolTheme::smallTextScale);
            setTextAlignment(ctx.ui, statusLabel, UiHorizontalAlign::Right, UiVerticalAlign::Middle);

            openPromptButton = createButtonRelative(ctx.ui, root, {0.020f, 0.068f, 0.190f, 0.040f}, fontPtr, "Open Prompt", ToolTheme::buttonTextScale);
            editTextButton   = createButtonRelative(ctx.ui, root, {0.220f, 0.068f, 0.180f, 0.040f}, fontPtr, "Edit Text", ToolTheme::buttonTextScale);
            rebuildButton    = createButtonRelative(ctx.ui, root, {0.410f, 0.068f, 0.160f, 0.040f}, fontPtr, "Preview", ToolTheme::buttonTextScale);
            saveButton       = createButtonRelative(ctx.ui, root, {0.580f, 0.068f, 0.160f, 0.040f}, fontPtr, "Save", ToolTheme::buttonTextScale);

            createTextRelative(ctx.ui,
                               root,
                               {0.020f, 0.126f, 0.350f, 0.028f},
                               fontPtr,
                               "Hierarchy",
                               ToolTheme::text,
                               ToolTheme::headerTextScale);
            createTextRelative(ctx.ui,
                               root,
                               {0.390f, 0.126f, 0.280f, 0.028f},
                               fontPtr,
                               "Inspector",
                               ToolTheme::text,
                               ToolTheme::headerTextScale);
            createTextRelative(ctx.ui,
                               root,
                               {0.700f, 0.126f, 0.270f, 0.028f},
                               fontPtr,
                               "Validation",
                               ToolTheme::text,
                               ToolTheme::headerTextScale);

            hierarchyRoot = createRectRelative(ctx.ui, root, {0.020f, 0.160f, 0.350f, 0.600f}, ToolTheme::panelInset);
            inspectorRoot = createRectRelative(ctx.ui, root, {0.390f, 0.160f, 0.290f, 0.600f}, ToolTheme::panelInset);
            validationRoot = createRectRelative(ctx.ui, root, {0.700f, 0.160f, 0.280f, 0.600f}, ToolTheme::panelInset);

            buildHierarchyRows(ctx.ui);
            buildInspectorRows(ctx.ui);
            buildValidationRows(ctx.ui);

            previewLabel = createTextRelative(ctx.ui,
                                              root,
                                              {0.020f, 0.790f, 0.960f, 0.055f},
                                              fontPtr,
                                              "Preview is instantiated into its own UiSurface.",
                                              ToolTheme::mutedText,
                                              ToolTheme::smallTextScale);
            setTextAlignment(ctx.ui, previewLabel, UiHorizontalAlign::Left, UiVerticalAlign::Middle);

            setVisible(ctx.ui, true);
        }

        void update(EngineToolContext& ctx,
                    EngineToolStateComponent& state,
                    const UiInteractionResult& interaction) override
        {
            ensureSampleOpen(ctx.ui);
            document.setPreviewVisible(ctx.ui, true);

            if (wasClicked(interaction, openPromptButton.rect))
                openSample(ctx.ui);
            if (wasClicked(interaction, editTextButton.rect))
            {
                document.selectWidget("label");
                document.setSelectedText("Press F to inspect");
                document.rebuildPreview(ctx.ui);
            }
            if (wasClicked(interaction, rebuildButton.rect))
                document.rebuildPreview(ctx.ui);
            if (wasClicked(interaction, saveButton.rect))
            {
                const std::filesystem::path path =
                    std::filesystem::temp_directory_path() / "gravitas_visual_ui_editor_interaction_prompt.uiwidget.json";
                std::string error;
                if (!document.saveAs(path, &error))
                    state.status = "UI Editor save failed: " + error;
                else
                    state.status = "UI Editor saved " + path.string();
            }

            updateHierarchyRows(ctx.ui, interaction);
            updateInspectorRows(ctx.ui);
            updateValidationRows(ctx.ui);
            updateStatus(ctx.ui, state);
        }

        void onDeactivate(EngineToolContext& ctx) override
        {
            document.setPreviewVisible(ctx.ui, false);
        }

        void setVisible(UiSystem& ui, bool visible) override
        {
            if (root != UI_INVALID_HANDLE)
                setVisibleRecursive(ui, root, visible);
            document.setPreviewVisible(ui, visible);
        }

        void destroy(UiSystem& ui) override
        {
            document.destroyPreview(ui);
            if (root != UI_INVALID_HANDLE)
            {
                ui.removeNode(root);
                root = UI_INVALID_HANDLE;
            }
        }

        VisualUiEditorDocument& documentModel() { return document; }
        const VisualUiEditorDocument& documentModel() const { return document; }

    private:
        static constexpr size_t MaxHierarchyRows  = 12;
        static constexpr size_t MaxInspectorRows  = 13;
        static constexpr size_t MaxValidationRows = 10;

        void ensureSampleOpen(UiSystem& ui)
        {
            if (document.hasAsset())
                return;
            openSample(ui);
        }

        void openSample(UiSystem& ui)
        {
            UiTheme theme = ui.theme();
            registerVisualUiEditorSampleThemeClasses(theme);
            ui.setTheme(theme);
            ui.widgetAssets().registerAsset(createVisualUiEditorStatusPromptAsset());
            UiWidgetAssetDefinition prompt = createVisualUiEditorInteractionPromptAsset();
            document.openAsset(prompt);
            document.selectWidget("label");
            document.rebuildPreview(ui);
        }

        void buildHierarchyRows(UiSystem& ui)
        {
            hierarchyRows.clear();
            float y = 0.018f;
            for (size_t i = 0; i < MaxHierarchyRows; ++i)
            {
                hierarchyRows.push_back(createButtonRelative(
                    ui, hierarchyRoot, {0.018f, y, 0.964f, 0.058f}, fontPtr, "", ToolTheme::smallTextScale));
                y += 0.064f;
            }
        }

        void buildInspectorRows(UiSystem& ui)
        {
            inspectorLabels.clear();
            float y = 0.018f;
            for (size_t i = 0; i < MaxInspectorRows; ++i)
            {
                inspectorLabels.push_back(createTextRelative(
                    ui, inspectorRoot, {0.025f, y, 0.950f, 0.046f}, fontPtr, "", ToolTheme::mutedText, ToolTheme::smallTextScale));
                y += 0.050f;
            }
        }

        void buildValidationRows(UiSystem& ui)
        {
            validationLabels.clear();
            float y = 0.018f;
            for (size_t i = 0; i < MaxValidationRows; ++i)
            {
                validationLabels.push_back(createTextRelative(
                    ui, validationRoot, {0.025f, y, 0.950f, 0.052f}, fontPtr, "", ToolTheme::mutedText, ToolTheme::smallTextScale));
                y += 0.058f;
            }
        }

        void updateHierarchyRows(UiSystem& ui, const UiInteractionResult& interaction)
        {
            const std::vector<VisualUiEditorHierarchyItem> items = document.hierarchy();
            for (size_t i = 0; i < hierarchyRows.size(); ++i)
            {
                if (i >= items.size())
                {
                    updateButton(ui, hierarchyRows[i], "");
                    setVisibleRecursive(ui, hierarchyRows[i].rect, false);
                    continue;
                }

                setVisibleRecursive(ui, hierarchyRows[i].rect, true);
                const VisualUiEditorHierarchyItem& item = items[i];
                if (wasClicked(interaction, hierarchyRows[i].rect))
                    document.selectWidget(item.widgetId);

                const std::string indent(static_cast<size_t>(std::max(0, item.depth)) * 2, ' ');
                std::string label = indent + item.widgetId + "  [" + item.type + "]";
                if (!item.asset.empty())
                    label += " -> " + item.asset;
                updateButton(ui, hierarchyRows[i], label);
                setRectColor(ui, hierarchyRows[i].rect, item.selected ? ToolTheme::buttonActive : ToolTheme::buttonSecondary);
            }
        }

        void updateInspectorRows(UiSystem& ui)
        {
            const std::vector<EditorPropertyDescriptor> properties = document.selectedProperties();
            for (size_t i = 0; i < inspectorLabels.size(); ++i)
            {
                if (i >= properties.size())
                {
                    setText(ui, inspectorLabels[i], "");
                    continue;
                }

                const EditorPropertyDescriptor& property = properties[i];
                setText(ui,
                        inspectorLabels[i],
                        property.metadata.displayName + ": " + propertyValueToDisplayString(property));
            }
        }

        void updateValidationRows(UiSystem& ui)
        {
            UiSerializedValidationResult validation = document.validate(ui.widgetAssets());
            if (validation.valid())
            {
                if (!validationLabels.empty())
                {
                    setText(ui, validationLabels[0], "OK: asset validates");
                    setTextColor(ui, validationLabels[0], ToolTheme::success);
                }
                for (size_t i = 1; i < validationLabels.size(); ++i)
                    setText(ui, validationLabels[i], "");
                return;
            }

            for (size_t i = 0; i < validationLabels.size(); ++i)
            {
                if (i >= validation.issues.size())
                {
                    setText(ui, validationLabels[i], "");
                    continue;
                }
                const UiSerializedValidationIssue& issue = validation.issues[i];
                setText(ui, validationLabels[i], issue.path + ": " + issue.message);
                setTextColor(ui,
                             validationLabels[i],
                             issue.severity == UiSerializedValidationIssue::Severity::Error ? ToolTheme::error
                                                                                              : ToolTheme::warning);
            }
        }

        void updateStatus(UiSystem& ui, EngineToolStateComponent& state)
        {
            const VisualUiEditorPreviewState& preview = document.preview();
            std::string text = document.hasAsset() ? document.asset().id : "No asset";
            if (document.dirty())
                text += " *";
            text += " | preview rebuilds: " + std::to_string(preview.rebuildCount);
            if (preview.surface != UI_INVALID_SURFACE)
                text += " | surface " + std::to_string(preview.surface);
            if (!preview.loadResult.success)
                text += " | preview invalid";
            setText(ui, statusLabel, text);
            setText(ui, previewLabel, "Open -> modify authored widget asset -> validate -> rebuild preview surface -> save.");
            if (state.status.empty())
                state.status = "UI Editor edits widget assets; runtime preview uses a dedicated UiSurface.";
        }

        BitmapFont* fontPtr = nullptr;
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle hierarchyRoot = UI_INVALID_HANDLE;
        UiHandle inspectorRoot = UI_INVALID_HANDLE;
        UiHandle validationRoot = UI_INVALID_HANDLE;
        UiHandle statusLabel = UI_INVALID_HANDLE;
        UiHandle previewLabel = UI_INVALID_HANDLE;
        ToolButton openPromptButton;
        ToolButton editTextButton;
        ToolButton rebuildButton;
        ToolButton saveButton;
        std::vector<ToolButton> hierarchyRows;
        std::vector<UiHandle> inspectorLabels;
        std::vector<UiHandle> validationLabels;
        VisualUiEditorDocument document;
    };
}
