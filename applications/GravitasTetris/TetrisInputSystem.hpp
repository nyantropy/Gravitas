#pragma once
#include "ECSControllerSystem.hpp"
#include "TetrisInputState.hpp"
#include "GtsKey.h"

// the system that handles all input for the tetris game
class TetrisInputSystem : public ECSControllerSystem
{
    private:
        TetrisInputState& input;

    public:
        TetrisInputSystem(TetrisInputState& in)
            : input(in) {}

        void update(ECSWorld& world, SceneContext& ctx) override
        {
            input.clear();

            if (ctx.input->isKeyPressed(GtsKey::A))
                input.moveLeft = true;

            if (ctx.input->isKeyPressed(GtsKey::D))
                input.moveRight = true;

            if (ctx.input->isKeyPressed(GtsKey::W))
                input.rotate = true;

            if (ctx.input->isKeyDown(GtsKey::S))
                input.softDrop = true;
        }
};
