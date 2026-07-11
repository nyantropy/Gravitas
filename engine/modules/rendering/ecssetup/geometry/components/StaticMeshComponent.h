#pragma once

#include <string>

// gameplay-facing description of a static mesh entity
// contains only the asset path — no GPU handles, no pre-computed data
// StaticMeshBindingSystem reads this and drives MeshGpuComponent
// pair with MaterialReferenceComponent, or legacy MaterialComponent during migration
struct StaticMeshComponent
{
    std::string meshPath;
};
