#pragma once

#include <vector>
#include "GlmConfig.h"

// evaluated deformation matrices in skin-local binding-slot order, without world placement
struct GtsSkinPalette
{
    std::vector<glm::mat4> matrices;
};
