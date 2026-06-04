#pragma once

#include "InputBindingRegistry.h"

class ECSWorld;

// context that passes into simulation systems
// contains only data that is independent of frame timing and rendering

// ordering note: simulation systems run in registration order, structural world mutations
// like entity creation or deletion, or adjustment of components are visible to subsequent systems
// in the same tick, but commands queued through ctx.world.commands() are only flushed after the current system

// that means that for determinism the order of system registration is extremely important
// simulation systems should also only write components they own
struct EcsSimulationContext
{
    ECSWorld&               world;
    float                   dt;             // fixed timestep delta
    const InputBindingRegistry* input = nullptr;
};
