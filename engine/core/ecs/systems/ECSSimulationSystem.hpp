#pragma once

#include <string_view>

#include "EcsSimulationContext.hpp"

// Pure deterministic system — runs at the fixed simulation tick rate.
// Suitable for physics, animation, and deterministic gameplay logic.
class ECSSimulationSystem
{
public:
    virtual ~ECSSimulationSystem() = default;
    virtual void update(const EcsSimulationContext& ctx) = 0;

    std::string_view getName() const
    {
        return debugName;
    }

    void setDebugName(std::string_view name)
    {
        debugName = name;
    }

private:
    std::string_view debugName = "ECSSimulationSystem";
};
