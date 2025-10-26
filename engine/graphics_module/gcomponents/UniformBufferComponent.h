#pragma once

#include <cstdint>

#include "UniformBufferResource.h"
#include "Types.h"

// a uniform buffer component only contains the id of the entities uniform buffers which are located in the resource manager
// and of course a ptr to the resource for convenience
struct UniformBufferComponent 
{
    uniform_id_type uniformID;
};
