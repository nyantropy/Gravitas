#pragma once

#include <limits>
#include "GlmConfig.h"
#include "Types.h"

// Sentinel: SSBO slot not yet allocated by a binding system
constexpr ssbo_id_type RENDERABLE_SLOT_UNALLOCATED = std::numeric_limits<ssbo_id_type>::max();

// Engine-internal per-object GPU state. Managed by RenderGpuSystem.
// Do not read or write from game code.
//
// modelMatrix    : kept current by RenderGpuSystem when dirty == true.
// readyToRender  : starts false; set true by RenderGpuSystem the first time it writes a
//   valid model matrix from TransformComponent.  RenderCommandExtractor skips any entity
//   where this is still false, preventing the one-frame glitch where a newly created entity
//   is drawn at the origin before RenderGpuSystem has had a chance to compute its matrix.
struct RenderGpuComponent
{
    ssbo_id_type objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
    glm::mat4    modelMatrix    = glm::mat4(1.0f);
    bool         dirty          = true;
    bool         readyToRender  = false;
};
