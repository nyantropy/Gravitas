#pragma once

#include <algorithm>
#include <cmath>

#include "ECSWorld.hpp"
#include "RenderViewportComponent.h"
#include "ToolWorkspace.h"

namespace gts::tools
{
    struct EngineToolWorkspaceComponent
    {
        bool active = false;

        float topBarHeight          = 0.0f;
        float viewportToolbarHeight = 0.0f;
        float leftRailWidth         = 0.0f;
        float rightPaneWidth        = 0.0f;
        float bottomDockHeight      = 0.0f;
        float bottomBarHeight       = 0.0f;

        float viewportX      = 0.0f;
        float viewportY      = 0.0f;
        float viewportWidth  = 1.0f;
        float viewportHeight = 1.0f;

        RenderViewportRect sceneViewport;
    };

    inline RenderViewportRect normalizedViewportToPixels(float x,
                                                         float y,
                                                         float width,
                                                         float height,
                                                         int safeWidth,
                                                         int safeHeight)
    {
        RenderViewportRect viewport{
            static_cast<int>(std::round(x * static_cast<float>(safeWidth))),
            static_cast<int>(std::round(y * static_cast<float>(safeHeight))),
            static_cast<int>(std::round(width * static_cast<float>(safeWidth))),
            static_cast<int>(std::round(height * static_cast<float>(safeHeight)))};
        return viewport.clampedTo(safeWidth, safeHeight);
    }

    inline EngineToolWorkspaceComponent buildEngineToolWorkspace(int windowWidth,
                                                                 int windowHeight,
                                                                 bool active,
                                                                 ToolWorkspace activeWorkspace)
    {
        EngineToolWorkspaceComponent workspace;
        workspace.active = active;

        const int safeWidth  = std::max(1, windowWidth);
        const int safeHeight = std::max(1, windowHeight);

        if (!active)
        {
            workspace.viewportX      = 0.0f;
            workspace.viewportY      = 0.0f;
            workspace.viewportWidth  = 1.0f;
            workspace.viewportHeight = 1.0f;
            workspace.sceneViewport  = RenderViewportRect::full(safeWidth, safeHeight);
            return workspace;
        }

        const bool particleWorkspace = activeWorkspace == ToolWorkspace::Particles;
        const int topBarPixels          = std::clamp(34, 28, std::max(28, safeHeight / 12));
        const int viewportToolbarPixels = std::clamp(44, 34, std::max(34, safeHeight / 13));
        const int leftRailPixels =
            std::clamp(particleWorkspace ? 320 : 300, 220, std::max(220, safeWidth / 3));
        const int rightPanePixels =
            std::clamp(particleWorkspace ? 380 : 340, 280, std::max(280, safeWidth / 3));
        const int paneGapPixels = std::clamp(10, 6, std::max(6, safeWidth / 220));
        const int verticalGapPixels = std::clamp(8, 5, std::max(5, safeHeight / 160));
        const int bottomDockPixels = 0;
        const int bottomBarPixels       = std::clamp(28, 22, std::max(22, safeHeight / 16));

        workspace.topBarHeight          = static_cast<float>(topBarPixels) / static_cast<float>(safeHeight);
        workspace.viewportToolbarHeight = static_cast<float>(viewportToolbarPixels) / static_cast<float>(safeHeight);
        workspace.leftRailWidth         = static_cast<float>(leftRailPixels) / static_cast<float>(safeWidth);
        workspace.rightPaneWidth        = static_cast<float>(rightPanePixels) / static_cast<float>(safeWidth);
        workspace.bottomDockHeight      = static_cast<float>(bottomDockPixels) / static_cast<float>(safeHeight);
        workspace.bottomBarHeight       = static_cast<float>(bottomBarPixels) / static_cast<float>(safeHeight);

        const float paneGapX = static_cast<float>(paneGapPixels) / static_cast<float>(safeWidth);
        const float paneGapY = static_cast<float>(verticalGapPixels) / static_cast<float>(safeHeight);

        workspace.viewportX      = workspace.leftRailWidth + paneGapX;
        workspace.viewportY      = workspace.topBarHeight + workspace.viewportToolbarHeight + paneGapY;
        workspace.viewportWidth  = std::max(0.05f,
                                             1.0f - workspace.leftRailWidth - workspace.rightPaneWidth -
                                                 paneGapX * 2.0f);
        workspace.viewportHeight =
            std::max(0.05f,
                     1.0f - workspace.viewportY - workspace.bottomDockHeight -
                         workspace.bottomBarHeight - paneGapY);

        // Particle and asset workspaces do not expose WorldViewportPane. The
        // runtime scene viewport remains a stable render context behind opaque
        // tool panes; the visible center is owned by each workspace preview.
        float sceneViewportX = workspace.viewportX;
        float sceneViewportY = workspace.viewportY;
        float sceneViewportWidth = workspace.viewportWidth;
        float sceneViewportHeight = workspace.viewportHeight;

        workspace.sceneViewport = normalizedViewportToPixels(sceneViewportX,
                                                             sceneViewportY,
                                                             sceneViewportWidth,
                                                             sceneViewportHeight,
                                                             safeWidth,
                                                             safeHeight);
        return workspace;
    }

    inline EngineToolWorkspaceComponent&
    publishEngineToolWorkspace(ECSWorld& world,
                               int windowWidth,
                               int windowHeight,
                               bool active,
                               ToolWorkspace activeWorkspace)
    {
        EngineToolWorkspaceComponent workspace =
            buildEngineToolWorkspace(windowWidth, windowHeight, active, activeWorkspace);

        EngineToolWorkspaceComponent* storedWorkspace = nullptr;
        if (world.hasAny<EngineToolWorkspaceComponent>())
            storedWorkspace = &world.getSingleton<EngineToolWorkspaceComponent>();
        else
            storedWorkspace = &world.createSingleton<EngineToolWorkspaceComponent>();
        *storedWorkspace = workspace;

        RenderViewportComponent* renderViewport = nullptr;
        if (world.hasAny<RenderViewportComponent>())
            renderViewport = &world.getSingleton<RenderViewportComponent>();
        else
            renderViewport = &world.createSingleton<RenderViewportComponent>();
        renderViewport->sceneViewport = workspace.sceneViewport;
        return *storedWorkspace;
    }
} // namespace gts::tools
