#pragma once

#include <string>

// Gameplay-facing description of what an entity should look like.  Contains only
// asset paths — no GPU handles, no pre-computed data.
// RenderBindingSystem reads this and drives RenderGpuComponent.
struct RenderDescriptionComponent
{
    std::string meshPath;
    std::string texturePath;
};
