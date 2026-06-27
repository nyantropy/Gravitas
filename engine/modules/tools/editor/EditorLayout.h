#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "BitmapFont.h"
#include "EditorTheme.h"
#include "ToolWidgets.h"
#include "UiSystem.h"

namespace gts::tools
{
    enum class EditorDockArea
    {
        Center,
        Left,
        Right,
        Top,
        Bottom
    };

    enum class EditorSplitOrientation
    {
        Horizontal,
        Vertical
    };

    struct EditorPanelState
    {
        std::string    id;
        std::string    title;
        std::string    subtitle;
        EditorDockArea area = EditorDockArea::Center;

        float size         = 0.25f;
        float minSize      = 0.08f;
        float maxSize      = 0.90f;
        float restoredSize = 0.25f;

        bool visible     = true;
        bool collapsed   = false;
        bool resizable   = true;
        bool collapsible = true;
        bool closable    = false;
    };

    struct EditorTabSpec
    {
        std::string id;
        std::string label;
        bool        active   = false;
        bool        visible  = true;
        bool        closable = false;
    };

    struct EditorPanelHandles
    {
        UiHandle root           = UI_INVALID_HANDLE;
        UiHandle header         = UI_INVALID_HANDLE;
        UiHandle title          = UI_INVALID_HANDLE;
        UiHandle subtitle       = UI_INVALID_HANDLE;
        UiHandle body           = UI_INVALID_HANDLE;
        UiHandle collapseButton = UI_INVALID_HANDLE;
        UiHandle closeButton    = UI_INVALID_HANDLE;
        UiHandle resizeHandle   = UI_INVALID_HANDLE;
    };

    struct EditorBarHandles
    {
        UiHandle root  = UI_INVALID_HANDLE;
        UiHandle label = UI_INVALID_HANDLE;
    };

    struct EditorTabBarHandles
    {
        UiHandle                root = UI_INVALID_HANDLE;
        std::vector<ToolButton> tabs;
    };

    struct EditorSplitViewSpec
    {
        EditorSplitOrientation orientation  = EditorSplitOrientation::Horizontal;
        float                  firstRatio   = 0.50f;
        float                  splitterSize = 0.006f;
        bool                   resizable    = true;
    };

    struct EditorSplitViewHandles
    {
        UiHandle root     = UI_INVALID_HANDLE;
        UiHandle first    = UI_INVALID_HANDLE;
        UiHandle splitter = UI_INVALID_HANDLE;
        UiHandle second   = UI_INVALID_HANDLE;
    };

    struct EditorDockLayoutSpec
    {
        bool showToolbar = true;
        bool showSidebar = true;
        bool showFooter  = true;

        float toolbarHeight    = DefaultEditorTheme.dimensions.toolbarHeight;
        float sidebarWidth     = DefaultEditorTheme.dimensions.sidebarWidth;
        float footerHeight     = DefaultEditorTheme.dimensions.statusBarHeight;
        float resizeHandleSize = DefaultEditorTheme.borders.focusWidth;

        std::string                   toolbarTitle = "GRAVITAS";
        std::string                   footerStatus;
        std::vector<EditorPanelState> panels;
        std::vector<EditorTabSpec>    workspaceTabs;
    };

    struct EditorDockLayoutHandles
    {
        UiHandle                        root = UI_INVALID_HANDLE;
        EditorBarHandles                toolbar;
        EditorBarHandles                sidebar;
        EditorBarHandles                footer;
        UiHandle                        center = UI_INVALID_HANDLE;
        UiRect                          centerRect;
        std::vector<EditorPanelHandles> panels;
        EditorTabBarHandles             workspaceTabs;
    };

    inline float clampPanelSize(const EditorPanelState& panel, float value)
    {
        return std::clamp(value, panel.minSize, panel.maxSize);
    }

    inline void setPanelSize(EditorPanelState& panel, float value)
    {
        panel.size = clampPanelSize(panel, value);
        if (!panel.collapsed)
            panel.restoredSize = panel.size;
    }

    inline void collapsePanel(EditorPanelState& panel)
    {
        if (panel.collapsed)
            return;

        panel.restoredSize = panel.size;
        panel.collapsed    = true;
    }

    inline void expandPanel(EditorPanelState& panel)
    {
        panel.collapsed = false;
        panel.size      = clampPanelSize(panel, panel.restoredSize);
    }

