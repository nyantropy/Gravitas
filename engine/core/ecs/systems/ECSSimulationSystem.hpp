#pragma once

#include "EcsSimulationContext.hpp"

// Pure deterministic system — runs at the fixed simulation tick rate.
// Suitable for physics, animation, and deterministic gameplay logic.
class ECSSimulationSystem
{
public:
    virtual ~ECSSimulationSystem() = default;
    virtual void update(const EcsSimulationContext& ctx) = 0;
};
