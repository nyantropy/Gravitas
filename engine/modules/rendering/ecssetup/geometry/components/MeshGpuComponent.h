#pragma once

#include <string>
#include "Types.h"

// Engine-internal mesh GPU state. Managed exclusively by mesh binding systems.
// Do not read or write from game code.
//
// meshID:
//   Shared static meshes are cache-backed and not owned by this component.
//   Uploaded runtime meshes are entity-owned; ownsProceduralMeshResource marks those
//   cases so ECS teardown can release them automatically.
struct MeshGpuComponent
{
    mesh_id_type meshID       = 0;
    bool         ownsProceduralMeshResource = false;

    // Internal tracking for StaticMeshBindingSystem
    std::string  boundMeshPath;

    // Internal tracking for QuadMeshBindingSystem
    float        boundQuadWidth  = 0.0f;
    float        boundQuadHeight = 0.0f;

    // Internal tracking for DynamicMeshBindingSystem
    uint64_t     boundDynamicGeometryVersion = 0;
    uint64_t     attemptedDynamicGeometryVersion = 0;
    uint32_t     dynamicVertexCapacityBytes = 0;
    uint32_t     dynamicIndexCapacityBytes = 0;
    uint32_t     dynamicVertexUsedBytes = 0;
    uint32_t     dynamicIndexUsedBytes = 0;
};
