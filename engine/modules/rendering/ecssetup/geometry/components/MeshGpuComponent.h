#pragma once

#include <string>
#include "Types.h"

// Engine-internal mesh GPU state. Managed exclusively by mesh binding systems.
// Do not read or write from game code.
//
// meshID:
//   Shared static meshes are cache-backed and not owned by this component.
//   Procedural meshes are entity-owned; ownsProceduralMeshResource marks those
//   cases so ECS teardown can release them automatically.
struct MeshGpuComponent
{
    mesh_id_type meshID       = 0;
    bool         ownsProceduralMeshResource = false;

    // Internal tracking for StaticMeshBindingSystem
    std::string  boundMeshPath;

    // Internal tracking for ProceduralMeshBindingSystem
    float        boundWidth   = 0.0f;
    float        boundHeight  = 0.0f;
    uint64_t     boundGeometryVersion = 0;
};
