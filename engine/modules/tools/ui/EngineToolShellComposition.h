#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "EngineToolPanes.h"
#include "EngineToolUiHelpers.h"
#include "ToolTheme.h"
#include "UiComposition.h"
#include "UiWidget.h"

namespace gts::tools
{
    class EngineToolShellComposition : public UiComposition
    {
    public:
        explicit EngineToolShellComposition(BitmapFont* inFont)
            : font(inFont)
        {
            panes.emplace_back(std::make_unique<MenuBarPane>());
            panes.emplace_back(std::make_unique<WorkspaceTabsPane>());
            panes.emplace_back(std::make_unique<ToolToolbarPane>());
            panes.emplace_back(std::make_unique<WorldViewportPane>());
            panes.emplace_back(std::make_unique<SceneBrowserPane>());
            panes.emplace_back(std::make_unique<EffectHierarchyPane>());
            panes.emplace_back(std::make_unique<EmitterDetailsPane>());
            panes.emplace_back(std::make_unique<ParticlePreviewViewportPane>());
            panes.emplace_back(std::make_unique<PropertyInspectorPane>());
            panes.emplace_back(std::make_unique<CurveTimelinePane>());
            panes.emplace_back(std::make_unique<DiagnosticsPane>());
            panes.emplace_back(std::make_unique<StatusBarPane>());
        }

        void setView(ToolShellView inView)
        {
            view = std::move(inView);
        }

        std::vector<ToolCommand> consumeCommands()
        {
            return commands.consume();
        }

        UiHandle particlePreviewImageHandle() const
        {
            for (const auto& pane : panes)
            {
                if (pane->descriptor().id == ToolPaneId::ParticlePreviewViewport)
                    return pane->previewImageHandle();
            }
            return UI_INVALID_HANDLE;
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setLayout(context.surface, context.root, gts::ui::fillLayout());
            context.ui.setState(context.surface,
                                context.root,
                                toolui::visibleState(true, true, false));

            buildChrome(widgetContext, context.root);
            for (auto& pane : panes)
                pane->build(widgetContext, context.root, font, layoutFor(pane->descriptor()));
        }

        void update(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setState(context.surface,
                                context.root,
                                toolui::visibleState(true, true, false));

            syncChrome(widgetContext);
            for (auto& pane : panes)
            {
                const PaneDescriptor& descriptor = pane->descriptor();
                const bool visible = paneVisibleInWorkspace(descriptor, view.activeWorkspace);
                pane->update(widgetContext, view, layoutFor(descriptor), visible);
            }
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            for (auto& pane : panes)
            {
                const PaneDescriptor& descriptor = pane->descriptor();
                if (paneVisibleInWorkspace(descriptor, view.activeWorkspace))
                    pane->onEvent(widgetContext, event, commands, view);
            }
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            for (auto& pane : panes)
                pane->destroy(widgetContext);

            statusChrome.destroy(widgetContext);
            toolbarChrome.destroy(widgetContext);
            topChrome.destroy(widgetContext);
        }

    private:
        BitmapFont* font = nullptr;
        ToolShellView view;
        ToolCommandQueue commands;
        std::vector<std::unique_ptr<ToolPane>> panes;

        gts::ui::UiPanelWidget topChrome;
        gts::ui::UiPanelWidget toolbarChrome;
        gts::ui::UiPanelWidget statusChrome;

        void buildChrome(gts::ui::UiWidgetContext& context, UiHandle parent)
        {
            buildChromePanel(topChrome,
                             context,
                             parent,
                             toolui::rect(0.0f, 0.0f, 1.0f, view.topBarHeight),
                             ToolTheme::barBackground);
            buildChromePanel(toolbarChrome,
                             context,
                             parent,
                             toolui::rect(0.0f, view.topBarHeight, 1.0f, view.viewportToolbarHeight),
                             ToolTheme::viewportBar);
            buildChromePanel(statusChrome,
                             context,
                             parent,
                             toolui::rect(0.0f, 1.0f - view.bottomBarHeight, 1.0f, view.bottomBarHeight),
                             ToolTheme::barBackground);
        }

