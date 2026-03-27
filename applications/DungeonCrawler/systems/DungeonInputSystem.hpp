#pragma once

#include "ECSControllerSystem.hpp"
#include "InputActionManager.hpp"
#include "GtsKey.h"
#include "DungeonAction.h"
#include "DungeonInputComponent.h"

// Translates raw key presses into DungeonInputComponent singleton state.
// Maintains its own InputActionManager<DungeonAction> so dungeon-specific
// bindings stay local to this project and never touch the engine-level
// GtsAction enum.
//
// Must run before any system that reads DungeonInputComponent.
class DungeonInputSystem : public ECSControllerSystem
{
    InputActionManager<DungeonAction> actionManager;

    void initBindings()
    {
        actionManager.bind(DungeonAction::MoveForward,       GtsKey::W);
        actionManager.bind(DungeonAction::MoveBackward,      GtsKey::S);
        actionManager.bind(DungeonAction::StrafeLeft,        GtsKey::A);
        actionManager.bind(DungeonAction::StrafeRight,       GtsKey::D);
        actionManager.bind(DungeonAction::TurnLeft,          GtsKey::Q);
        actionManager.bind(DungeonAction::TurnRight,         GtsKey::E);
        actionManager.bind(DungeonAction::ToggleDebugCamera, GtsKey::T);
    }

public:
    DungeonInputSystem() { initBindings(); }

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        actionManager.update(*ctx.inputSource);

        auto& input = world.getSingleton<DungeonInputComponent>();

        // Reset all flags — systems downstream set them true if needed.
        input = DungeonInputComponent{};

        input.moveForward       = actionManager.isActionActive(DungeonAction::MoveForward);
        input.moveBackward      = actionManager.isActionActive(DungeonAction::MoveBackward);
        input.strafeLeft        = actionManager.isActionActive(DungeonAction::StrafeLeft);
        input.strafeRight       = actionManager.isActionActive(DungeonAction::StrafeRight);
        input.turnLeft          = actionManager.isActionActive(DungeonAction::TurnLeft);
        input.turnRight         = actionManager.isActionActive(DungeonAction::TurnRight);
        input.toggleDebugCamera = actionManager.isActionPressed(DungeonAction::ToggleDebugCamera);
    }
};
