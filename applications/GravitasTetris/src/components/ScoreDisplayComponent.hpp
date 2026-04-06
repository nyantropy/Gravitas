#pragma once

// Identifies which stats panel an entity displays.
// id = 0 : left sidebar  — SPEED LV and LINES
// id = 1 : right sidebar — SCORE and BEST
// TetrisScoreSystem branches on this value to write the correct text to each entity.
struct ScoreDisplayComponent
{
    int id = 0;
};
