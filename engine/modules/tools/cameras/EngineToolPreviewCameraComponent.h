#pragma once

#include "GlmConfig.h"

namespace gts::tools
{
    struct EngineToolPreviewCameraComponent
    {
        bool active = false;

        glm::vec3 position = {0.0f, 1.0f, 4.0f};
        glm::vec3 target   = {0.0f, 0.6f, 0.0f};
        glm::vec3 up       = {0.0f, 1.0f, 0.0f};
        float     fov      = 70.0f;
    };
} // namespace gts::tools
