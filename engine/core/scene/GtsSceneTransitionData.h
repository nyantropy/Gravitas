#pragma once

// Base class for data passed between scenes at transition time.
// Derive from this in game code to carry scene-specific state.
// Lifetime: stack-allocated by the caller, read once in onLoad, not stored.
struct GtsSceneTransitionData
{
    virtual ~GtsSceneTransitionData() = default;
};
