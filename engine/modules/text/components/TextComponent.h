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
//   - Set text/font/scale, leave dirty = true (default).
//   - TextBuildSystem (SimulationSystem) rebuilds quad geometry when dirty.
//   - TextBindingSystem (ControllerSystem) uploads the geometry to GPU and
//     feeds the atlas texture into RenderGpuComponent for the normal pipeline.
//   - Set dirty = true whenever text or font changes.
struct TextComponent
{
    std::string  text;
    BitmapFont*  font  = nullptr;
    float        scale = 1.0f;
    bool         dirty = true;
};
