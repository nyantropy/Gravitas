#pragma once

class ECSWorld;
struct InputSnapshot;

// Context passed to ECSSimulationSystem::update each fixed-timestep tick.
// Contains only data that is independent of frame timing and rendering.
//
// The same InputSnapshot is reused for all ticks within a single frame.
// input may be null on the first frame before input has been sampled.
//
// Ordering note: simulation systems run in registration order. Structural
// world mutations (entity creation/deletion, component add/remove) take
// effect immediately within the same tick and are visible to subsequently
// registered systems. Determinism therefore depends on a stable, documented
// system registration order. Systems should only write components they own.
struct EcsSimulationContext
{
    ECSWorld&               world;
    float                   dt;             // fixed timestep delta
    const InputSnapshot*    input = nullptr; // sampled once per frame, same for all ticks
};