        void syncChrome(gts::ui::UiWidgetContext& context)
        {
            context.ui.setLayout(context.surface,
                                 topChrome.root(),
                                 toolui::rect(0.0f, 0.0f, 1.0f, view.topBarHeight));
            context.ui.setLayout(context.surface,
                                 toolbarChrome.root(),
                                 toolui::rect(0.0f, view.topBarHeight, 1.0f, view.viewportToolbarHeight));
            context.ui.setLayout(context.surface,
                                 statusChrome.root(),
                                 toolui::rect(0.0f, 1.0f - view.bottomBarHeight, 1.0f, view.bottomBarHeight));
            topChrome.setVisible(context, true);
            toolbarChrome.setVisible(context, true);
            statusChrome.setVisible(context, true);
        }

        static void buildChromePanel(gts::ui::UiPanelWidget& panel,
                                     gts::ui::UiWidgetContext& context,
                                     UiHandle parent,
                                     const UiLayoutSpec& layout,
                                     UiColor color)
        {
            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = true;
            panel.build(context, parent, desc);
            toolui::setRectPayload(context.ui, context.surface, panel.root(), color);
        }

        UiLayoutSpec layoutFor(const PaneDescriptor& descriptor) const
        {
            const float leftWidth = std::clamp(view.leftPaneWidth, 0.0f, 0.45f);
            const float rightWidth = std::clamp(view.rightPaneWidth, 0.0f, 0.45f);
            const float rightX = std::max(0.0f, 1.0f - rightWidth);
            const float contentY = view.viewportY;
            const float statusTop = std::max(contentY, 1.0f - view.bottomBarHeight);
            const float leftHeight = std::max(0.0f, statusTop - contentY);
            const float rightHeight = leftHeight;
            const float hierarchyHeight = leftHeight * 0.660f;
            const float previewHeight = rightHeight * 0.340f;
            const float inspectorHeight = view.activeWorkspace == ToolWorkspace::Particles
                ? rightHeight * 0.420f
                : rightHeight * 0.640f;

            switch (descriptor.id)
            {
                case ToolPaneId::MenuBar:
                    return toolui::rect(0.0f, 0.0f, 1.0f, view.topBarHeight);

                case ToolPaneId::WorkspaceTabs:
                    return toolui::rect(0.185f, 0.0f, 0.360f, view.topBarHeight);

                case ToolPaneId::ToolToolbar:
                    return toolui::rect(0.0f,
                                        view.topBarHeight,
                                        1.0f,
                                        view.viewportToolbarHeight);

                case ToolPaneId::WorldViewport:
                    return toolui::rect(view.viewportX,
                                        view.viewportY,
                                        view.viewportWidth,
                                        view.viewportHeight);

                case ToolPaneId::SceneBrowser:
                    return toolui::rect(0.0f, contentY, leftWidth, leftHeight);

                case ToolPaneId::EffectHierarchy:
                    return toolui::rect(0.0f, contentY, leftWidth, hierarchyHeight);

                case ToolPaneId::EmitterDetails:
                    return toolui::rect(0.0f,
                                        contentY + hierarchyHeight,
                                        leftWidth,
                                        std::max(0.0f, leftHeight - hierarchyHeight));

                case ToolPaneId::ParticlePreviewViewport:
                    return toolui::rect(rightX, contentY, rightWidth, previewHeight);

                case ToolPaneId::PropertyInspector:
                {
                    const float inspectorY = view.activeWorkspace == ToolWorkspace::Particles
                        ? contentY + previewHeight
                        : contentY;
                    return toolui::rect(rightX, inspectorY, rightWidth, inspectorHeight);
                }

                case ToolPaneId::Diagnostics:
                {
                    const float diagnosticsY = view.activeWorkspace == ToolWorkspace::Particles
                        ? contentY + previewHeight + inspectorHeight
                        : contentY + inspectorHeight;
                    return toolui::rect(rightX,
                                        diagnosticsY,
                                        rightWidth,
                                        std::max(0.0f, statusTop - diagnosticsY));
                }

                case ToolPaneId::CurveTimeline:
                    return toolui::rect(view.viewportX,
                                        view.viewportY + view.viewportHeight,
                                        view.viewportWidth,
                                        view.bottomDockHeight);

                case ToolPaneId::StatusBar:
                    return toolui::rect(0.0f, 1.0f - view.bottomBarHeight, 1.0f, view.bottomBarHeight);
            }

            return gts::ui::fillLayout();
        }
    };
} // namespace gts::tools
