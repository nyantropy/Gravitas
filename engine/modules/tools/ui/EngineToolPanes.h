#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "EngineToolUiHelpers.h"
#include "ToolPane.h"
#include "ToolPaneWidgets.h"
#include "ToolPropertyInspector.h"
#include "ToolTheme.h"

namespace gts::tools
{
    class ToolPaneBase : public ToolPane
    {
    public:
        const PaneDescriptor& descriptor() const override { return paneDescriptor; }
        UiHandle root() const override { return rootPanel.root(); }

    protected:
        explicit ToolPaneBase(PaneDescriptor descriptor)
            : paneDescriptor(descriptor)
        {
        }

        void buildRoot(gts::ui::UiWidgetContext& context,
                       UiHandle parent,
                       BitmapFont* inFont,
                       const UiLayoutSpec& layout,
                       UiColor color,
                       bool interactable)
        {
            font = inFont;

            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = interactable;
            rootPanel.build(context, parent, desc);
            if (color.a > 0.0f)
            {
                toolui::setPanelPayload(context.ui,
                                        context.surface,
                                        rootPanel.root(),
                                        color,
                                        ToolTheme::border,
                                        ToolTheme::panelBorderWidth,
                                        ToolTheme::shadow,
                                        {ToolTheme::panelShadowOffset, ToolTheme::panelShadowOffset});
            }
            else
            {
                toolui::setRectPayload(context.ui, context.surface, rootPanel.root(), color);
            }
        }

        void updateRoot(gts::ui::UiWidgetContext& context, const UiLayoutSpec& layout, bool visible)
        {
            if (rootPanel.root() == UI_INVALID_HANDLE)
                return;

            context.ui.setLayout(context.surface, rootPanel.root(), layout);
            rootPanel.setVisible(context, visible);
        }

        void destroyRoot(gts::ui::UiWidgetContext& context)
        {
            rootPanel.destroy(context);
        }

        UiHandle content() const { return rootPanel.content(); }

        gts::ui::UiLabelDesc label(const std::string& text,
                                   const UiLayoutSpec& layout,
                                   UiColor color,
                                   float scale,
                                   UiHorizontalAlign align = UiHorizontalAlign::Left,
                                   int maxLines = 2) const
        {
            return toolLabel(font, text, layout, color, scale, align, maxLines);
        }

        gts::ui::UiButtonDesc button(const std::string& text,
                                     const UiLayoutSpec& layout,
                                     float scale,
                                     UiHorizontalAlign align = UiHorizontalAlign::Center) const
        {
            return toolButton(font, text, layout, scale, align);
        }

        static gts::ui::UiImageDesc image(const UiLayoutSpec& layout)
        {
            gts::ui::UiImageDesc desc;
            desc.layout = layout;
            desc.tint = {1.0f, 1.0f, 1.0f, 0.0f};
            desc.visible = false;
            return desc;
        }

        static void buildPanel(gts::ui::UiWidgetContext& context,
                               gts::ui::UiPanelWidget& panel,
                               UiHandle parent,
                               const UiLayoutSpec& layout,
                               UiColor color,
                               bool interactable = false,
                               bool framed = false,
                               bool shadow = false)
        {
            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = interactable;
            panel.build(context, parent, desc);
            if (framed)
            {
                toolui::setPanelPayload(context.ui,
                                        context.surface,
                                        panel.root(),
                                        color,
                                        ToolTheme::borderSubtle,
                                        ToolTheme::panelBorderWidth,
                                        shadow ? ToolTheme::shadow : transparent(),
                                        shadow ? UiVec2{ToolTheme::panelShadowOffset, ToolTheme::panelShadowOffset}
                                               : UiVec2{});
            }
            else
            {
                toolui::setRectPayload(context.ui, context.surface, panel.root(), color);
            }
        }

        void syncButton(gts::ui::UiWidgetContext& context,
                        gts::ui::UiButtonWidget& widget,
                        bool visible,
                        const std::string& text,
                        bool enabled,
                        bool active)
        {
            syncToolButton(context, widget, visible, text, enabled, active);
        }

        static std::string pageText(size_t offset, size_t visible, size_t total)
        {
            return toolPageText(offset, visible, total);
        }

