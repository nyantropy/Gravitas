#pragma once

enum class ScoringEventType
{
    LinesCleared,
    GameOver
};

struct ScoringEvent
{
    ScoringEventType type;
    int              linesCleared = 0; // only meaningful for LinesCleared events
};
