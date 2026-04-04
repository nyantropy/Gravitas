#pragma once

// Base class for data passed between scenes at transition time.
// Derive from this in game code to carry scene-specific state.
// Lifetime is owned by the transition command until the destination scene's
// onLoad() consumes it.
struct GtsSceneTransitionData
{
    virtual ~GtsSceneTransitionData() = default;
};
