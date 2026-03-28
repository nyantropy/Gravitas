#pragma once

#include <string>
#include "GlmConfig.h"
#include "BitmapFont.h"
#include "UiElement.h"

struct UiTextElement : public UiElement
{
    std::string text;
    BitmapFont* font  = nullptr;
    float       x     = 0.0f;
    float       y     = 0.0f;
    float       scale = 0.05f;
    glm::vec4   color = {1.0f, 1.0f, 1.0f, 1.0f};
};
