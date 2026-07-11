#pragma once

#include "GlmConfig.h"

// Application-facing point light descriptor.
// Position is resolved from the entity's WorldTransformComponent.
struct PointLightComponent
{
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    float range = 10.0f;
    bool enabled = true;
    int priority = 0;
};

