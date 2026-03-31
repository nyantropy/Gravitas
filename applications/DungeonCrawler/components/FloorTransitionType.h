#pragma once

// The type of floor transition. Each type has its own animation handler
// in FloorTransitionSystem.
enum class FloorTransitionType
{
    Stairs,   // bidirectional, camera arc animation along ramp angle
    Hole,     // one-way downward, camera falls straight down
    Collapse, // one-way downward, camera shakes then drops
};
