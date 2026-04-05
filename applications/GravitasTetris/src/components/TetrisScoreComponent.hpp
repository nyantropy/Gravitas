#pragma once

#include <vector>
#include "struct/TetrisScoringEvent.hpp"

// Singleton component. Holds the live score, run stats, and an inbox of scoring
// events produced by gameplay systems each simulation tick.
//
// highScore is session-only: survives game-over resets but is never persisted to disk.
struct TetrisScoreComponent
{
    int score     = 0;
    int highScore = 0;  // updated whenever score exceeds it; never reset on game over
    int lines     = 0;  // total lines cleared this run; reset to 0 on game over

    std::vector<ScoringEvent> pendingEvents;
};
