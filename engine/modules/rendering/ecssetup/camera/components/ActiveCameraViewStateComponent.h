#pragma once

#include <limits>

#include "Entity.h"
#include "GlmConfig.h"

// Engine-owned read-only frame state for the currently active camera.
// Application/UI systems may consume this to project world-space data without
// depending on renderer GPU companion components.
struct ActiveCameraViewStateComponent
{
    Entity    activeCamera = Entity{ std::numeric_limits<entity_id_type>::max() };
    glm::mat4 viewMatrix   = glm::mat4(1.0f);
    glm::mat4 projMatrix   = glm::mat4(1.0f);
    glm::mat4 viewProjMatrix = glm::mat4(1.0f);
    bool      valid        = false;
};
