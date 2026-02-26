#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <glm.hpp>

#include "Types.h"

// sentinel value indicating the SSBO slot has not yet been allocated by RenderBindingSystem
constexpr ssbo_id_type RENDERABLE_SLOT_UNALLOCATED = std::numeric_limits<ssbo_id_type>::max();

// Internal renderer-maintained component.  Created and updated exclusively by
// RenderBindingSystem — gameplay and visual systems must not write to this directly.
//
// boundMeshPath / boundTexturePath : the paths used to resolve the current resource IDs.
//   Compared against RenderDescriptionComponent each frame to detect when a rebind is needed.
// modelMatrix    : kept current by RenderableTransformSystem when dirty == true.
struct RenderableComponent
{
    mesh_id_type    meshID         = 0;
    texture_id_type textureID      = 0;
    ssbo_id_type    objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
    glm::mat4       modelMatrix    = glm::mat4(1.0f);
    bool            dirty          = true;

    std::string boundMeshPath;
    std::string boundTexturePath;
};