    inline void hidePanel(EditorPanelState& panel)
    {
        panel.visible = false;
    }

    inline void restorePanel(EditorPanelState& panel)
    {
        panel.visible = true;
        if (panel.collapsed)
            expandPanel(panel);
    }

    inline ToolRect toToolRect(const UiRect& rect)
    {
        return {rect.x, rect.y, rect.width, rect.height};
    }

    inline UiRect insetRect(const UiRect& rect, float inset)
    {
        const float clampedInset = std::max(0.0f, inset);
        return {rect.x + clampedInset,
                rect.y + clampedInset,
                std::max(0.0f, rect.width - clampedInset * 2.0f),
                std::max(0.0f, rect.height - clampedInset * 2.0f)};
    }

    inline EditorBarHandles createEditorToolbar(UiSystem&          ui,
                                                UiHandle           parent,
                                                const UiRect&      rect,
                                                BitmapFont*        font,
                                                std::string_view   title,
                                                const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorBarHandles handles;
        handles.root  = createRectRelative(ui, parent, toToolRect(rect), theme.colors.barBackground);
        handles.label = createTextRelative(ui,
                                           handles.root,
                                           {theme.spacing.shellPadding, 0.16f, 0.40f, 0.70f},
                                           font,
                                           std::string(title),
                                           theme.colors.text,
                                           theme.typography.titleScale);
        setTextAlignment(ui, handles.label, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
        return handles;
    }

    inline EditorBarHandles createEditorSidebar(UiSystem&          ui,
                                                UiHandle           parent,
                                                const UiRect&      rect,
                                                BitmapFont*        font,
                                                const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorBarHandles handles;
        handles.root  = createRectRelative(ui, parent, toToolRect(rect), theme.colors.railBackground);
        handles.label = createTextRelative(ui,
                                           handles.root,
                                           {0.08f, 0.015f, 0.84f, 0.040f},
                                           font,
                                           "",
                                           theme.colors.mutedText,
                                           theme.typography.smallScale);
        return handles;
    }

    inline EditorBarHandles createEditorFooter(UiSystem&          ui,
                                               UiHandle           parent,
                                               const UiRect&      rect,
                                               BitmapFont*        font,
                                               std::string_view   status,
                                               const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorBarHandles handles;
        handles.root  = createRectRelative(ui, parent, toToolRect(rect), theme.colors.barBackground);
        handles.label = createTextRelative(ui,
                                           handles.root,
                                           {theme.spacing.shellPadding, 0.16f, 0.82f, 0.70f},
                                           font,
                                           std::string(status),
                                           theme.colors.statusText,
                                           theme.typography.smallScale);
        setTextAlignment(ui, handles.label, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
        return handles;
    }

    inline EditorPanelHandles createEditorPanel(UiSystem&               ui,
                                                UiHandle                parent,
                                                const UiRect&           rect,
                                                const EditorPanelState& panel,
                                                BitmapFont*             font,
                                                const EditorTheme&      theme = DefaultEditorTheme)
    {
        EditorPanelHandles handles;
        if (!panel.visible)
            return handles;

        handles.root = createRectRelative(ui, parent, toToolRect(rect), theme.colors.panelSurface);

        const float headerRatio =
            rect.height <= 0.0f ? 1.0f : std::clamp(theme.dimensions.compactRowHeight / rect.height, 0.08f, 1.0f);
        handles.header =
            createRectRelative(ui, handles.root, {0.0f, 0.0f, 1.0f, headerRatio}, theme.colors.sectionHeader);
        handles.title    = createTextRelative(ui,
                                              handles.header,
                                              {0.025f, 0.12f, 0.52f, 0.76f},
                                              font,
                                              panel.title,
                                              theme.colors.text,
                                              theme.typography.buttonScale);
        handles.subtitle = createTextRelative(ui,
                                              handles.header,
                                              {0.56f, 0.14f, 0.27f, 0.72f},
                                              font,
                                              panel.subtitle,
                                              theme.colors.mutedText,
                                              theme.typography.smallScale);
        setTextAlignment(ui, handles.subtitle, UiHorizontalAlign::Right, UiVerticalAlign::Middle);

        if (panel.collapsible)
        {
            handles.collapseButton =
                createRectRelative(ui, handles.header, {0.850f, 0.18f, 0.055f, 0.64f}, theme.colors.button, true);
        }
        if (panel.closable)
        {
            handles.closeButton =
                createRectRelative(ui, handles.header, {0.920f, 0.18f, 0.055f, 0.64f}, theme.colors.button, true);
        }

        const float bodyY       = panel.collapsed ? 1.0f : headerRatio;
        const float bodyHeight  = panel.collapsed ? 0.0f : std::max(0.0f, 1.0f - headerRatio);
        handles.body            = createContainerRelative(ui, handles.root, {0.0f, bodyY, 1.0f, bodyHeight});
        UiLayoutSpec bodyLayout = relativeLayout({0.0f, bodyY, 1.0f, bodyHeight});
        bodyLayout.padding      = theme.spacing.panelPadding;
        bodyLayout.clipMode     = UiClipMode::ClipChildren;
        ui.setLayout(handles.body, bodyLayout);

        if (panel.resizable && !panel.collapsed)
        {
            handles.resizeHandle =
                createRectRelative(ui, handles.root, {0.0f, 0.985f, 1.0f, 0.015f}, theme.colors.border, true);
        }

        return handles;
    }

    inline ToolPanelFrame createEditorPanelFrameRelative(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, BitmapFont* font, const EditorPanelState& panel)
    {
        if (!panel.visible)
            return {};

        return createPanelFrameRelative(ui, parent, rect, font, panel.title, panel.subtitle);
    }

    inline EditorTabBarHandles createEditorTabBar(UiSystem&                         ui,
                                                  UiHandle                          parent,
                                                  const UiRect&                     rect,
                                                  const std::vector<EditorTabSpec>& tabs,
                                                  BitmapFont*                       font,
                                                  const EditorTheme&                theme = DefaultEditorTheme)
    {
        EditorTabBarHandles handles;
        handles.root = createContainerRelative(ui, parent, toToolRect(rect));

        const size_t visibleCount = static_cast<size_t>(std::count_if(tabs.begin(),
                                                                      tabs.end(),
                                                                      [](const EditorTabSpec& tab)
                                                                      {
                                                                          return tab.visible;
                                                                      }));
        if (visibleCount == 0)
            return handles;

        const float gap = theme.spacing.xs;
        const float width =
            std::max(0.02f, (1.0f - gap * static_cast<float>(visibleCount - 1)) / static_cast<float>(visibleCount));
        size_t visibleIndex = 0;
        for (const EditorTabSpec& tab : tabs)
        {
            if (!tab.visible)
                continue;

            const float x      = static_cast<float>(visibleIndex) * (width + gap);
            ToolButton  button = createButtonRelative(
                ui, handles.root, {x, 0.0f, width, 1.0f}, font, tab.label, theme.typography.buttonScale);
            if (tab.active)
                setRectColor(ui, button.rect, theme.colors.selection);
            handles.tabs.push_back(button);
            ++visibleIndex;
        }

        return handles;
    }

    inline EditorSplitViewHandles createEditorSplitView(UiSystem&                  ui,
                                                        UiHandle                   parent,
                                                        const UiRect&              rect,
                                                        const EditorSplitViewSpec& split,
                                                        const EditorTheme&         theme = DefaultEditorTheme)
    {
        EditorSplitViewHandles handles;
        handles.root = createContainerRelative(ui, parent, toToolRect(rect));

        const float ratio    = std::clamp(split.firstRatio, 0.05f, 0.95f);
        const float splitter = std::max(0.0f, split.splitterSize);
        if (split.orientation == EditorSplitOrientation::Horizontal)
        {
            const float firstWidth = std::max(0.0f, ratio - splitter * 0.5f);
            const float secondX    = std::min(1.0f, ratio + splitter * 0.5f);
            handles.first          = createContainerRelative(ui, handles.root, {0.0f, 0.0f, firstWidth, 1.0f});
            handles.splitter       = createRectRelative(
                ui, handles.root, {firstWidth, 0.0f, splitter, 1.0f}, theme.colors.border, split.resizable);
            handles.second =
                createContainerRelative(ui, handles.root, {secondX, 0.0f, std::max(0.0f, 1.0f - secondX), 1.0f});
        }
        else
        {
            const float firstHeight = std::max(0.0f, ratio - splitter * 0.5f);
            const float secondY     = std::min(1.0f, ratio + splitter * 0.5f);
            handles.first           = createContainerRelative(ui, handles.root, {0.0f, 0.0f, 1.0f, firstHeight});
            handles.splitter        = createRectRelative(
                ui, handles.root, {0.0f, firstHeight, 1.0f, splitter}, theme.colors.border, split.resizable);
            handles.second =
                createContainerRelative(ui, handles.root, {0.0f, secondY, 1.0f, std::max(0.0f, 1.0f - secondY)});
        }

        return handles;
    }

    inline float panelExtent(const EditorPanelState& panel, const EditorTheme& theme)
    {
        if (!panel.visible)
            return 0.0f;
        if (panel.collapsed)
            return theme.dimensions.compactRowHeight;
        return clampPanelSize(panel, panel.size);
    }

    inline EditorDockLayoutHandles buildEditorDockLayout(UiSystem&                   ui,
                                                         UiHandle                    parent,
                                                         BitmapFont*                 font,
                                                         const EditorDockLayoutSpec& spec,
                                                         const EditorTheme&          theme = DefaultEditorTheme)
    {
        EditorDockLayoutHandles handles;
        handles.root = createContainerRelative(ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});

        UiRect remaining = {0.0f, 0.0f, 1.0f, 1.0f};
        if (spec.showToolbar)
        {
            handles.toolbar = createEditorToolbar(
                ui, handles.root, {0.0f, 0.0f, 1.0f, spec.toolbarHeight}, font, spec.toolbarTitle, theme);
            remaining.y += spec.toolbarHeight;
            remaining.height = std::max(0.0f, remaining.height - spec.toolbarHeight);
        }

        if (spec.showFooter)
        {
            handles.footer =
                createEditorFooter(ui,
                                   handles.root,
                                   {0.0f, std::max(0.0f, 1.0f - spec.footerHeight), 1.0f, spec.footerHeight},
                                   font,
                                   spec.footerStatus,
                                   theme);
            remaining.height = std::max(0.0f, remaining.height - spec.footerHeight);
        }

        if (spec.showSidebar)
        {
            handles.sidebar = createEditorSidebar(
                ui, handles.root, {0.0f, remaining.y, spec.sidebarWidth, remaining.height}, font, theme);
            remaining.x += spec.sidebarWidth;
            remaining.width = std::max(0.0f, remaining.width - spec.sidebarWidth);
        }

        for (const EditorPanelState& panel : spec.panels)
        {
            if (!panel.visible || panel.area == EditorDockArea::Center)
                continue;

            UiRect      panelRect = remaining;
            const float extent    = panelExtent(panel, theme);
            switch (panel.area)
            {
            case EditorDockArea::Left:
                panelRect.width = std::min(extent, remaining.width);
                remaining.x += panelRect.width;
                remaining.width = std::max(0.0f, remaining.width - panelRect.width);
                break;
            case EditorDockArea::Right:
                panelRect.width = std::min(extent, remaining.width);
                panelRect.x     = remaining.x + remaining.width - panelRect.width;
                remaining.width = std::max(0.0f, remaining.width - panelRect.width);
                break;
            case EditorDockArea::Top:
                panelRect.height = std::min(extent, remaining.height);
                remaining.y += panelRect.height;
                remaining.height = std::max(0.0f, remaining.height - panelRect.height);
                break;
            case EditorDockArea::Bottom:
                panelRect.height = std::min(extent, remaining.height);
                panelRect.y      = remaining.y + remaining.height - panelRect.height;
                remaining.height = std::max(0.0f, remaining.height - panelRect.height);
                break;
            case EditorDockArea::Center:
                break;
            }

            handles.panels.push_back(
                createEditorPanel(ui, handles.root, insetRect(panelRect, theme.spacing.xs), panel, font, theme));
        }

        handles.centerRect = insetRect(remaining, theme.spacing.xs);
        handles.center =
            createRectRelative(ui, handles.root, toToolRect(handles.centerRect), theme.colors.windowBackground);

        for (const EditorPanelState& panel : spec.panels)
        {
            if (!panel.visible || panel.area != EditorDockArea::Center)
                continue;

            handles.panels.push_back(createEditorPanel(ui, handles.root, handles.centerRect, panel, font, theme));
            break;
        }

        if (!spec.workspaceTabs.empty())
        {
            handles.workspaceTabs = createEditorTabBar(ui,
                                                       handles.center,
                                                       {0.0f, 0.0f, 1.0f, theme.dimensions.tabHeight * 1.8f},
                                                       spec.workspaceTabs,
                                                       font,
                                                       theme);
        }

        return handles;
    }
} // namespace gts::tools
