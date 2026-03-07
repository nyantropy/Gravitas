#pragma once

// Bundles the three per-frame debounce accumulators used by TetrisGameSystem.
// Call advance(dt) once per tick to advance all three, then use the static
// ready() helper to test and consume individual timers on demand.
struct TickTimers
{
    float fall   = 0.0f;
    float move   = 0.0f;
    float rotate = 0.0f;

    // Advances all three timers by dt.
    void advance(float dt)
    {
        fall   += dt;
        move   += dt;
        rotate += dt;
    }

    // Returns true and resets 'timer' to zero if it has reached 'interval'.
    // Because it resets on the positive edge, repeated calls in the same frame
    // are safe — the second call will find timer == 0 and return false.
    static bool ready(float& timer, float interval)
    {
        if (timer >= interval)
        {
            timer = 0.0f;
            return true;
        }
        return false;
    }
};
