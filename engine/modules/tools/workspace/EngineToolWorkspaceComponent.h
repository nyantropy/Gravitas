#pragma once

#include <algorithm>
#include <cmath>

#include "ECSWorld.hpp"
#include "RenderViewportComponent.h"

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

    inline EngineToolWorkspaceComponent buildEngineToolWorkspace(int windowWidth, int windowHeight, bool active)
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

        const int topBarPixels          = std::clamp(30, 24, std::max(24, safeHeight / 12));
        const int viewportToolbarPixels = std::clamp(28, 22, std::max(22, safeHeight / 14));
        const int leftRailPixels        = std::clamp(280, 180, std::max(180, safeWidth / 3));
        const int rightPanePixels       = std::clamp(360, 240, std::max(240, safeWidth / 3));
        const int bottomDockPixels      = std::clamp(140, 96, std::max(96, safeHeight / 4));
        const int bottomBarPixels       = std::clamp(26, 20, std::max(20, safeHeight / 16));

        workspace.topBarHeight          = static_cast<float>(topBarPixels) / static_cast<float>(safeHeight);
        workspace.viewportToolbarHeight = static_cast<float>(viewportToolbarPixels) / static_cast<float>(safeHeight);
        workspace.leftRailWidth         = static_cast<float>(leftRailPixels) / static_cast<float>(safeWidth);
        workspace.rightPaneWidth        = static_cast<float>(rightPanePixels) / static_cast<float>(safeWidth);
        workspace.bottomDockHeight      = static_cast<float>(bottomDockPixels) / static_cast<float>(safeHeight);
        workspace.bottomBarHeight       = static_cast<float>(bottomBarPixels) / static_cast<float>(safeHeight);

        workspace.viewportX      = workspace.leftRailWidth;
        workspace.viewportY      = workspace.topBarHeight + workspace.viewportToolbarHeight;
        workspace.viewportWidth  = std::max(0.05f, 1.0f - workspace.leftRailWidth - workspace.rightPaneWidth);
        workspace.viewportHeight =
            std::max(0.05f, 1.0f - workspace.viewportY - workspace.bottomDockHeight - workspace.bottomBarHeight);

        workspace.sceneViewport = {
            static_cast<int>(std::round(workspace.viewportX * static_cast<float>(safeWidth))),
            static_cast<int>(std::round(workspace.viewportY * static_cast<float>(safeHeight))),
            static_cast<int>(std::round(workspace.viewportWidth * static_cast<float>(safeWidth))),
            static_cast<int>(std::round(workspace.viewportHeight * static_cast<float>(safeHeight)))};
        workspace.sceneViewport = workspace.sceneViewport.clampedTo(safeWidth, safeHeight);
        return workspace;
    }

    inline EngineToolWorkspaceComponent&
    publishEngineToolWorkspace(ECSWorld& world, int windowWidth, int windowHeight, bool active)
    {
        EngineToolWorkspaceComponent workspace = buildEngineToolWorkspace(windowWidth, windowHeight, active);

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
