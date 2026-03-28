#pragma once

#include "GlmConfig.h"
#include "Types.h"

// Screen-space image component for the UI stage.
// Renders a textured quad at a normalized viewport position.
// (x, y) = top-left in [0..1] viewport coords.
// width   = desired width in normalized coords [0..1].
// imageAspect = image width / image height (natural ratio of the source image).
// UiCommandExtractor computes the correct normalized height from width,
// imageAspect, and the current viewport aspect ratio.
struct UiImageComponent
{
    texture_id_type textureID    = 0;
    float           x            = 0.0f;   // [0..1] from left
    float           y            = 0.0f;   // [0..1] from top
    float           width        = 0.2f;   // desired width in normalized coords
    float           imageAspect  = 1.0f;   // image width / image height
    glm::vec4       tint         = {1.0f, 1.0f, 1.0f, 1.0f};
    bool            visible      = true;
};
