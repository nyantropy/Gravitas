#pragma once

#include <algorithm>
#include <vector>

#include "EngineToolUiHelpers.h"
#include "ToolPane.h"

namespace gts::tools
{
    struct ToolDockLayoutBounds
    {
        float menuBarHeight = 0.0f;
        float toolbarHeight = 0.0f;
        float leftWidth = 0.0f;
        float rightWidth = 0.0f;
        float bottomHeight = 0.0f;
        float statusBarHeight = 0.0f;
        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerWidth = 1.0f;
        float centerHeight = 1.0f;
    };

    struct ResolvedPaneLayout
    {
        ToolPaneId id = ToolPaneId::WorldViewport;
        bool visible = false;
        ToolDockArea dockArea = ToolDockArea::Center;
        UiLayoutSpec layout = gts::ui::fillLayout();
        int order = 0;
    };

    inline ToolDockLayoutBounds toolDockLayoutBoundsFromView(const ToolShellView& view)
    {
        ToolDockLayoutBounds bounds;
        bounds.menuBarHeight = view.topBarHeight;
        bounds.toolbarHeight = view.viewportToolbarHeight;
        bounds.leftWidth = view.leftPaneWidth;
        bounds.rightWidth = view.rightPaneWidth;
        bounds.bottomHeight = view.bottomDockHeight;
        bounds.statusBarHeight = view.bottomBarHeight;
        bounds.centerX = view.viewportX;
        bounds.centerY = view.viewportY;
        bounds.centerWidth = view.viewportWidth;
        bounds.centerHeight = view.viewportHeight;
        return bounds;
    }

