#pragma once

#include "ECSWorld.hpp"

namespace gts::debugdraw
{
    struct DebugDrawSettingsComponent
    {
        bool enabled = true;
        bool selectedBounds = true;
        bool allBounds = false;
        bool transformAxes = true;
        bool cameraFrustum = false;
        bool pickRay = false;

        float lineThickness = 0.025f;
        float axisLength = 1.35f;
        float pickRayLength = 30.0f;
    };

    inline DebugDrawSettingsComponent& ensureSettings(ECSWorld& world)
    {
        if (!world.hasAny<DebugDrawSettingsComponent>())
            return world.createSingleton<DebugDrawSettingsComponent>();
        return world.getSingleton<DebugDrawSettingsComponent>();
    }
}
