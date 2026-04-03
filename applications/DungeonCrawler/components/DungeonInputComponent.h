#pragma once

// Singleton component written by DungeonInputSystem each frame.
// All systems that need player input read from this instead of
// querying the action manager directly.
struct DungeonInputComponent
{
    bool moveForward        = false;
    bool moveBackward       = false;
    bool strafeLeft         = false;
    bool strafeRight        = false;
    bool turnLeft           = false;
    bool turnRight          = false;
    bool toggleDebugCamera  = false; // edge-triggered (pressed this frame only)
    bool attackPressed      = false; // edge-triggered (pressed this frame only)
    bool regeneratePressed  = false; // edge-triggered (pressed this frame only)
};
