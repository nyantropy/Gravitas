#pragma once

// Gameplay-facing description of a runtime-generated flat quad mesh.
// ProceduralMeshBindingSystem reads this and drives MeshGpuComponent.
// Pair with MaterialComponent to fully describe a renderable entity.
//
// width and height are in world-space units.
// The quad is axis-aligned in the XY plane, centered at the entity origin.
// Apply rotation via TransformComponent to orient as needed.
struct ProceduralMeshComponent
{
    float width  = 1.0f;
    float height = 1.0f;
};
