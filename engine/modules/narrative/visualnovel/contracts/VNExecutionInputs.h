#pragma once

#include "EcsExecutionSelection.h"

namespace gts::vn
{
    struct VNExecutionInputs
    {
        EcsExecutionSelection defaultSelection;
        EcsExecutionSelection overlay;
        EcsExecutionSelection fullscreen;
    };
} // namespace gts::vn
