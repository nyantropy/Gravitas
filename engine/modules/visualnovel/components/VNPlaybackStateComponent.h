#pragma once

#include "VNRuntime.h"

namespace gts::vn
{
    struct VNPlaybackStateComponent
    {
        bool active = false;
        bool blockingGameplayInput = false;
        bool waitingForInput = false;
        bool waitingForChoice = false;
        bool waitingForAnimation = false;
        VNRuntimeStatus status = VNRuntimeStatus::Idle;
    };
} // namespace gts::vn
