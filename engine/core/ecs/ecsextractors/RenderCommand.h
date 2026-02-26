#pragma once

#include <glm.hpp>

#include "Types.h"
#include "CameraUBO.h"

// Consumed by the renderer to produce one frame.
// objectSSBOSlot : index into the shared object SSBO; assigned by RenderBindingSystem.
// modelMatrix    : CPU-side model matrix; the renderer packs this into ObjectUBO before writing.
// cameraUboPtr   : pointer into CameraGpuComponent::ubo; renderer uploads it to the camera UBO.
struct RenderCommand
{
    mesh_id_type    meshID;
    texture_id_type textureID;
    ssbo_id_type    objectSSBOSlot;
    view_id_type    cameraViewID;

    glm::mat4  modelMatrix;  // value copy — no raw pointer into ECS component memory
    CameraUBO* cameraUboPtr; // camera side; owned by CameraGpuComponent
};