    class ToolDockLayout
    {
    public:
        std::vector<ResolvedPaneLayout> resolve(const std::vector<PaneDescriptor>& descriptors,
                                                ToolWorkspace workspace,
                                                const ToolDockLayoutBounds& bounds) const
        {
            std::vector<ResolvedPaneLayout> resolved;
            resolved.reserve(descriptors.size());

            for (const PaneDescriptor& descriptor : descriptors)
            {
                ResolvedPaneLayout layout;
                layout.id = descriptor.id;
                layout.visible = paneVisibleInWorkspace(descriptor, workspace);
                layout.dockArea = descriptor.dockArea;
                layout.order = descriptor.order;
                resolved.push_back(layout);
            }

            resolveOverlayArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::MenuBar,
                               {0.0f, 0.0f, 1.0f, bounds.menuBarHeight});
            resolveOverlayArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::Toolbar,
                               {0.0f, bounds.menuBarHeight, 1.0f, bounds.toolbarHeight});
            resolveStackedArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::Left,
                               {0.0f,
                                bounds.centerY,
                                std::clamp(bounds.leftWidth, 0.0f, 0.45f),
                                sidePaneHeight(bounds)});
            resolveOverlayArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::Center,
                               {bounds.centerX, bounds.centerY, bounds.centerWidth, bounds.centerHeight});
            resolveStackedArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::Right,
                               {std::max(0.0f, 1.0f - std::clamp(bounds.rightWidth, 0.0f, 0.45f)),
                                bounds.centerY,
                                std::clamp(bounds.rightWidth, 0.0f, 0.45f),
                                sidePaneHeight(bounds)});
            resolveOverlayArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::Bottom,
                               {bounds.centerX,
                                bounds.centerY + bounds.centerHeight,
                                bounds.centerWidth,
                                bounds.bottomHeight});
            resolveOverlayArea(descriptors,
                               resolved,
                               workspace,
                               ToolDockArea::StatusBar,
                               {0.0f, 1.0f - bounds.statusBarHeight, 1.0f, bounds.statusBarHeight});

            return resolved;
        }

    private:
        static float sidePaneHeight(const ToolDockLayoutBounds& bounds)
        {
            const float statusTop = std::max(bounds.centerY, 1.0f - bounds.statusBarHeight);
            return std::max(0.0f, statusTop - bounds.centerY);
        }

        static std::vector<size_t> collectVisibleArea(const std::vector<PaneDescriptor>& descriptors,
                                                      const std::vector<ResolvedPaneLayout>& resolved,
                                                      ToolDockArea area)
        {
            std::vector<size_t> indices;
            for (size_t i = 0; i < descriptors.size() && i < resolved.size(); ++i)
            {
                if (resolved[i].visible && descriptors[i].dockArea == area)
                    indices.push_back(i);
            }

            std::sort(indices.begin(),
                      indices.end(),
                      [&](size_t lhs, size_t rhs)
                      {
                          if (descriptors[lhs].order != descriptors[rhs].order)
                              return descriptors[lhs].order < descriptors[rhs].order;
                          return static_cast<int>(descriptors[lhs].id) < static_cast<int>(descriptors[rhs].id);
                      });
            return indices;
        }

        static float preferredSize(const PaneDescriptor& descriptor, ToolWorkspace workspace)
        {
            float workspaceSize = 0.0f;
            switch (workspace)
            {
                case ToolWorkspace::World:
                    workspaceSize = descriptor.preferredSizeInWorld;
                    break;
                case ToolWorkspace::Particles:
                    workspaceSize = descriptor.preferredSizeInParticles;
                    break;
                case ToolWorkspace::Assets:
                    workspaceSize = descriptor.preferredSizeInAssets;
                    break;
            }
            return workspaceSize > 0.0f ? workspaceSize : descriptor.preferredSize;
        }

        static void resolveOverlayArea(const std::vector<PaneDescriptor>& descriptors,
                                       std::vector<ResolvedPaneLayout>& resolved,
                                       ToolWorkspace workspace,
                                       ToolDockArea area,
                                       const UiRect& areaRect)
        {
            const std::vector<size_t> indices = collectVisibleArea(descriptors, resolved, area);

            for (size_t index : indices)
            {
                const PaneDescriptor& descriptor = descriptors[index];
                const float requestedWidth = preferredSize(descriptor, workspace);
                const float width = requestedWidth > 0.0f
                    ? std::max(descriptor.minimumSize, requestedWidth)
                    : areaRect.width;
                const float offset = std::clamp(descriptor.leadingOffset, 0.0f, 1.0f);
                resolved[index].layout = toolui::rect(areaRect.x + areaRect.width * offset,
                                                      areaRect.y,
                                                      std::min(width, std::max(0.0f, areaRect.width * (1.0f - offset))),
                                                      areaRect.height);
            }
        }

        static void resolveStackedArea(const std::vector<PaneDescriptor>& descriptors,
                                       std::vector<ResolvedPaneLayout>& resolved,
                                       ToolWorkspace workspace,
                                       ToolDockArea area,
                                       const UiRect& areaRect)
        {
            const std::vector<size_t> indices = collectVisibleArea(descriptors, resolved, area);
            if (indices.empty())
                return;

            float fixedFraction = 0.0f;
            size_t flexCount = 0;
            for (size_t index : indices)
            {
                const float preferred = preferredSize(descriptors[index], workspace);
                if (preferred > 0.0f)
                    fixedFraction += std::clamp(preferred, 0.0f, 1.0f);
                else
                    ++flexCount;
            }

            const float flexFraction = flexCount == 0
                ? 0.0f
                : std::max(0.0f, 1.0f - fixedFraction) / static_cast<float>(flexCount);
            float cursorY = areaRect.y;

            for (size_t index : indices)
            {
                const PaneDescriptor& descriptor = descriptors[index];
                const float preferred = preferredSize(descriptor, workspace);
                const float fraction = preferred > 0.0f ? std::clamp(preferred, 0.0f, 1.0f) : flexFraction;
                const float height = std::max(descriptor.minimumSize, areaRect.height * fraction);
                resolved[index].layout = toolui::rect(areaRect.x,
                                                      cursorY,
                                                      areaRect.width,
                                                      std::min(height, std::max(0.0f, areaRect.y + areaRect.height - cursorY)));
                cursorY += height;
            }
        }
    };
} // namespace gts::tools
