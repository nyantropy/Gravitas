#pragma once

#include <cstdint>

#include "UniformBufferResource.h"
#include "UniformBufferObject.h"
#include "Types.h"

// a uniform buffer component only contains the id of the entities uniform buffers which are located in the resource manager
// and of course a ptr to the resource for convenience
struct UniformBufferComponent 
{
    // the id that points to the uniform resources
    uniform_id_type uniformID;

    // the uniform buffer object, that will eventually be uploaded to the shaders
    UniformBufferObject uniformBufferObject;
};
