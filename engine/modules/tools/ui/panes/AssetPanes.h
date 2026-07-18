#pragma once

#include <array>
#include <string>

#include "ToolPaneBase.h"
#include "ToolStringUtils.h"

namespace gts::tools
{
    class AssetBrowserPane final : public ToolPaneBase
    {
    public:
        AssetBrowserPane()
            : ToolPaneBase({ToolPaneId::AssetBrowser, ToolDockArea::Left, "Assets", false, false, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolListSectionDesc desc;
            desc.title = "Assets";
            desc.headerLayout = toolui::rect(0.050f, 0.030f, 0.430f, 0.040f);
            desc.countLayout = toolui::rect(0.500f, 0.032f, 0.450f, 0.034f);
            desc.stackLayout = toolui::rect(0.050f, 0.092f, 0.900f, 0.500f);
            desc.previousLayout = toolui::rect(0.050f, 0.620f, 0.430f, 0.050f);
            desc.nextLayout = toolui::rect(0.520f, 0.620f, 0.430f, 0.050f);
            desc.rowHeight = 0.043f;
            desc.rowGap = 0.006f;
            desc.headerScale = ToolTheme::headerTextScale;
            desc.rowTextScale = ToolTheme::bodyTextScale;
            desc.showCount = true;
            desc.showPager = true;
            assets.build(context, content(), font, desc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            std::vector<ToolSelectableRowData> rows;
            rows.reserve(view.assets.size());
            for (const ToolAssetRow& asset : view.assets)
            {
                ToolSelectableRowData row;
                row.badge = asset.valid ? "AS" : "!";
                row.primary = asset.label;
                row.secondary = asset.summary;
                row.selected = asset.active;
                row.warning = !asset.valid;
                rows.push_back(std::move(row));
            }

            const bool hasPrevious = visible && view.assetOffset > 0;
            const bool hasNext = visible && view.assetOffset + view.assets.size() < view.assetTotal;
            assets.sync(context,
                        visible,
                        rows,
                        pageText(view.assetOffset, view.assets.size(), view.assetTotal),
                        hasPrevious,
                        hasNext);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            assets.onEvent(context, event);

            if (assets.consumePreviousPressed())
                commands.push({ToolCommandType::AssetPagePrevious});
            if (assets.consumeNextPressed())
                commands.push({ToolCommandType::AssetPageNext});

            size_t rowIndex = 0;
            if (assets.consumeRowPressed(rowIndex) && rowIndex < view.assets.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::SelectAsset;
                command.value = view.assets[rowIndex].manifestPath;
                commands.push(std::move(command));
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            assets.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxRows = 10;

        ToolListSection<MaxRows> assets;
    };

    class AssetPreviewPane final : public ToolPaneBase
    {
    public:
        AssetPreviewPane()
            : ToolPaneBase({ToolPaneId::AssetPreview, ToolDockArea::Center, "Asset Preview", false, false, 0})
        {
        }

        UiHandle assetPreviewImageHandle() const override { return previewImage.root(); }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::workspaceBackground, false);
            buildPanel(context,
                       headerPanel,
                       content(),
                       toolui::rect(0.024f, 0.026f, 0.952f, 0.090f),
                       ToolTheme::headerBackground,
                       false,
                       true);
            title.build(context,
                        headerPanel.content(),
                        label("No Asset",
                              toolui::rect(0.024f, 0.100f, 0.560f, 0.460f),
                              ToolTheme::text,
                              ToolTheme::headerTextScale));
            path.build(context,
                       headerPanel.content(),
                       label("resources/assets",
                             toolui::rect(0.024f, 0.550f, 0.720f, 0.360f),
                             ToolTheme::mutedText,
                             ToolTheme::smallTextScale));
            state.build(context,
                        headerPanel.content(),
                        label("Idle",
                              toolui::rect(0.730f, 0.160f, 0.240f, 0.640f),
                              ToolTheme::statusText,
                              ToolTheme::smallTextScale,
                              UiHorizontalAlign::Right));

            buildPanel(context,
                       previewPanel,
                       content(),
                       toolui::rect(0.024f, 0.132f, 0.952f, 0.742f),
                       ToolTheme::panelInset,
                       false,
                       true,
                       true);
            previewImage.build(context,
                               previewPanel.content(),
                               image(toolui::rect(0.018f, 0.025f, 0.964f, 0.950f)));
            previewTitle.build(context,
                               previewPanel.content(),
                               label("Asset Viewport",
                                     toolui::rect(0.060f, 0.070f, 0.420f, 0.100f),
                                     ToolTheme::text,
                                     ToolTheme::smallTextScale));
            material.build(context,
                           previewPanel.content(),
                           label("Material: --",
                                 toolui::rect(0.540f, 0.070f, 0.400f, 0.100f),
                                 ToolTheme::statusText,
                                 ToolTheme::smallTextScale,
                                 UiHorizontalAlign::Right));
            model.build(context,
                        previewPanel.content(),
                        label("Model: --",
                              toolui::rect(0.060f, 0.250f, 0.880f, 0.100f),
                              ToolTheme::mutedText,
                              ToolTheme::smallTextScale));
            texture.build(context,
                          previewPanel.content(),
                          label("Texture: --",
                                toolui::rect(0.060f, 0.390f, 0.880f, 0.100f),
                                ToolTheme::mutedText,
                                ToolTheme::smallTextScale));
            bounds.build(context,
                         previewPanel.content(),
                         label("Bounds: --",
                               toolui::rect(0.060f, 0.530f, 0.880f, 0.100f),
                               ToolTheme::mutedText,
                               ToolTheme::smallTextScale));
            source.build(context,
                         previewPanel.content(),
                         label("Source: --",
                               toolui::rect(0.060f, 0.670f, 0.880f, 0.100f),
                               ToolTheme::mutedText,
                               ToolTheme::smallTextScale));

            buildPanel(context,
                       footerPanel,
                       content(),
                       toolui::rect(0.024f, 0.895f, 0.952f, 0.055f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            footer.build(context,
                         footerPanel.content(),
                         label("Select an asset manifest",
                               toolui::rect(0.030f, 0.140f, 0.940f, 0.720f),
                               ToolTheme::statusText,
                               ToolTheme::smallTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);

            headerPanel.setVisible(context, visible);
            previewPanel.setVisible(context, visible);
            footerPanel.setVisible(context, visible);
            title.setVisible(context, visible);
            path.setVisible(context, visible);
            state.setVisible(context, visible);
            previewTitle.setVisible(context, visible);
            material.setVisible(context, visible);
            const bool hasPreview = visible && view.assetPreviewTexture != 0;
            previewImage.setVisible(context, hasPreview);
            model.setVisible(context, visible && !hasPreview);
            texture.setVisible(context, visible && !hasPreview);
            bounds.setVisible(context, visible && !hasPreview);
            source.setVisible(context, visible && !hasPreview);
            footer.setVisible(context, visible);

            UiImageData imageData;
            imageData.textureID = view.assetPreviewTexture;
            imageData.tint = {1.0f, 1.0f, 1.0f, hasPreview ? 1.0f : 0.0f};
            imageData.imageAspect = 1.0f;
            context.ui.setPayload(context.surface, previewImage.root(), imageData);

            title.setText(context, view.assetSelected ? view.assetTitle : "No Asset");
            path.setText(context, view.assetManifestPath.empty()
                                      ? std::string("resources/assets")
                                      : toolui::compact(view.assetManifestPath, 68));
            state.setText(context,
                          !view.assetSelected ? "Idle" : (view.assetSelectedValid ? "Valid" : "Invalid"));
            material.setText(context,
                             view.assetSelectedValid
                                 ? "Material: " + view.assetMaterialMode
                                 : "Material: --");
            model.setText(context,
                          view.assetModelPath.empty()
                              ? std::string("Model: --")
                              : "Model: " + toolui::compact(view.assetModelPath, 72));
            texture.setText(context,
                            view.assetTexturePath.empty()
                                ? std::string("Texture: --")
                                : "Texture: " + toolui::compact(view.assetTexturePath, 72));
            bounds.setText(context,
                           view.assetBounds.empty()
                               ? std::string("Bounds: --")
                               : "Bounds: " + view.assetBounds);
            source.setText(context,
                           view.assetSourcePath.empty()
                               ? std::string("Source: --")
                               : "Source: " + toolui::compact(view.assetSourcePath, 72));
            footer.setText(context,
                           !view.assetSelected ? "No manifest selected"
                           : view.assetSelectedValid ? (hasPreview ? "Rendered preview" : "Building preview")
                                                     : toolui::compact(view.assetError, 96));
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            footer.destroy(context);
            footerPanel.destroy(context);
            source.destroy(context);
            bounds.destroy(context);
            texture.destroy(context);
            model.destroy(context);
            material.destroy(context);
            previewTitle.destroy(context);
            previewImage.destroy(context);
            previewPanel.destroy(context);
            state.destroy(context);
            path.destroy(context);
            title.destroy(context);
            headerPanel.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget headerPanel;
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget path;
        gts::ui::UiLabelWidget state;
        gts::ui::UiPanelWidget previewPanel;
        gts::ui::UiImageWidget previewImage;
        gts::ui::UiLabelWidget previewTitle;
        gts::ui::UiLabelWidget material;
        gts::ui::UiLabelWidget model;
        gts::ui::UiLabelWidget texture;
        gts::ui::UiLabelWidget bounds;
        gts::ui::UiLabelWidget source;
        gts::ui::UiPanelWidget footerPanel;
        gts::ui::UiLabelWidget footer;
    };

} // namespace gts::tools
