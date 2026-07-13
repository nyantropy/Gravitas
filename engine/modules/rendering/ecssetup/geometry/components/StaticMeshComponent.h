#pragma once

#include <string>

// gameplay-facing description of a static mesh entity
// contains only the asset path — no GPU handles, no pre-computed data
// StaticMeshBindingSystem reads this and drives MeshGpuComponent
// pair with MaterialReferenceComponent for renderer material state
struct StaticMeshComponent
{
    std::string meshPath;
    // When true, cooked submesh material references are resolved and used per draw range.
    // The default preserves legacy entity-level material overrides for direct mesh usage.
    bool useMeshMaterials = false;
};
