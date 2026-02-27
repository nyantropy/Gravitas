#pragma once

#include <glm.hpp>

#include "Types.h"

// Consumed by the renderer to produce one frame.
// All fields are value copies — no raw pointers into ECS component memory.
// objectSSBOSlot  : SSBO slot index assigned by RenderBindingSystem
// modelMatrix     : packed into ObjectUBO by the renderer
// viewMatrix/proj : packed into CameraUBO by the renderer (mirrors the object side)
struct RenderCommand
{
    mesh_id_type    meshID;
    texture_id_type textureID;
    ssbo_id_type    objectSSBOSlot;
    view_id_type    cameraViewID;

    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
};
