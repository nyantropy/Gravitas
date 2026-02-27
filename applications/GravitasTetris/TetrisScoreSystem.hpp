#pragma once

#include <cstdio>
#include <string>
#include <algorithm>

#include "ECSSimulationSystem.hpp"
#include "TetrisScoreComponent.hpp"
#include "ScoreDisplayComponent.hpp"
#include "TextComponent.h"

// Drains the pending-events inbox each tick, updates the score, then
// rewrites the text on every entity carrying a ScoreDisplayComponent.
class TetrisScoreSystem : public ECSSimulationSystem
{
    private:
        // Classic Tetris points for 1-4 simultaneous line clears.
        static constexpr int LINE_POINTS[4] = { 100, 300, 500, 800 };

    public:
        void update(ECSWorld& world, float dt) override
        {
            auto& sc = world.getSingleton<TetrisScoreComponent>();
            if (sc.pendingEvents.empty())
                return;

            bool scoreChanged = false;
            for (const ScoringEvent& ev : sc.pendingEvents)
            {
                switch (ev.type)
                {
                    case ScoringEventType::LinesCleared:
                    {
                        int idx = std::min(ev.linesCleared, 4) - 1;
                        sc.score += LINE_POINTS[idx];
                        scoreChanged = true;
                        break;
                    }
                    case ScoringEventType::GameOver:
                        sc.score = 0;
                        scoreChanged = true;
                        break;
                }
            }
            sc.pendingEvents.clear();

            if (!scoreChanged)
                return;

            char buf[9];
            std::snprintf(buf, sizeof(buf), "%08d", sc.score);
            std::string newText = "SCORE\n" + std::string(buf);

            world.forEach<ScoreDisplayComponent, TextComponent>(
                [&](Entity, ScoreDisplayComponent&, TextComponent& text)
                {
                    text.text  = newText;
                    text.dirty = true;
                });
        }
};
