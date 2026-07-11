#pragma once

#include "GlmConfig.h"

// Application-facing spot light descriptor.
// Position and cone direction are resolved from WorldTransformComponent.
// Cone angles are authored in radians.
struct SpotLightComponent
{
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    float range = 10.0f;
    float innerConeAngle = 0.43633232f; // 25 degrees
    float outerConeAngle = 0.61086524f; // 35 degrees

    bool enabled = true;
    int priority = 0;
};

