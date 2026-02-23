#pragma once

#include "Types.h"
#include "CameraUBO.h"

struct CameraGpuComponent
{
    view_id_type viewID;
    CameraUBO ubo;
    bool dirty;
};
