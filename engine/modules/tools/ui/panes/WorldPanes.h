#pragma once

#include <array>
#include <string>

#include "ToolPaneBase.h"
#include "ToolStringUtils.h"

namespace gts::tools
{
    class WorldViewportPane final : public ToolPaneBase
    {
    public:
        WorldViewportPane()
            : ToolPaneBase({ToolPaneId::WorldViewport, ToolDockArea::Center, "World Viewport", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            buildPanel(context,
                       topBar,
                       content(),
                       toolui::rect(0.000f, 0.000f, 1.000f, 0.054f),
                       ToolTheme::headerBackground,
                       true,
                       true);
            buildPanel(context,
                       bottomBar,
                       content(),
                       toolui::rect(0.000f, 0.944f, 1.000f, 0.056f),
                       ToolTheme::headerBackground,
                       true,
                       true);
            buildPanel(context,
                       evalChip,
                       content(),
                       toolui::rect(0.020f, 0.080f, 0.250f, 0.044f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            buildPanel(context,
                       debugChip,
                       content(),
                       toolui::rect(0.740f, 0.080f, 0.240f, 0.044f),
                       ToolTheme::viewportOverlay,
                       false,
                       true);
            title.build(context,
                        topBar.content(),
                        label("World Viewport",
                              toolui::rect(0.018f, 0.080f, 0.220f, 0.840f),
                              ToolTheme::text,
                              ToolTheme::smallTextScale));
            scene.build(context,
                        topBar.content(),
                        label("Scene",
                              toolui::rect(0.260f, 0.080f, 0.490f, 0.840f),
                              ToolTheme::mutedText,
                              ToolTheme::smallTextScale));
            mode.build(context,
                       topBar.content(),
                       label("Runtime",
                             toolui::rect(0.780f, 0.080f, 0.200f, 0.840f),
                             ToolTheme::statusText,
                             ToolTheme::smallTextScale,
                             UiHorizontalAlign::Right));
            viewMode.build(context,
                           bottomBar.content(),
                           label("Runtime view",
                                 toolui::rect(0.018f, 0.080f, 0.260f, 0.840f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale));
            cameraMode.build(context,
                             bottomBar.content(),
                             label("Scene camera",
                                   toolui::rect(0.760f, 0.080f, 0.220f, 0.840f),
                                   ToolTheme::mutedText,
                                   ToolTheme::smallTextScale,
                                   UiHorizontalAlign::Right));
            evalLabel.build(context,
                            evalChip.content(),
                            label("World Render Target",
                                  toolui::rect(0.055f, 0.080f, 0.890f, 0.840f),
                                  ToolTheme::statusText,
                                  ToolTheme::smallTextScale));
            debugLabel.build(context,
                             debugChip.content(),
                             label("Debug Draw Off",
                                   toolui::rect(0.055f, 0.080f, 0.890f, 0.840f),
                                   ToolTheme::statusText,
                                   ToolTheme::smallTextScale,
                                   UiHorizontalAlign::Right));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            scene.setText(context, view.activeSceneName.empty()
                                       ? std::string("No scene")
                                       : "Scene: " + toolui::compact(view.activeSceneName, 42));
            title.setVisible(context, visible);
            scene.setVisible(context, visible);
            mode.setVisible(context, visible);
            viewMode.setVisible(context, visible);
            cameraMode.setVisible(context, visible);
            topBar.setVisible(context, visible);
            bottomBar.setVisible(context, visible);
            evalChip.setVisible(context, visible);
            debugChip.setVisible(context, visible);
            evalLabel.setVisible(context, visible);
            debugLabel.setVisible(context, visible);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            scene.destroy(context);
            mode.destroy(context);
            viewMode.destroy(context);
            cameraMode.destroy(context);
            evalLabel.destroy(context);
            debugLabel.destroy(context);
            debugChip.destroy(context);
            evalChip.destroy(context);
            bottomBar.destroy(context);
            topBar.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget topBar;
        gts::ui::UiPanelWidget bottomBar;
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget scene;
        gts::ui::UiLabelWidget mode;
        gts::ui::UiLabelWidget viewMode;
        gts::ui::UiLabelWidget cameraMode;
        gts::ui::UiPanelWidget evalChip;
        gts::ui::UiPanelWidget debugChip;
        gts::ui::UiLabelWidget evalLabel;
        gts::ui::UiLabelWidget debugLabel;
    };

    class SceneBrowserPane final : public ToolPaneBase
    {
    public:
        SceneBrowserPane()
            : ToolPaneBase({ToolPaneId::SceneBrowser, ToolDockArea::Left, "Scenes", true, false, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolListSectionDesc desc;
            desc.title = "Scenes";
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
            scenes.build(context, content(), font, desc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            std::vector<ToolSelectableRowData> rows;
            rows.reserve(view.scenes.size());
            for (const ToolSceneRow& scene : view.scenes)
            {
                ToolSelectableRowData row;
                row.badge = "SC";
                row.primary = scene.label;
                row.enabled = !scene.active;
                row.selected = scene.active;
                rows.push_back(std::move(row));
            }
            const bool hasPrevious = visible && view.sceneOffset > 0;
            const bool hasNext = visible && view.sceneOffset + view.scenes.size() < view.sceneTotal;
            scenes.sync(context,
                        visible,
                        rows,
                        pageText(view.sceneOffset, view.scenes.size(), view.sceneTotal),
                        hasPrevious,
                        hasNext);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            scenes.onEvent(context, event);

            if (scenes.consumePreviousPressed())
                commands.push({ToolCommandType::ScenePagePrevious});
            if (scenes.consumeNextPressed())
                commands.push({ToolCommandType::ScenePageNext});

            size_t rowIndex = 0;
            if (scenes.consumeRowPressed(rowIndex) && rowIndex < view.scenes.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::LoadScene;
                command.value = view.scenes[rowIndex].id;
                commands.push(std::move(command));
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            scenes.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxRows = 10;

        ToolListSection<MaxRows> scenes;
    };

} // namespace gts::tools
