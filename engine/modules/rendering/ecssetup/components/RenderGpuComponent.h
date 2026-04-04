#pragma once

#include <limits>
#include "GlmConfig.h"
#include "Types.h"

// Sentinel: SSBO slot not yet allocated by a binding system
constexpr ssbo_id_type RENDERABLE_SLOT_UNALLOCATED = std::numeric_limits<ssbo_id_type>::max();

// Engine-internal per-object GPU state. Managed by RenderGpuSystem.
// Do not read or write from game code.
//
// objectSSBOSlot : unique per-entity SSBO slot allocated by a binding system and
//   owned by this component. Automatically recycled when the component or entity
//   is removed from the ECS world.
// modelMatrix    : kept current by RenderGpuSystem when the entity's local
//   transform changes, when a parent transform changes, or when newly bound.
// readyToRender  : starts false; set true by RenderGpuSystem the first time it writes a
//   valid model matrix from TransformComponent.  RenderCommandExtractor skips any entity
//   where this is still false, preventing the one-frame glitch where a newly created entity
//   is drawn at the origin before RenderGpuSystem has had a chance to compute its matrix.
// commandDirty   : marks that the cached RenderCommand needs to be rebuilt. Set
//   by binding systems when mesh/material state changes and by RenderGpuSystem
//   when the world-space model matrix changes.
struct RenderGpuComponent
{
    ssbo_id_type objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
    glm::mat4    modelMatrix    = glm::mat4(1.0f);
    bool         dirty          = true;
    bool         readyToRender  = false;
    bool         commandDirty   = true;
};
