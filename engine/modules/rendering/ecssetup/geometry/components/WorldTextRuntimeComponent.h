#pragma once

#include <string>

#include "GlmConfig.h"
#include "Types.h"

// engine-owned runtime companion for WorldTextComponent
// keeps resolved font handles and last authored values
struct WorldTextRuntimeComponent
{
    font_id_type fontID = 0;
    std::string boundFontPath;

    std::string lastText;
    std::string lastFontPath;
    float       lastScale = 1.0f;
    glm::vec4   lastTint  = {1.0f, 1.0f, 1.0f, 1.0f};
    bool        initialized = false;
};
