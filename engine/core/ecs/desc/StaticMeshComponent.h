#pragma once

#include <string>

// gameplay-facing description of a static mesh entity
// contains only the asset path — no GPU handles, no pre-computed data
// StaticMeshBindingSystem reads this and drives MeshGpuComponent
// pair with MaterialComponent to fully describe a renderable entity
struct StaticMeshComponent
{
    std::string meshPath;
};
