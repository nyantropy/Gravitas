#pragma once

#include "GlmConfig.h"

#include "MaterialTypes.h"
#include "Types.h"

// Consumed by the renderer to produce one frame.
// All fields are value copies — no raw pointers into ECS component memory.
// objectSSBOSlot  : SSBO slot index assigned by a mesh binding system
// cameraViewID    : selects the camera UBO uploaded by the renderer this frame
struct RenderCommand
{
    mesh_id_type    meshID;
    texture_id_type textureID;
    ssbo_id_type    objectSSBOSlot;
    view_id_type    cameraViewID;
    MaterialInstanceHandle material;
    MaterialGpuHandle      materialGpu;

    MaterialBlendMode blendMode = MaterialBlendMode::Alpha;
    bool      doubleSided       = false; // selects the no-cull pipeline variant in SceneRenderStage
    bool      vertexColorOnly   = false; // uses mesh vertex color instead of sampling the material texture
    bool      depthWrite        = true;
};

struct ObjectUploadCommand
{
    ssbo_id_type objectSSBOSlot = 0;
    glm::mat4    modelMatrix    = glm::mat4(1.0f);
    glm::vec4    uvTransform    = {1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4    tint           = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct CameraUploadCommand
{
    view_id_type cameraViewID = 0;
    glm::mat4    viewMatrix   = glm::mat4(1.0f);
    glm::mat4    projMatrix   = glm::mat4(1.0f);
};
