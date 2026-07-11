#pragma once

#include "GlmConfig.h"

// Application-facing directional light descriptor.
// Direction is resolved from the entity's WorldTransformComponent: light rays
// travel along the entity's local -Z axis, so shaders receive local +Z as the
// direction from the shaded point toward the light.
struct DirectionalLightComponent
{
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float ambientIntensity = 0.12f;
    bool enabled = true;
    int priority = 0;
    // Compatibility gate retained from the original single-light path.
    // Multiple active directional lights may now be selected by priority.
    bool active = false;
};
