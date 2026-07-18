#pragma once

#include <array>
#include <string>

#include "ToolPaneBase.h"
#include "ToolStringUtils.h"

namespace gts::tools
{
    class EffectHierarchyPane final : public ToolPaneBase
    {
    public:
        EffectHierarchyPane()
            : ToolPaneBase({ToolPaneId::EffectHierarchy,
                            ToolDockArea::Left,
                            "Effect Hierarchy",
                            false,
                            true,
                            0,
                            0.760f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolListSectionDesc effectDesc;
            effectDesc.title = "Particle Assets";
            effectDesc.headerLayout = toolui::rect(0.050f, 0.024f, 0.560f, 0.040f);
            effectDesc.countLayout = toolui::rect(0.620f, 0.028f, 0.330f, 0.034f);
            effectDesc.stackLayout = toolui::rect(0.050f, 0.086f, 0.900f, 0.290f);
            effectDesc.previousLayout = toolui::rect(0.050f, 0.395f, 0.430f, 0.044f);
            effectDesc.nextLayout = toolui::rect(0.520f, 0.395f, 0.430f, 0.044f);
            effectDesc.rowHeight = 0.038f;
            effectDesc.rowGap = 0.005f;
            effectDesc.headerScale = ToolTheme::headerTextScale;
            effectDesc.rowTextScale = ToolTheme::bodyTextScale;
            effectDesc.showCount = true;
            effectDesc.showPager = true;
            effects.build(context, content(), font, effectDesc);

            ToolListSectionDesc emitterDesc;
            emitterDesc.title = "Emitters";
            emitterDesc.headerLayout = toolui::rect(0.050f, 0.485f, 0.900f, 0.034f);
            emitterDesc.stackLayout = toolui::rect(0.050f, 0.530f, 0.900f, 0.220f);
            emitterDesc.rowHeight = 0.036f;
            emitterDesc.rowGap = 0.005f;
            emitterDesc.headerScale = ToolTheme::smallTextScale;
            emitterDesc.rowTextScale = ToolTheme::bodyTextScale;
            emitters.build(context, content(), font, emitterDesc);

            ToolListSectionDesc moduleDesc;
            moduleDesc.title = "Modules";
            moduleDesc.headerLayout = toolui::rect(0.050f, 0.780f, 0.900f, 0.034f);
            moduleDesc.stackLayout = toolui::rect(0.050f, 0.820f, 0.900f, 0.170f);
            moduleDesc.rowHeight = 0.033f;
            moduleDesc.rowGap = 0.004f;
            moduleDesc.headerScale = ToolTheme::smallTextScale;
            moduleDesc.rowTextScale = ToolTheme::bodyTextScale;
            modules.build(context, content(), font, moduleDesc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            std::vector<ToolSelectableRowData> effectRows;
            effectRows.reserve(view.effects.size());
            for (const ToolEffectRow& effect : view.effects)
            {
                ToolSelectableRowData row;
                row.badge = "FX";
                row.primary = effect.label;
                row.selected = effect.active;
                row.hasChildren = true;
                row.expanded = effect.active;
                effectRows.push_back(std::move(row));
            }

            const bool hasPrevious = visible && view.effectOffset > 0;
            const bool hasNext = visible && view.effectOffset + view.effects.size() < view.effectTotal;
            effects.sync(context,
                         visible,
                         effectRows,
                         pageText(view.effectOffset, view.effects.size(), view.effectTotal),
                         hasPrevious,
                         hasNext);

            std::vector<ToolSelectableRowData> emitterRows;
            emitterRows.reserve(view.emitters.size());
            for (const ToolEmitterRow& emitter : view.emitters)
            {
                ToolSelectableRowData row;
                row.badge = "E";
                row.primary = emitter.label;
                row.secondary = emitter.summary;
                row.depth = 1;
                row.selected = emitter.active;
                row.warning = !emitter.enabled;
                row.hasChildren = true;
                row.expanded = emitter.active;
                emitterRows.push_back(std::move(row));
            }
            emitters.sync(context, visible, emitterRows);

            std::vector<ToolSelectableRowData> moduleRows;
            moduleRows.reserve(view.modules.size());
            for (const ToolModuleRow& module : view.modules)
            {
                ToolSelectableRowData row;
                row.badge = "M";
                row.primary = module.label;
                row.secondary = module.summary;
                row.depth = 2;
                row.selected = module.active;
                row.warning = !module.enabled;
                moduleRows.push_back(std::move(row));
            }
            modules.sync(context, visible, moduleRows);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            effects.onEvent(context, event);
            emitters.onEvent(context, event);
            modules.onEvent(context, event);

            if (effects.consumePreviousPressed())
                commands.push({ToolCommandType::EffectPagePrevious});
            if (effects.consumeNextPressed())
                commands.push({ToolCommandType::EffectPageNext});

            size_t rowIndex = 0;
            if (effects.consumeRowPressed(rowIndex) && rowIndex < view.effects.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::OpenParticleEffect;
                command.value = view.effects[rowIndex].path;
                commands.push(std::move(command));
            }

            if (emitters.consumeRowPressed(rowIndex) && rowIndex < view.emitters.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::SelectEmitter;
                command.index = view.emitters[rowIndex].index;
                commands.push(command);
            }

            if (modules.consumeRowPressed(rowIndex) && rowIndex < view.modules.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::SelectModule;
                command.index = view.modules[rowIndex].index;
                commands.push(command);
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            effects.destroy(context);
            emitters.destroy(context);
            modules.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxEffectRows = 6;
        static constexpr size_t MaxEmitterRows = 5;
        static constexpr size_t MaxModuleRows = 4;

        ToolListSection<MaxEffectRows> effects;
        ToolListSection<MaxEmitterRows> emitters;
        ToolListSection<MaxModuleRows> modules;
    };

    class EmitterDetailsPane final : public ToolPaneBase
    {
    public:
        EmitterDetailsPane()
            : ToolPaneBase({ToolPaneId::EmitterDetails,
                            ToolDockArea::Left,
                            "Emitter Details",
                            false,
                            true,
                            1,
                            0.240f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolInspectorSectionDesc detailsDesc;
            detailsDesc.title = "Emitter Details";
            detailsDesc.headerLayout = toolui::rect(0.050f, 0.060f, 0.900f, 0.115f);
            detailsDesc.stackLayout = toolui::rect(0.050f, 0.210f, 0.900f, 0.245f);
            detailsDesc.lineHeight = ToolTheme::compactRowHeight;
            detailsDesc.lineGap = DefaultEditorTheme.spacing.xs;
            details.build(context, content(), font, detailsDesc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);

            const bool hasSelection = hasSelectedEmitter(view) || hasSelectedModule(view);
            details.sync(context,
                         visible && hasSelection,
                         "Emitter Details",
                         {selectedEmitterText(view), selectedModuleText(view)});
        }

        void onEvent(gts::ui::UiWidgetContext&,
                     UiEvent&,
                     ToolCommandQueue&,
                     const ToolShellView&) override
        {
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            details.destroy(context);
            destroyRoot(context);
        }

    private:
        static std::string selectedEmitterText(const ToolShellView& view)
        {
            const auto it = std::find_if(view.emitters.begin(),
                                         view.emitters.end(),
                                         [](const ToolEmitterRow& row)
                                         {
                                             return row.active;
                                         });
            return it == view.emitters.end() ? "No emitter selected" : "Emitter: " + it->label;
        }

        static std::string selectedModuleText(const ToolShellView& view)
        {
            const auto it = std::find_if(view.modules.begin(),
                                         view.modules.end(),
                                         [](const ToolModuleRow& row)
                                         {
                                             return row.active;
                                         });
            return it == view.modules.end() ? "No module selected" : "Module: " + it->label;
        }

        static bool hasSelectedEmitter(const ToolShellView& view)
        {
            return std::any_of(view.emitters.begin(),
                               view.emitters.end(),
                               [](const ToolEmitterRow& row)
                               {
                                   return row.active;
                               });
        }

        static bool hasSelectedModule(const ToolShellView& view)
        {
            return std::any_of(view.modules.begin(),
                               view.modules.end(),
                               [](const ToolModuleRow& row)
                               {
                                   return row.active;
                               });
        }

        ToolInspectorSection<2> details;
    };

    class ParticlePreviewViewportPane final : public ToolPaneBase
    {
    public:
        ParticlePreviewViewportPane()
            : ToolPaneBase({ToolPaneId::ParticlePreviewViewport,
                            ToolDockArea::Right,
                            "Particle Preview",
                            false,
                            true,
                            0,
                            0.340f})
        {
        }

        UiHandle previewImageHandle() const override { return previewImage.root(); }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, false);
            buildPanel(context,
                       headerPanel,
                       content(),
                       toolui::rect(0.035f, 0.035f, 0.930f, 0.145f),
                       ToolTheme::headerBackground,
                       false,
                       true);
            title.build(context,
                        headerPanel.content(),
                        label("No Effect",
                              toolui::rect(0.024f, 0.060f, 0.920f, 0.450f),
                              ToolTheme::text,
                              ToolTheme::headerTextScale));
            path.build(context,
                       headerPanel.content(),
                       label("Select a particle asset",
                             toolui::rect(0.024f, 0.500f, 0.920f, 0.420f),
                             ToolTheme::mutedText,
                             ToolTheme::smallTextScale));

            gts::ui::UiPanelDesc previewDesc;
            previewDesc.layout = toolui::rect(0.050f, 0.205f, 0.900f, 0.480f);
            previewDesc.styleClass.clear();
            previewDesc.enabled = true;
            previewDesc.interactable = false;
            previewPanel.build(context, content(), previewDesc);
            toolui::setPanelPayload(context.ui,
                                    context.surface,
                                    previewPanel.root(),
                                    ToolTheme::panelInset,
                                    ToolTheme::borderSubtle,
                                    ToolTheme::panelBorderWidth,
                                    ToolTheme::shadow,
                                    {0.0f, ToolTheme::panelShadowOffset},
                                    ToolTheme::panelRadius);

            previewImage.build(context, previewPanel.content(), image(toolui::rect(0.018f, 0.025f, 0.964f, 0.950f)));
            for (size_t i = 0; i < previewGridVertical.size(); ++i)
            {
                const float x = 0.140f + static_cast<float>(i) * 0.120f;
                buildPanel(context,
                           previewGridVertical[i],
                           previewPanel.content(),
                           toolui::rect(x, 0.455f, 0.0011f, 0.365f),
                           previewGridColor());
            }
            for (size_t i = 0; i < previewGridHorizontal.size(); ++i)
            {
                const float y = 0.510f + static_cast<float>(i) * 0.060f;
                buildPanel(context,
                           previewGridHorizontal[i],
                           previewPanel.content(),
                           toolui::rect(0.085f, y, 0.830f, 0.0014f),
                           previewGridColor());
            }
            buildPanel(context,
                       originHorizontal,
                       previewPanel.content(),
                       toolui::rect(0.450f, 0.505f, 0.100f, 0.0020f),
                       previewOriginColor());
            buildPanel(context,
                       originVertical,
                       previewPanel.content(),
                       toolui::rect(0.499f, 0.455f, 0.0020f, 0.100f),
                       previewOriginColor());
            buildPanel(context,
                       previewTopBar,
                       previewPanel.content(),
                       toolui::rect(0.018f, 0.025f, 0.964f, 0.072f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            previewViewportTitle.build(context,
                                       previewTopBar.content(),
                                       label("Particle Viewport",
                                             toolui::rect(0.025f, 0.120f, 0.300f, 0.760f),
                                             ToolTheme::text,
                                             ToolTheme::smallTextScale));
            previewViewportContext.build(context,
                                         previewTopBar.content(),
                                         label("Effect",
                                               toolui::rect(0.340f, 0.120f, 0.635f, 0.760f),
                                               ToolTheme::mutedText,
                                               ToolTheme::smallTextScale,
                                               UiHorizontalAlign::Right));
            buildPanel(context,
                       previewStatsChip,
                       previewPanel.content(),
                       toolui::rect(0.720f, 0.120f, 0.245f, 0.072f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            previewStats.build(context,
                               previewStatsChip.content(),
                               label("Particles 0 / 0",
                                     toolui::rect(0.050f, 0.120f, 0.900f, 0.760f),
                                     ToolTheme::statusText,
                                     ToolTheme::smallTextScale,
                                     UiHorizontalAlign::Right));
            buildPanel(context,
                       previewBottomBar,
                       previewPanel.content(),
                       toolui::rect(0.018f, 0.875f, 0.964f, 0.082f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            previewEmitter.build(context,
                                 previewBottomBar.content(),
                                 label("Emitter",
                                       toolui::rect(0.025f, 0.110f, 0.540f, 0.780f),
                                       ToolTheme::statusText,
                                       ToolTheme::smallTextScale));
            previewCamera.build(context,
                                previewBottomBar.content(),
                                label("Orbit Camera",
                                      toolui::rect(0.600f, 0.110f, 0.375f, 0.780f),
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale,
                                      UiHorizontalAlign::Right));
            buildPanel(context,
                       previewPlaybackChip,
                       previewPanel.content(),
                       toolui::rect(0.035f, 0.120f, 0.250f, 0.072f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            previewPlayback.build(context,
                                  previewPlaybackChip.content(),
                                  label("Preview idle",
                                        toolui::rect(0.050f, 0.120f, 0.900f, 0.760f),
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale));
            placeholder.build(context,
                              previewPanel.content(),
                              label("No preview",
                                    toolui::rect(0.050f, 0.420f, 0.900f, 0.160f),
                                    ToolTheme::mutedText,
                                    ToolTheme::smallTextScale,
                                    UiHorizontalAlign::Center));

            buildPanel(context,
                       statusStrip,
                       content(),
                       toolui::rect(0.050f, 0.705f, 0.900f, 0.060f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            previewStatus.build(context,
                                statusStrip.content(),
                                label("Preview idle",
                                      toolui::rect(0.035f, 0.100f, 0.520f, 0.800f),
                                      ToolTheme::statusText,
                                      ToolTheme::smallTextScale));
            previewTarget.build(context,
                                statusStrip.content(),
                                label("Separate render target",
                                      toolui::rect(0.515f, 0.100f, 0.450f, 0.800f),
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale,
                                      UiHorizontalAlign::Right));

            buildPanel(context,
                       actionBar,
                       content(),
                       toolui::rect(0.050f, 0.790f, 0.900f, 0.105f),
                       ToolTheme::headerBackground,
                       false,
                       true);
            std::array<ToolToolbarButtonSlot, 4> slots;
            slots[0] = {"Save", toolui::rect(0.015f, 0.140f, 0.220f, 0.720f)};
            slots[1] = {"Reload", toolui::rect(0.255f, 0.140f, 0.220f, 0.720f)};
            slots[2] = {"Pause", toolui::rect(0.495f, 0.140f, 0.220f, 0.720f)};
            slots[3] = {"Restart", toolui::rect(0.735f, 0.140f, 0.250f, 0.720f)};
            actions.build(context, actionBar.content(), font, slots, ToolTheme::buttonTextScale);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            const bool dominantPreview = view.activeWorkspace == ToolWorkspace::Particles;
            context.ui.setLayout(context.surface,
                                 headerPanel.root(),
                                 dominantPreview
                                     ? toolui::rect(0.024f, 0.026f, 0.952f, 0.090f)
                                     : toolui::rect(0.035f, 0.035f, 0.930f, 0.145f));
            context.ui.setLayout(context.surface,
                                 previewPanel.root(),
                                 dominantPreview
                                     ? toolui::rect(0.024f, 0.132f, 0.952f, 0.742f)
                                     : toolui::rect(0.050f, 0.205f, 0.900f, 0.480f));
            context.ui.setLayout(context.surface,
                                 statusStrip.root(),
                                 dominantPreview
                                     ? toolui::rect(0.024f, 0.895f, 0.952f, 0.055f)
                                     : toolui::rect(0.050f, 0.705f, 0.900f, 0.060f));
            context.ui.setLayout(context.surface,
                                 actionBar.root(),
                                 toolui::rect(0.000f, 0.000f, 0.000f, 0.000f));
            context.ui.setLayout(context.surface,
                                 previewImage.root(),
                                 dominantPreview
                                     ? toolui::rect(0.012f, 0.018f, 0.976f, 0.964f)
                                     : toolui::rect(0.018f, 0.025f, 0.964f, 0.950f));

            title.setText(context, view.particleTitle + (view.particleDirty ? " *" : ""));
            path.setText(context,
                         view.particlePath.empty()
                             ? std::string("Select a particle asset")
                             : toolui::compact(view.particlePath, 62));

            const bool hasPreview = visible && view.previewTexture != 0;
            previewImage.setVisible(context, hasPreview);
            placeholder.setVisible(context, visible && !hasPreview);
            UiImageData imageData;
            imageData.textureID = view.previewTexture;
            imageData.tint = {1.0f, 1.0f, 1.0f, hasPreview ? 1.0f : 0.0f};
            imageData.imageAspect = 1.0f;
            context.ui.setPayload(context.surface, previewImage.root(), imageData);

            headerPanel.setVisible(context, visible);
            previewPanel.setVisible(context, visible);
            statusStrip.setVisible(context, visible);
            previewStatus.setVisible(context, visible);
            previewTarget.setVisible(context, visible);
            actionBar.setVisible(context, false);
            const bool overlayVisible = visible && dominantPreview && hasPreview;
            for (auto& line : previewGridVertical)
                line.setVisible(context, overlayVisible);
            for (auto& line : previewGridHorizontal)
                line.setVisible(context, overlayVisible);
            originHorizontal.setVisible(context, overlayVisible);
            originVertical.setVisible(context, overlayVisible);
            previewTopBar.setVisible(context, overlayVisible);
            previewBottomBar.setVisible(context, overlayVisible);
            previewStatsChip.setVisible(context, overlayVisible);
            previewPlaybackChip.setVisible(context, overlayVisible);
            previewViewportTitle.setVisible(context, overlayVisible);
            previewViewportContext.setVisible(context, overlayVisible);
            previewStats.setVisible(context, overlayVisible);
            previewEmitter.setVisible(context, overlayVisible);
            previewCamera.setVisible(context, overlayVisible);
            previewPlayback.setVisible(context, overlayVisible);

            previewViewportContext.setText(context, toolui::compact(view.particleTitle, 38));
            previewStats.setText(context,
                                 "Particles " + std::to_string(view.previewRenderedParticles) +
                                     " / " + std::to_string(view.previewMaxParticles));
            previewEmitter.setText(context,
                                   "Emitter: " + toolui::compact(view.previewEmitterName, 24) +
                                       "  Module: " + toolui::compact(view.previewModuleName, 20));
            previewCamera.setText(context,
                                  "Orbit camera  |  " +
                                      std::to_string(view.previewEmitterCount) + " emitters  |  " +
                                      std::to_string(view.previewModuleCount) + " modules");
            previewPlayback.setText(context,
                                    std::string(view.particlePlaying ? "Playing" : "Paused") +
                                        "  " + toolui::fixed(view.previewTimeScale, 2) + "x" +
                                        (view.previewLooping ? "  Loop" : ""));
            previewStatus.setText(context,
                                  view.particleLoaded
                                      ? (view.particlePlaying ? "Preview playing" : "Preview paused")
                                      : "Preview idle");
            previewTarget.setText(context,
                                  std::to_string(view.previewEmitterCount) + " emitters  |  " +
                                      std::to_string(view.previewModuleCount) + " modules");

            actions.sync(context, 0, false, "Save", false, false);
            actions.sync(context, 1, false, "Reload", false, false);
            actions.sync(context, 2, false, view.particlePlaying ? "Pause" : "Play", false, false);
            actions.sync(context, 3, false, "Restart", false, false);
        }

        void onEvent(gts::ui::UiWidgetContext&,
                     UiEvent&,
                     ToolCommandQueue&,
                     const ToolShellView&) override
        {
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            path.destroy(context);
            placeholder.destroy(context);
            previewPlayback.destroy(context);
            previewCamera.destroy(context);
            previewEmitter.destroy(context);
            previewStats.destroy(context);
            previewViewportContext.destroy(context);
            previewViewportTitle.destroy(context);
            previewPlaybackChip.destroy(context);
            previewStatsChip.destroy(context);
            previewBottomBar.destroy(context);
            previewTopBar.destroy(context);
            originVertical.destroy(context);
            originHorizontal.destroy(context);
            for (auto& line : previewGridHorizontal)
                line.destroy(context);
            for (auto& line : previewGridVertical)
                line.destroy(context);
            previewImage.destroy(context);
            previewPanel.destroy(context);
            previewTarget.destroy(context);
            previewStatus.destroy(context);
            statusStrip.destroy(context);
            actions.destroy(context);
            actionBar.destroy(context);
            headerPanel.destroy(context);
            destroyRoot(context);
        }

    private:
        static UiColor previewGridColor()
        {
            return {0.120f, 0.170f, 0.210f, 0.155f};
        }

        static UiColor previewOriginColor()
        {
            return {0.210f, 0.550f, 0.780f, 0.340f};
        }

        gts::ui::UiPanelWidget headerPanel;
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget path;
        gts::ui::UiPanelWidget previewPanel;
        gts::ui::UiImageWidget previewImage;
        std::array<gts::ui::UiPanelWidget, 7> previewGridVertical;
        std::array<gts::ui::UiPanelWidget, 6> previewGridHorizontal;
        gts::ui::UiPanelWidget originHorizontal;
        gts::ui::UiPanelWidget originVertical;
        gts::ui::UiPanelWidget previewTopBar;
        gts::ui::UiPanelWidget previewBottomBar;
        gts::ui::UiPanelWidget previewStatsChip;
        gts::ui::UiPanelWidget previewPlaybackChip;
        gts::ui::UiLabelWidget previewViewportTitle;
        gts::ui::UiLabelWidget previewViewportContext;
        gts::ui::UiLabelWidget previewStats;
        gts::ui::UiLabelWidget previewEmitter;
        gts::ui::UiLabelWidget previewCamera;
        gts::ui::UiLabelWidget previewPlayback;
        gts::ui::UiLabelWidget placeholder;
        gts::ui::UiPanelWidget statusStrip;
        gts::ui::UiLabelWidget previewStatus;
        gts::ui::UiLabelWidget previewTarget;
        gts::ui::UiPanelWidget actionBar;
        ToolToolbarRow<4> actions;
    };

} // namespace gts::tools