        static UiColor transparent()
        {
            return {0.0f, 0.0f, 0.0f, 0.0f};
        }

        PaneDescriptor paneDescriptor;
        BitmapFont* font = nullptr;
        gts::ui::UiPanelWidget rootPanel;
    };

    class MenuBarPane final : public ToolPaneBase
    {
    public:
        MenuBarPane()
            : ToolPaneBase({ToolPaneId::MenuBar, ToolDockArea::MenuBar, "Menu", true, true, 0, 1.0f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            title.build(context,
                        content(),
                        label("GRAVITAS",
                              toolui::rect(0.014f, 0.120f, 0.088f, 0.740f),
                              ToolTheme::text,
                              ToolTheme::titleTextScale));
            menuItems.build(context,
                            content(),
                            label("Toolchain",
                                  toolui::rect(0.112f, 0.160f, 0.190f, 0.660f),
                                  ToolTheme::mutedText,
                                  ToolTheme::smallTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView&,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            menuItems.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget menuItems;
    };

    class WorkspaceTabsPane final : public ToolPaneBase
    {
    public:
        WorkspaceTabsPane()
            : ToolPaneBase({ToolPaneId::WorkspaceTabs,
                            ToolDockArea::MenuBar,
                            "Workspaces",
                            true,
                            true,
                            1,
                            0.300f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.305f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);

            std::array<ToolToolbarButtonSlot, 3> slots;
            slots[0] = {"World", toolui::rect(0.000f, 0.160f, 0.300f, 0.680f)};
            slots[1] = {"Particles", toolui::rect(0.330f, 0.160f, 0.330f, 0.680f)};
            slots[2] = {"Assets", toolui::rect(0.690f, 0.160f, 0.310f, 0.680f)};
            tabs.build(context, content(), font, slots, ToolTheme::buttonTextScale);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            tabs.sync(context, 0, visible, "World", true, view.activeWorkspace == ToolWorkspace::World);
            tabs.sync(context, 1, visible, "Particles", true, view.activeWorkspace == ToolWorkspace::Particles);
            tabs.sync(context, 2, visible, "Assets", true, view.activeWorkspace == ToolWorkspace::Assets);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            tabs.onEvent(context, event);

            if (tabs.consumePressed(0))
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::World});
            if (tabs.consumePressed(1))
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::Particles});
            if (tabs.consumePressed(2))
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::Assets});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            tabs.destroy(context);
            destroyRoot(context);
        }

    private:
        ToolToolbarRow<3> tabs;
    };

    class ToolToolbarPane final : public ToolPaneBase
    {
    public:
        ToolToolbarPane()
            : ToolPaneBase({ToolPaneId::ToolToolbar, ToolDockArea::Toolbar, "Toolbar", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            buildPanel(context,
                       sceneGroup,
                       content(),
                       toolui::rect(0.012f, 0.120f, 0.318f, 0.760f),
                       ToolTheme::toolbarGroupBackground,
                       false,
                       true);
            viewportLabel.build(context,
                                sceneGroup.content(),
                                label("Active",
                                      toolui::rect(0.040f, 0.080f, 0.230f, 0.840f),
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale));
            sceneLabel.build(context,
                             sceneGroup.content(),
                             label("Scene",
                                   toolui::rect(0.295f, 0.080f, 0.665f, 0.840f),
                                   ToolTheme::text,
                                   ToolTheme::smallTextScale));
            buildPanel(context,
                       actionGroup,
                       content(),
                       toolui::rect(0.348f, 0.115f, 0.340f, 0.770f),
                       ToolTheme::toolbarGroupBackground,
                       false,
                       true);
            std::array<ToolToolbarButtonSlot, 4> slots;
            slots[0] = {"Save", toolui::rect(0.025f, 0.150f, 0.170f, 0.700f)};
            slots[1] = {"Reload", toolui::rect(0.215f, 0.150f, 0.210f, 0.700f)};
            slots[2] = {"Play", toolui::rect(0.445f, 0.150f, 0.170f, 0.700f)};
            slots[3] = {"Restart", toolui::rect(0.635f, 0.150f, 0.340f, 0.700f)};
            actions.build(context, actionGroup.content(), font, slots, ToolTheme::buttonTextScale);
            buildPanel(context,
                       workspaceGroup,
                       content(),
                       toolui::rect(0.748f, 0.120f, 0.238f, 0.760f),
                       ToolTheme::toolbarGroupBackground,
                       false,
                       true);
            workspaceLabel.build(context,
                                 workspaceGroup.content(),
                                 label("Workspace",
                                       toolui::rect(0.040f, 0.080f, 0.920f, 0.840f),
                                       ToolTheme::mutedText,
                                       ToolTheme::smallTextScale,
                                       UiHorizontalAlign::Right));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            const std::string activeScene = view.activeSceneName.empty() ? "none" : toolui::compact(view.activeSceneName, 48);
            sceneLabel.setText(context, activeScene);
            const bool particleWorkspace = view.activeWorkspace == ToolWorkspace::Particles;
            sceneGroup.setVisible(context, visible);
            actionGroup.setVisible(context, visible && particleWorkspace);
            workspaceGroup.setVisible(context, visible);
            viewportLabel.setVisible(context, visible);
            sceneLabel.setVisible(context, visible);
            actions.sync(context, 0, visible && particleWorkspace, "Save", view.particleLoaded, false);
            actions.sync(context, 1, visible && particleWorkspace, "Reload", view.particleLoaded, false);
            actions.sync(context, 2, visible && particleWorkspace, view.particlePlaying ? "Pause" : "Play", view.particleLoaded, false);
            actions.sync(context, 3, visible && particleWorkspace, "Restart", view.particleLoaded, false);
            workspaceLabel.setText(context, workspaceLabelText(view.activeWorkspace));
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            actions.onEvent(context, event);

            if (actions.consumePressed(0))
                commands.push({ToolCommandType::SaveParticleEffect});
            if (actions.consumePressed(1))
                commands.push({ToolCommandType::ReloadParticleEffect});
            if (actions.consumePressed(2))
                commands.push({ToolCommandType::ToggleParticlePlayback});
            if (actions.consumePressed(3))
                commands.push({ToolCommandType::RestartParticlePreview});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            viewportLabel.destroy(context);
            sceneLabel.destroy(context);
            actions.destroy(context);
            workspaceLabel.destroy(context);
            workspaceGroup.destroy(context);
            actionGroup.destroy(context);
            sceneGroup.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget sceneGroup;
        gts::ui::UiLabelWidget viewportLabel;
        gts::ui::UiLabelWidget sceneLabel;
        gts::ui::UiPanelWidget actionGroup;
        ToolToolbarRow<4> actions;
        gts::ui::UiPanelWidget workspaceGroup;
        gts::ui::UiLabelWidget workspaceLabel;

        static std::string workspaceLabelText(ToolWorkspace workspace)
        {
            switch (workspace)
            {
                case ToolWorkspace::World:
                    return "World";
                case ToolWorkspace::Particles:
                    return "Particles";
                case ToolWorkspace::Assets:
                    return "Assets";
            }
            return "Tools";
        }
    };

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
            buildRoot(context, parent, font, layout, ToolTheme::workspaceBackground, true);
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
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
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
                                    {ToolTheme::panelShadowOffset, ToolTheme::panelShadowOffset});

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

    class PropertyInspectorPane final : public ToolPaneBase
    {
    public:
        PropertyInspectorPane()
            : ToolPaneBase({ToolPaneId::PropertyInspector,
                            ToolDockArea::Right,
                            "Inspector",
                            true,
                            true,
                            1,
                            0.420f,
                            0.640f,
                            0.420f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
            inspector.build(context, content(), font, toolui::rect(0.050f, 0.050f, 0.900f, 0.900f));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            inspector.sync(context, visible, view.propertySections);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            inspector.onEvent(context, event, commands);
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            inspector.destroy(context);
            destroyRoot(context);
        }

    private:
        ToolPropertyInspector<5, 18> inspector;
    };

    class CurveTimelinePane final : public ToolPaneBase
    {
    public:
        CurveTimelinePane()
            : ToolPaneBase({ToolPaneId::CurveTimeline, ToolDockArea::Bottom, "Curves", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
            buildPanel(context,
                       curveListPanel,
                       content(),
                       toolui::rect(0.018f, 0.180f, 0.215f, 0.725f),
                       ToolTheme::panelInset);
            buildPanel(context,
                       timelinePanel,
                       content(),
                       toolui::rect(0.250f, 0.180f, 0.732f, 0.725f),
                       ToolTheme::panelInset);
            header.build(context,
                         content(),
                         label("Curves / Timeline",
                               toolui::rect(0.025f, 0.040f, 0.260f, 0.110f),
                               ToolTheme::text,
                               ToolTheme::smallTextScale));
            metadata.build(context,
                           content(),
                           label("No curve parameter selected",
                                 toolui::rect(0.305f, 0.040f, 0.440f, 0.110f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale));
            buildPanel(context,
                       tabStrip,
                       content(),
                       toolui::rect(0.765f, 0.036f, 0.210f, 0.120f),
                       ToolTheme::toolbarGroupBackground);
            curvesTab.build(context,
                            tabStrip.content(),
                            label("Curves",
                                  toolui::rect(0.040f, 0.080f, 0.300f, 0.840f),
                                  ToolTheme::text,
                                  ToolTheme::smallTextScale));
            timelineTab.build(context,
                              tabStrip.content(),
                              label("Timeline",
                                    toolui::rect(0.355f, 0.080f, 0.345f, 0.840f),
                                    ToolTheme::mutedText,
                                    ToolTheme::smallTextScale));
            graphTab.build(context,
                           tabStrip.content(),
                           label("Graph",
                                 toolui::rect(0.720f, 0.080f, 0.240f, 0.840f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale,
                                 UiHorizontalAlign::Right));
            const std::array<std::pair<const char*, UiColor>, 5> curves{{
                {"Spawn.Rate", ToolTheme::curveBlue},
                {"Lifetime", ToolTheme::curveGreen},
                {"Size", ToolTheme::curveYellow},
                {"Velocity", ToolTheme::curveViolet},
                {"Alpha", ToolTheme::mutedText}
            }};
            for (size_t i = 0; i < curveRows.size(); ++i)
            {
                const float y = 0.070f + static_cast<float>(i) * 0.160f;
                buildPanel(context,
                           curveRows[i],
                           curveListPanel.content(),
                           toolui::rect(0.045f, y, 0.090f, 0.060f),
                           curves[i].second);
                curveLabels[i].build(context,
                                     curveListPanel.content(),
                                     label(curves[i].first,
                                           toolui::rect(0.160f, y - 0.020f, 0.780f, 0.100f),
                                           ToolTheme::mutedText,
                                           ToolTheme::smallTextScale));
            }
            for (size_t i = 0; i < timeTicks.size(); ++i)
            {
                const float x = 0.070f + static_cast<float>(i) * 0.215f;
                buildPanel(context,
                           timeTicks[i],
                           timelinePanel.content(),
                           toolui::rect(x, 0.130f, 0.0025f, 0.760f),
                           ToolTheme::borderSubtle);
            }
            const std::array<UiColor, 4> lineColors{
                ToolTheme::curveBlue,
                ToolTheme::curveGreen,
                ToolTheme::curveYellow,
                ToolTheme::curveViolet
            };
            for (size_t i = 0; i < trackLines.size(); ++i)
            {
                const float y = 0.300f + static_cast<float>(i) * 0.130f;
                buildPanel(context,
                           trackLines[i],
                           timelinePanel.content(),
                           toolui::rect(0.070f, y, 0.865f, 0.010f),
                           lineColors[i]);
            }
            buildPanel(context,
                       playhead,
                       timelinePanel.content(),
                       toolui::rect(0.560f, 0.090f, 0.004f, 0.820f),
                       ToolTheme::accent);
            buildPanel(context,
                       playheadBadge,
                       timelinePanel.content(),
                       toolui::rect(0.505f, 0.870f, 0.118f, 0.075f),
                       ToolTheme::viewportOverlay);
            playheadLabel.build(context,
                                playheadBadge.content(),
                                label("2.3s",
                                      toolui::rect(0.060f, 0.080f, 0.880f, 0.840f),
                                      ToolTheme::statusText,
                                      ToolTheme::smallTextScale,
                                      UiHorizontalAlign::Center));
            lane.build(context,
                       timelinePanel.content(),
                       label("0s        1s        2s        3s        4s",
                             toolui::rect(0.045f, 0.030f, 0.900f, 0.090f),
                             ToolTheme::mutedText,
                             ToolTheme::smallTextScale,
                             UiHorizontalAlign::Right));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            curveListPanel.setVisible(context, visible);
            timelinePanel.setVisible(context, visible);
            tabStrip.setVisible(context, visible);
            curvesTab.setVisible(context, visible);
            timelineTab.setVisible(context, visible);
            graphTab.setVisible(context, visible);
            for (auto& row : curveRows)
                row.setVisible(context, visible);
            for (auto& label : curveLabels)
                label.setVisible(context, visible);
            for (auto& tick : timeTicks)
                tick.setVisible(context, visible);
            for (auto& line : trackLines)
                line.setVisible(context, visible);
            playhead.setVisible(context, visible);
            playheadBadge.setVisible(context, visible);
            playheadLabel.setVisible(context, visible);
            header.setVisible(context, visible);
            metadata.setVisible(context, visible);
            lane.setVisible(context, visible);
            metadata.setText(context,
                             view.activeWorkspace == ToolWorkspace::Particles
                                 ? "Focused particle curves will appear here"
                                 : "World timeline placeholder");
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            header.destroy(context);
            metadata.destroy(context);
            lane.destroy(context);
            playheadLabel.destroy(context);
            playheadBadge.destroy(context);
            graphTab.destroy(context);
            timelineTab.destroy(context);
            curvesTab.destroy(context);
            tabStrip.destroy(context);
            for (auto& line : trackLines)
                line.destroy(context);
            for (auto& tick : timeTicks)
                tick.destroy(context);
            for (auto& label : curveLabels)
                label.destroy(context);
            for (auto& row : curveRows)
                row.destroy(context);
            playhead.destroy(context);
            timelinePanel.destroy(context);
            curveListPanel.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget curveListPanel;
        gts::ui::UiPanelWidget timelinePanel;
        gts::ui::UiLabelWidget header;
        gts::ui::UiLabelWidget metadata;
        gts::ui::UiLabelWidget lane;
        gts::ui::UiPanelWidget tabStrip;
        gts::ui::UiLabelWidget curvesTab;
        gts::ui::UiLabelWidget timelineTab;
        gts::ui::UiLabelWidget graphTab;
        std::array<gts::ui::UiPanelWidget, 5> curveRows;
        std::array<gts::ui::UiLabelWidget, 5> curveLabels;
        std::array<gts::ui::UiPanelWidget, 5> timeTicks;
        std::array<gts::ui::UiPanelWidget, 4> trackLines;
        gts::ui::UiPanelWidget playhead;
        gts::ui::UiPanelWidget playheadBadge;
        gts::ui::UiLabelWidget playheadLabel;
    };

    class DiagnosticsPane final : public ToolPaneBase
    {
    public:
        DiagnosticsPane()
            : ToolPaneBase({ToolPaneId::Diagnostics, ToolDockArea::Right, "Diagnostics", true, true, 2})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolInspectorSectionDesc desc;
            desc.title = "Diagnostics";
            desc.headerLayout = toolui::rect(0.050f, 0.065f, 0.900f, 0.100f);
            desc.stackLayout = toolui::rect(0.050f, 0.210f, 0.900f, 0.700f);
            desc.lineHeight = 0.105f;
            desc.lineGap = 0.020f;
            desc.titleScale = ToolTheme::smallTextScale;
            diagnostics.build(context, content(), font, desc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            diagnostics.sync(context, visible, "Diagnostics", view.diagnostics);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            diagnostics.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxLines = 5;

        ToolInspectorSection<MaxLines> diagnostics;
    };

    class StatusBarPane final : public ToolPaneBase
    {
    public:
        StatusBarPane()
            : ToolPaneBase({ToolPaneId::StatusBar, ToolDockArea::StatusBar, "Status", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            status.build(context,
                         content(),
                         label("F6 TO HIDE",
                               toolui::rect(0.014f, 0.120f, 0.960f, 0.760f),
                               ToolTheme::statusText,
                               ToolTheme::smallTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            status.setText(context, view.status.empty() ? "F6 TO HIDE" : view.status);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            status.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget status;
    };
} // namespace gts::tools
