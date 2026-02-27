#pragma once

#include <vector>

#include "Vertex.h"
#include "Types.h"

// Internal component managed exclusively by TextBuildSystem and TextBindingSystem.
// Do not write to this from gameplay or visual code.
//
// TextBuildSystem fills vertices/indices from TextComponent geometry each time
// the text is dirty.  TextBindingSystem uploads them to a GPU mesh and stores
// the resulting ID here so subsequent updates know which buffer to overwrite.
struct TextMeshComponent
{
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    mesh_id_type meshId   = 0;     // 0 = not yet uploaded; set by TextBindingSystem
    bool         gpuDirty = false; // TextBuildSystem sets true after rebuilding geometry
};
