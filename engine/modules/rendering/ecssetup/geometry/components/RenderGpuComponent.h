#pragma once

#include <cstdint>
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
// modelMatrix    : cached representation of WorldTransformComponent::matrix.
// uploadedWorldTransformVersion records the last consumed world-transform version.
// uvTransform    : per-object scene-material UV scale/offset written by
//   TextureAnimationSystem. Defaults to identity so static materials are
//   unchanged.
// readyToRender  : starts false; set true by RenderGpuSystem the first time it
//   consumes a WorldTransformComponent. RenderCommandExtractor skips any entity
//   where this is still false.
// commandDirty   : marks that the cached RenderCommand needs to be rebuilt. Set
//   by binding systems when mesh/material state changes and by RenderGpuSystem
//   when the world-space model matrix changes.
struct RenderGpuComponent
{
    ssbo_id_type objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
    glm::mat4    modelMatrix    = glm::mat4(1.0f);
    glm::vec4    uvTransform    = {1.0f, 1.0f, 0.0f, 0.0f};
    uint32_t     uploadedWorldTransformVersion = 0;
    bool         readyToRender  = false;
    bool         commandDirty   = true;
};
