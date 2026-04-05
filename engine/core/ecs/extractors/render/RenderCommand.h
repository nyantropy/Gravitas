#pragma once

#include "GlmConfig.h"

#include "Types.h"

// Consumed by the renderer to produce one frame.
// All fields are value copies — no raw pointers into ECS component memory.
// objectSSBOSlot  : SSBO slot index assigned by a mesh binding system
// modelMatrix     : packed into ObjectUBO by the renderer
// cameraViewID    : selects the camera UBO already uploaded by CameraBindingSystem
struct RenderCommand
{
    mesh_id_type    meshID;
    texture_id_type textureID;
    ssbo_id_type    objectSSBOSlot;
    view_id_type    cameraViewID;

    glm::mat4 modelMatrix;
    float     alpha       = 1.0f;   // pushed to fragment shader; opaque objects use 1.0
    bool      doubleSided = false;  // selects the no-cull pipeline variant in SceneRenderStage
};
