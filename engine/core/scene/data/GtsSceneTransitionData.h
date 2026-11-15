#pragma once

// Base class for data passed between scenes at transition time.
// Derive from this in game code to carry scene-specific state.
// Lifetime is owned by the transition command until the destination scene's
// onLoad() consumes it.
// Application integration owns derived transport envelopes. Gameplay/domain
// values remain independent and are copied or moved into those envelopes.
struct GtsSceneTransitionData
{
    virtual ~GtsSceneTransitionData() = default;
};
