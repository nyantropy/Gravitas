#pragma once

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
}
