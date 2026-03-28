#pragma once

#include <string>

// Description component for a world-space flat quad rendered with a texture.
// WorldImageBindingSystem reads this and drives a RenderGpuComponent on the
// same entity — no manual mesh or GPU resource management needed.
//
// width and height are in world-space units (matching TransformComponent scale).
// Set them to produce the correct aspect ratio for the source image.
struct WorldImageComponent
{
    std::string texturePath;
    float       width  = 1.0f;
    float       height = 1.0f;

    // --- internal tracking; managed by WorldImageBindingSystem ---
    std::string _boundTexturePath;
    float       _boundWidth  = 0.0f;
    float       _boundHeight = 0.0f;
};
