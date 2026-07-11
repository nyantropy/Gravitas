#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "ToolDockLayout.h"
#include "EngineToolPanes.h"
#include "EngineToolUiHelpers.h"
#include "ToolTheme.h"
#include "ToolWorkspaceLayout.h"
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
                const UiHandle previewHandle = pane->previewImageHandle();
                if (previewHandle != UI_INVALID_HANDLE)
                    return previewHandle;
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
            resolvePaneLayouts();
            for (auto& pane : panes)
            {
                const ResolvedPaneLayout* resolved = resolvedLayoutFor(pane->descriptor().id);
                pane->build(widgetContext,
                            context.root,
                            font,
                            resolved == nullptr ? gts::ui::fillLayout() : resolved->layout);
            }
        }

        void update(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setState(context.surface,
                                context.root,
                                toolui::visibleState(true, true, false));

            syncChrome(widgetContext);
            resolvePaneLayouts();
            for (auto& pane : panes)
            {
                const ResolvedPaneLayout* resolved = resolvedLayoutFor(pane->descriptor().id);
                pane->update(widgetContext,
                             view,
                             resolved == nullptr ? gts::ui::fillLayout() : resolved->layout,
                             resolved != nullptr && resolved->visible);
            }
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            for (auto& pane : panes)
            {
                const ResolvedPaneLayout* resolved = resolvedLayoutFor(pane->descriptor().id);
                if (resolved != nullptr && resolved->visible)
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
        ToolDockLayout dockLayout;
        ToolWorkspaceLayout workspaceLayout;
        std::vector<ResolvedPaneLayout> resolvedLayouts;

        gts::ui::UiPanelWidget topChrome;
        gts::ui::UiPanelWidget toolbarChrome;
        gts::ui::UiPanelWidget statusChrome;

        std::vector<PaneDescriptor> paneDescriptors() const
        {
            std::vector<PaneDescriptor> descriptors;
            descriptors.reserve(panes.size());
            for (const auto& pane : panes)
                descriptors.push_back(pane->descriptor());
            return descriptors;
        }

        void resolvePaneLayouts()
        {
            const std::vector<PaneDescriptor> descriptors =
                workspaceLayout.apply(paneDescriptors(), view.activeWorkspace);
            resolvedLayouts = dockLayout.resolve(descriptors,
                                                 view.activeWorkspace,
                                                 toolDockLayoutBoundsFromView(view));
        }

        const ResolvedPaneLayout* resolvedLayoutFor(ToolPaneId id) const
        {
            for (const ResolvedPaneLayout& layout : resolvedLayouts)
            {
                if (layout.id == id)
                    return &layout;
            }
            return nullptr;
        }

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

    };
} // namespace gts::tools
