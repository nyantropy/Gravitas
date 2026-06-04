#pragma once

#include "InputBindingRegistry.h"

class ECSWorld;

// context that passes into simulation systems
// contains only data that is independent of frame timing and rendering

// ordering note: simulation systems run in registration order, structural world mutations
// like entity creation or deletion, or adjustment of compoents take effect immediately
// within the same tick and are visible to subsequent systems

// that means that for determinism the order of system registration is extremely important
// simulation systems should also only write compoents they own
struct EcsSimulationContext
{
    ECSWorld&               world;
    float                   dt;             // fixed timestep delta
    const InputBindingRegistry* input = nullptr;
};
