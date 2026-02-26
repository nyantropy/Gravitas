#pragma once

#include <string>

// Gameplay-facing description of what an entity should look like.
// Contains no GPU handles, no resource IDs, no renderer state —
// only the logical intent that gameplay and visual systems express.
//
// RenderBindingSystem reads this component and is solely responsible for
// resolving the paths to GPU resource IDs and maintaining RenderableComponent.
struct RenderDescriptionComponent
{
    std::string meshPath;
    std::string texturePath;
};
