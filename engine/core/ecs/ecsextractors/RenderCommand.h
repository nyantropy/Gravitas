#pragma once

#include "Types.h"
#include "ObjectUBO.h"
#include "CameraUBO.h"

// the render command will be consumed by the renderer to output a frame
struct RenderCommand
{
    mesh_id_type    meshID;
    texture_id_type textureID;
    uint32_t        objectSSBOIndex; // slot in the shared object SSBO
    view_id_type    cameraViewID;

    // typed pointers to the CPU-side UBO data, owned by their respective components
    ObjectUBO* objectUboPtr;
    CameraUBO* cameraUboPtr;
};
