#pragma once

#include "Types.h"
#include "ObjectUBO.h"

struct ObjectGpuComponent
{
    uniform_id_type buffer;
    ObjectUBO ubo;
    bool dirty;
};
