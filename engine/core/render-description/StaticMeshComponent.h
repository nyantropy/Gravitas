#pragma once

#include <string>

// Gameplay-facing description of a static mesh entity.
// Contains only the asset path — no GPU handles, no pre-computed data.
// StaticMeshBindingSystem reads this and drives MeshGpuComponent.
// Pair with MaterialComponent to fully describe a renderable entity.
struct StaticMeshComponent
{
    std::string meshPath;
};
