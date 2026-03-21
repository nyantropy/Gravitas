#pragma once
#include <glm.hpp>

// Local-space axis-aligned bounding box for frustum culling.
// min and max are in the entity's local coordinate space.
// RenderCommandExtractor transforms all 8 corners to world space using
// RenderGpuComponent::modelMatrix before testing against the frustum.
//
// Entities without a BoundsComponent are never culled (safe default).
// For a unit cube centered at origin: min = {-0.5, -0.5, -0.5},
//                                     max = {  0.5,  0.5,  0.5}
struct BoundsComponent
{
    glm::vec3 min{ -0.5f, -0.5f, -0.5f };
    glm::vec3 max{  0.5f,  0.5f,  0.5f };
};
