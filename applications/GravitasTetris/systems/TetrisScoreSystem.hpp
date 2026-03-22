#pragma once

#include <cstdio>
#include <string>
#include <algorithm>

#include "ECSSimulationSystem.hpp"
#include "TetrisScoreComponent.hpp"
#include "ScoreDisplayComponent.hpp"
#include "WorldTextComponent.h"
#include "SpeedHelper.hpp"

// Drains the pending-events inbox each tick, updates all stats, then rewrites
// the text on every entity carrying a ScoreDisplayComponent.
//
// Panel routing via ScoreDisplayComponent::id:
//   id == 0  (left sidebar) : SPEED LV / {level}  and  LINES / {lines}
//   id == 1  (right sidebar): SCORE / {score}      and  BEST  / {hiScore}
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

            const int speedLevel = SpeedHelper::computeSpeedLevel(sc.score).level;

            char lvlBuf[8];
            char linesBuf[8];
            char scoreBuf[12];
            char bestBuf[12];
            std::snprintf(lvlBuf,   sizeof(lvlBuf),   "%d",   speedLevel);
            std::snprintf(linesBuf, sizeof(linesBuf),  "%04d", sc.lines);
            std::snprintf(scoreBuf, sizeof(scoreBuf),  "%08d", sc.score);
            std::snprintf(bestBuf,  sizeof(bestBuf),   "%08d", sc.highScore);

            const std::string leftText =
                std::string("LEVEL\n") + lvlBuf  + "\n" +
                std::string("LINES\n")    + linesBuf;

            const std::string rightText =
                std::string("SCORE\n") + scoreBuf + "\n" +
                std::string("BEST\n")  + bestBuf;

            world.forEach<ScoreDisplayComponent, WorldTextComponent>(
                [&](Entity, ScoreDisplayComponent& sdc, WorldTextComponent& text)
                {
                    if (sdc.id == 0)
                    {
                        // Left panel: SPEED LV and LINES
                        text.text  = leftText;
                    }
                    else
                    {
                        // Right panel: SCORE and BEST
                        text.text  = rightText;
                    }
                    text.dirty = true;
                });
        }
};
