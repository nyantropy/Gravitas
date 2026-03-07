#pragma once

// Bundles the three per-frame debounce accumulators used by TetrisGameSystem.
struct TickTimers
{
    float fall   = 0.0f;
    float move   = 0.0f;
    float rotate = 0.0f;

    void advance(float dt) { fall += dt; move += dt; rotate += dt; }

    // Resets all movement timers so a newly-spawned piece cannot inherit
    // held-key state from the piece that just locked.
    void reset() { fall = 0.0f; move = 0.0f; rotate = 0.0f; }

    // Returns true and resets 'timer' to zero when it has reached 'interval'.
    static bool ready(float& timer, float interval)
    {
        if (timer >= interval) { timer = 0.0f; return true; }
        return false;
    }
};
