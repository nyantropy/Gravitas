#pragma once

#include "GlmConfig.h"
#include "Types.h"

// Screen-space image component for the UI stage.
// Renders a textured quad at a normalized viewport rectangle.
// (x, y) = top-left, (w, h) = size — all in [0..1] viewport coords.
struct UiImageComponent
{
    texture_id_type textureID = 0;
    float           x         = 0.0f;
    float           y         = 0.0f;
    float           w         = 0.2f;
    float           h         = 0.2f;
    glm::vec4       tint      = {1.0f, 1.0f, 1.0f, 1.0f};
    bool            visible   = true;
};
