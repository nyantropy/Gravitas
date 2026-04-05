#pragma once

#include "helpers/BoardController.hpp"
#include "components/TetrisScoreComponent.hpp"
#include "ECSWorld.hpp"

// Handles game-over transitions: fires the scoring event, wipes the playfield,
// and rebuilds the grid so the next spawn starts with a clean board.
struct GameStateController
{
    void triggerGameOver(ECSWorld& world, BoardController& board)
    {
        auto& sc = world.getSingleton<TetrisScoreComponent>();
        sc.pendingEvents.push_back({ ScoringEventType::GameOver });
        board.wipePlayfield(world);
        board.rebuild(world);
    }
};
