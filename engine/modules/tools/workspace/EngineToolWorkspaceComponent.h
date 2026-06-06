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

        float topBarHeight    = 0.046f;
        float leftRailWidth   = 0.055f;
        float rightPaneWidth  = 0.430f;
        float bottomBarHeight = 0.036f;

        float viewportX      = 0.055f;
        float viewportY      = 0.046f;
        float viewportWidth  = 0.515f;
        float viewportHeight = 0.918f;

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

        workspace.viewportX      = workspace.leftRailWidth;
        workspace.viewportY      = workspace.topBarHeight;
        workspace.viewportWidth  = std::max(0.05f, 1.0f - workspace.leftRailWidth - workspace.rightPaneWidth);
        workspace.viewportHeight = std::max(0.05f, 1.0f - workspace.topBarHeight - workspace.bottomBarHeight);

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
        renderViewport->constrained   = active;
        return *storedWorkspace;
    }
} // namespace gts::tools
