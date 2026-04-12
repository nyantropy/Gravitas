#pragma once

#include "EcsControllerContext.hpp"

// Per-frame system with access to all frame-dependent dependencies.
// Suitable for input handling, camera, rendering prep, and UI updates.
class ECSControllerSystem
{
public:
    virtual ~ECSControllerSystem() = default;
    virtual void update(const EcsControllerContext& ctx) = 0;
};
