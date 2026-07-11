#pragma once

#include "GlmConfig.h"

#include "LightingFrameData.h"
#include "MaterialTypes.h"
#include "Types.h"

// Consumed by the renderer to produce one frame.
// All fields are value copies — no raw pointers into ECS component memory.
// objectSSBOSlot  : SSBO slot index assigned by a mesh binding system
// cameraViewID    : selects the camera UBO uploaded by the renderer this frame
struct RenderCommand
{
    mesh_id_type meshID = 0;
    ssbo_id_type objectSSBOSlot = 0;
    view_id_type cameraViewID = 0;
    MaterialInstanceHandle material;
    MaterialGpuHandle materialGpu;
    MaterialVariantKey variantKey{};
    RenderQueue renderQueue = RenderQueue::Opaque;
    uint64_t sortKey = 0;
};

struct ObjectUploadCommand
{
    ssbo_id_type objectSSBOSlot = 0;
    glm::mat4    modelMatrix    = glm::mat4(1.0f);
    glm::vec4    uvTransform    = {1.0f, 1.0f, 0.0f, 0.0f};
};

struct CameraUploadCommand
{
    view_id_type cameraViewID      = 0;
    glm::mat4    viewMatrix        = glm::mat4(1.0f);
    glm::mat4    projMatrix        = glm::mat4(1.0f);
    glm::vec3    cameraWorldPosition = {0.0f, 0.0f, 0.0f};
    gts::rendering::LightingFrameData lighting =
        gts::rendering::defaultLightingFrameData();
};
