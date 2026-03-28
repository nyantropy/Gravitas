#pragma once

#include <string>
#include "Types.h"

// Engine-internal mesh GPU state. Managed exclusively by mesh binding systems.
// Do not read or write from game code.
struct MeshGpuComponent
{
    mesh_id_type meshID       = 0;

    // Internal tracking for StaticMeshBindingSystem
    std::string  boundMeshPath;

    // Internal tracking for ProceduralMeshBindingSystem
    float        boundWidth   = 0.0f;
    float        boundHeight  = 0.0f;
};
