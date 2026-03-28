#pragma once

#include <string>
#include "GlmConfig.h"
#include "Types.h"
#include "UiElement.h"

struct UiImageElement : public UiElement
{
    // Description fields
    std::string texturePath;
    float       x           = 0.0f;
    float       y           = 0.0f;
    float       width       = 0.2f;
    float       imageAspect = 1.0f;
    glm::vec4   tint        = {1.0f, 1.0f, 1.0f, 1.0f};

    // Resolved GPU handle — managed by UiTree internally
    texture_id_type textureID        = 0;
    std::string     boundTexturePath;
};
