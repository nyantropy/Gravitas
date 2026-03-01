#pragma once

#include "ECSControllerSystem.hpp"
#include "InputActionManager.hpp"
#include "TetrisInputComponent.hpp"
#include "TetrisAction.h"

// Translates raw key presses into the abstract TetrisInputComponent state.
// Maintains its own InputActionManager<TetrisAction> so tetris-specific
// bindings stay local to this project and never touch the engine-level
// GtsAction enum.
class TetrisInputSystem : public ECSControllerSystem
{
    InputActionManager<TetrisAction> actionManager;

    void initBindings()
    {
        actionManager.bind(TetrisAction::MoveLeft,  GtsKey::A);
        actionManager.bind(TetrisAction::MoveRight, GtsKey::D);
        actionManager.bind(TetrisAction::RotateCW,  GtsKey::E);
        actionManager.bind(TetrisAction::RotateCCW, GtsKey::Q);
        actionManager.bind(TetrisAction::HardDrop,  GtsKey::Space);
        actionManager.bind(TetrisAction::SoftDrop,  GtsKey::S);
    }

public:
    TetrisInputSystem()
    {
        initBindings();
    }

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        actionManager.update(*ctx.inputSource);

        auto& input = world.getSingleton<TetrisInputComponent>();

        if (actionManager.isActionActive(TetrisAction::MoveLeft))
            input.moveLeft = true;

        if (actionManager.isActionActive(TetrisAction::MoveRight))
            input.moveRight = true;

        // Rotation: held + timer-debounced in TetrisGameSystem for key-repeat feel
        if (actionManager.isActionActive(TetrisAction::RotateCW))
            input.rotateCW = true;

        if (actionManager.isActionActive(TetrisAction::RotateCCW))
            input.rotateCCW = true;

        // Hard drop: single-shot on first press frame only (no repeat while held)
        if (actionManager.isActionPressed(TetrisAction::HardDrop))
            input.hardDrop = true;

        if (actionManager.isActionActive(TetrisAction::SoftDrop))
            input.softDrop = true;
    }
};
