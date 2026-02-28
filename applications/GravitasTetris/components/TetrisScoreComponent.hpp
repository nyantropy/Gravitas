#pragma once

#include <vector>
#include "TetrisScoringEvent.hpp"

// Singleton component. Holds the live score and an inbox of scoring events
// produced by gameplay systems each simulation tick.
struct TetrisScoreComponent
{
    int                       score = 0;
    std::vector<ScoringEvent> pendingEvents;
};
