#pragma once

#include <string>
#include "GlmConfig.h"

// Description of a screen-space image element.
// All position/size values are in normalized viewport coordinates [0..1].
// (x, y) = top-left corner.
// width       = desired width in normalized coords.
// imageAspect = image width / image height — used to compute correct height.
struct UiImageDesc
{
    std::string texturePath;
    float       x           = 0.0f;
    float       y           = 0.0f;
    float       width       = 0.2f;
    float       imageAspect = 1.0f;
    glm::vec4   tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    bool        visible     = true;
};
