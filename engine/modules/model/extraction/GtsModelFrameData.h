#pragma once

#include <memory>
#include <vector>
#include "model/realization/GtsRealizedGeometry.h"
#include "animation/skinning/GtsSkinPalette.h"
#include "MaterialTypes.h"

// Immutable frame values. Geometry is an aliasing reference into the shared
// realization; occurrenceOwner is an opaque lifetime/identity, never sampled by a backend.
struct GtsModelDraw
{
    std::shared_ptr<const void>                occurrenceOwner;
    uint32_t                                   occurrenceIndex = 0;
    std::shared_ptr<const GtsRealizedGeometry> geometry;
    uint32_t                                   primitiveIndex = 0;
    MaterialFrameState                         material;
    glm::mat4                                  worldFromGeometry{1};
    float                                      cameraDepth = 0;
};

struct GtsSkinnedDraw : GtsModelDraw
{
    uint32_t                              paletteSlot = 0;
    std::shared_ptr<const GtsSkinPalette> palette;
};

struct GtsModelFrameData
{
    view_id_type                cameraViewID = 0;
    std::vector<GtsModelDraw>   staticDraws;
    std::vector<GtsSkinnedDraw> skinnedDraws;
};
