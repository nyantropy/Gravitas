#pragma once

#include <memory>
#include <vector>
#include "assets/processing/geometry/skinned/GtsPreparedSkinnedMesh.h"
#include "animation/skinning/GtsSkinPalette.h"
#include "MaterialTypes.h"

struct GtsSkinnedMeshPart
{
    GtsPreparedSkinnedMesh mesh;
    uint32_t bindingIndex = 0;
};

// Shared immutable render inputs. No skeleton, clip, playback or backend state.
struct GtsSkinnedModelData
{
    std::vector<GtsSkinnedMeshPart> parts;
    std::vector<MaterialFrameState> materials;
    uint32_t bindingCount = 0;
};

struct GtsSkinnedModelInstance
{
    std::shared_ptr<const GtsSkinnedModelData> model;
};

struct GtsSkinnedDraw
{
    std::shared_ptr<const GtsSkinnedModelInstance> instance;
    std::shared_ptr<const std::vector<GtsSkinPalette>> palettes;
    glm::mat4 worldFromReference{1};
};

struct SkinnedFrameData
{
    view_id_type cameraViewID = 0;
    std::vector<GtsSkinnedDraw> draws;
};
