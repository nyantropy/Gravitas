#pragma once

#include <cstdint>
#include "ObjectUBO.h"

struct ObjectGpuComponent
{
    uint32_t objectSSBOIndex;
    ObjectUBO ubo;
    bool dirty;
};
