#pragma once

#include <cstdint>
#include <vector>

#include "Vertex.h"

// Gameplay-facing description of a runtime-generated flat quad mesh.
// ProceduralMeshBindingSystem reads this and drives MeshGpuComponent.
// Pair with MaterialComponent to fully describe a renderable entity.
//
// By default this describes a generated quad using width/height in world-space
// units. For authored runtime geometry, set useCustomGeometry = true and
// provide vertices/indices plus a geometryVersion that changes when the mesh
// content changes.
struct ProceduralMeshComponent
{
    float                 width              = 1.0f;
    float                 height             = 1.0f;
    bool                  useCustomGeometry  = false;
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    uint64_t              geometryVersion    = 0;
};
