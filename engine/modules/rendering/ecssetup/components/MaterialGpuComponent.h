#pragma once

#include <string>
#include "Types.h"
#include "GlmConfig.h"

// Engine-internal material GPU state. Managed exclusively by binding systems.
// Do not read or write from game code.
//
// textureID points at shared/cached texture resources. It is not owned by an
// individual ECS entity and therefore is not released on component teardown.
struct MaterialGpuComponent
{
    texture_id_type textureID        = 0;
    glm::vec4       tint             = {1.0f, 1.0f, 1.0f, 1.0f};
    float           alpha            = 1.0f;
    bool            doubleSided      = false;

    // Internal tracking
    std::string     boundTexturePath;
};
