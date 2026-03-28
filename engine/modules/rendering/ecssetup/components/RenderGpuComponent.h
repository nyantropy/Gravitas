#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include "GlmConfig.h"

#include "Types.h"

// sentinel value indicating the SSBO slot has not yet been allocated by RenderBindingSystem
constexpr ssbo_id_type RENDERABLE_SLOT_UNALLOCATED = std::numeric_limits<ssbo_id_type>::max();

// Internal renderer-maintained component.  Created and updated exclusively by
// RenderBindingSystem — gameplay and visual systems must not write to this directly.
//
// boundMeshPath / boundTexturePath : the paths used to resolve the current resource IDs.
//   Compared against RenderDescriptionComponent each frame to detect when a rebind is needed.
// modelMatrix    : kept current by RenderGpuSystem when dirty == true.
// readyToRender  : starts false; set true by RenderGpuSystem the first time it writes a
//   valid model matrix from TransformComponent.  RenderCommandExtractor skips any entity
//   where this is still false, preventing the one-frame glitch where a newly created entity
//   is drawn at the origin before RenderGpuSystem has had a chance to compute its matrix.
struct RenderGpuComponent
{
    mesh_id_type    meshID         = 0;
    texture_id_type textureID      = 0;
    ssbo_id_type    objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
    glm::mat4       modelMatrix    = glm::mat4(1.0f);
    bool            dirty          = true;
    bool            readyToRender  = false;
    bool            doubleSided    = false;  // synced from RenderDescriptionComponent / WorldImageBindingSystem
    float           alpha          = 1.0f;   // synced from RenderDescriptionComponent by RenderBindingSystem

    std::string boundMeshPath;
    std::string boundTexturePath;
};
