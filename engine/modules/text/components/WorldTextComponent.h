#pragma once

#include <string>

#include "BitmapFont.h"

// Gameplay-facing text description component.
// Attach to any entity that should display a string in world space.
// The entity also needs a TransformComponent that positions the text block.
//
// One entity = one text block (multi-line supported via '\n').
// Never create one entity per character.
//
// Workflow:
//   - Set text/font/scale.
//   - WorldTextCommandExtractor reads this component every frame, generates
//     glyph quads in local space, and transforms them to screen space via
//     the entity's world transform and the active camera's view-projection matrix.
struct WorldTextComponent
{
    std::string  text;
    BitmapFont*  font  = nullptr;
    float        scale = 1.0f;
};
