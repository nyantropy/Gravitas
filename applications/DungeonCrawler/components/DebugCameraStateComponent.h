#pragma once

// Shared runtime flag for scenes that support a toggleable debug camera.
// Systems such as PlayerMovementSystem can read this to suppress gameplay
// controls while a free-fly debug camera is active.
struct DebugCameraStateComponent
{
    bool active = false;
};
