#pragma once

#include <string>

// Gameplay-facing description of what an entity should look like.  Contains only
// asset paths — no GPU handles, no pre-computed data.
// RenderBindingSystem reads this and drives RenderGpuComponent.
struct RenderDescriptionComponent
{
    std::string meshPath;
    std::string texturePath;
    float       alpha      = 1.0f;   // 1.0 = fully opaque; < 1.0 for transparent rendering (e.g. ghost blocks)
    bool        doubleSided = false;  // true = no backface culling (planes, quads viewed from both sides)
};
