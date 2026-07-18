#pragma once

#include <array>
#include <string>

#include "ToolPaneBase.h"
#include "ToolStringUtils.h"

namespace gts::tools
{
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
