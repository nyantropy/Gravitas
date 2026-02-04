#pragma once

#include "Types.h"
#include "UniformBufferObject.h"

// the render command will be consumed by the renderer to output a frame
struct RenderCommand 
{
    // it needs the ids of mesh, texture and the uniform resource
    mesh_id_type meshID;
    texture_id_type textureID;
    uniform_id_type uniformID;

    // as well as a ptr to the actual uniform buffer object, which will be owned by the component
    // this struct basically serves as a "bridge" between ecs and the rendering system
    UniformBufferObject* uboPtr;
};