#pragma once

#include <string>

#include "GlmConfig.h"

// gameplay-facing text description component
// attach to any entity that should display a string in world space
// the entity also needs a TransformComponent that positions the text block
//
// one entity = one text block (multi-line supported via '\n')
// never create one entity per character
struct WorldTextComponent
{
    std::string  text;
    std::string  fontPath;
    float        scale = 1.0f;
    glm::vec4    tint  = {1.0f, 1.0f, 1.0f, 1.0f};
};
