#pragma once
#include "ECSControllerSystem.hpp"
#include "TetrisInputComponent.hpp"
#include "GtsKey.h"

// the system that handles all input for the tetris game
class TetrisInputSystem : public ECSControllerSystem
{
    public:
        TetrisInputSystem() {}

        void update(ECSWorld& world, SceneContext& ctx) override
        {
            auto& input = world.getSingleton<TetrisInputComponent>();

            if (ctx.input->isKeyDown(GtsKey::A))
                input.moveLeft = true;

            if (ctx.input->isKeyDown(GtsKey::D))
                input.moveRight = true;

            if (ctx.input->isKeyDown(GtsKey::W))
                input.rotate = true;

            if (ctx.input->isKeyDown(GtsKey::S))
                input.softDrop = true;
        }
};
