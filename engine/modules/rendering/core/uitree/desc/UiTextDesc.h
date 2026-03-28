#pragma once

#include <string>
#include "GlmConfig.h"
#include "BitmapFont.h"

// Description of a screen-space text element.
// (x, y) = top-left in normalized viewport coordinates [0..1].
// scale = viewport-height units per font lineHeight.
struct UiTextDesc
{
    std::string text;
    BitmapFont* font    = nullptr;
    float       x       = 0.0f;
    float       y       = 0.0f;
    float       scale   = 0.05f;
    glm::vec4   color   = {1.0f, 1.0f, 1.0f, 1.0f};
    bool        visible = true;
};
