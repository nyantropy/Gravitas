#pragma once

#include "Types.h"
#include "ObjectUBO.h"
#include "CameraUBO.h"

// the render command will be consumed by the renderer to output a frame
struct RenderCommand 
{
    // ids of the mesh, texture, and the two separate uniform buffer resources
    mesh_id_type meshID;
    texture_id_type textureID;
    uniform_id_type objectUniformID;
    uniform_id_type cameraUniformID;

    // typed pointers to the CPU-side UBO data, owned by their respective components
    ObjectUBO* objectUboPtr;
    CameraUBO* cameraUboPtr;
};
