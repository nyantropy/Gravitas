#pragma once

#include "Types.h"
#include "CameraUBO.h"

struct CameraGpuComponent
{
    uniform_id_type buffer;
    CameraUBO ubo;
    bool dirty;
};
