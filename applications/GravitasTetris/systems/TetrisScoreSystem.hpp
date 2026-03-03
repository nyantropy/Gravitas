#pragma once

#include <cstdio>
#include <string>
#include <algorithm>

#include "ECSSimulationSystem.hpp"
#include "TetrisScoreComponent.hpp"
#include "ScoreDisplayComponent.hpp"
#include "TextComponent.h"
#include "SpeedHelper.hpp"

// Drains the pending-events inbox each tick, updates all stats, then rewrites
// the text on every entity carrying a ScoreDisplayComponent.
//
// Stats written to the display (8 lines, label + value pairs):
//   SPEED LV / {level}    — current speed level from SpeedHelper
//   LINES    / {lines}    — total lines cleared this run; reset on game over
//   SCORE    / {score}    — current score; reset on game over
//   BEST     / {hiScore}  — session high score; never reset
class TetrisScoreSystem : public ECSSimulationSystem
{
    private:
        // Classic Tetris points for 1-4 simultaneous line clears.
        static constexpr int LINE_POINTS[4] = { 100, 300, 500, 800 };

    public:
        void update(ECSWorld& world, float) override
        {
            auto& sc = world.getSingleton<TetrisScoreComponent>();
            if (sc.pendingEvents.empty())
                return;

            bool statsChanged = false;
            for (const ScoringEvent& ev : sc.pendingEvents)
            {
                switch (ev.type)
                {
                    case ScoringEventType::LinesCleared:
                    {
                        const int idx = std::min(ev.linesCleared, 4) - 1;
                        sc.score += LINE_POINTS[idx];
                        sc.lines += ev.linesCleared;
                        if (sc.score > sc.highScore)
                            sc.highScore = sc.score;
                        statsChanged = true;
                        break;
                    }
                    case ScoringEventType::GameOver:
                        // Update high score before resetting the run.
                        if (sc.score > sc.highScore)
                            sc.highScore = sc.score;
                        sc.score = 0;
                        sc.lines = 0;
                        statsChanged = true;
                        break;
                }
            }
            sc.pendingEvents.clear();

            if (!statsChanged)
                return;

            const int speedLevel = computeSpeedLevel(sc.score).level;

            char lvlBuf[8];
            char linesBuf[8];
            char scoreBuf[12];
            char bestBuf[12];
            std::snprintf(lvlBuf,   sizeof(lvlBuf),   "%d",   speedLevel);
            std::snprintf(linesBuf, sizeof(linesBuf),  "%04d", sc.lines);
            std::snprintf(scoreBuf, sizeof(scoreBuf),  "%08d", sc.score);
            std::snprintf(bestBuf,  sizeof(bestBuf),   "%08d", sc.highScore);

            const std::string newText =
                std::string("SPEED LV\n") + lvlBuf    + "\n" +
                std::string("LINES\n")    + linesBuf   + "\n" +
                std::string("SCORE\n")    + scoreBuf   + "\n" +
                std::string("BEST\n")     + bestBuf;

            world.forEach<ScoreDisplayComponent, TextComponent>(
                [&](Entity, ScoreDisplayComponent&, TextComponent& text)
                {
                    text.text  = newText;
                    text.dirty = true;
                });
        }
};
