#pragma once

#include <string>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "UITextComponent.h"
#include "HudMarkerComponent.h"
#include "DungeonGameStateComponent.h"

// Controller system — updates UITextComponent text each frame from game state.
// Runs after CombatSystem so the HUD always reflects the current frame's state.
class HudSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const auto& state = world.getSingleton<DungeonGameStateComponent>();

        const std::string hpStr =
            "HP  " + std::to_string(state.playerHp) +
            " OF " + std::to_string(state.playerMaxHp);

        std::string statusStr;
        if (state.result == DungeonGameResult::Won)
            statusStr = "VICTORY";
        else if (state.result == DungeonGameResult::Lost)
            statusStr = "GAME OVER";
        else
            statusStr = state.hasKey ? "KEY  YES" : "KEY  NO";

        world.forEach<HudMarkerComponent, UITextComponent>(
            [&](Entity, HudMarkerComponent& marker, UITextComponent& tc)
        {
            switch (marker.type)
            {
                case HudMarkerComponent::Type::Health:
                    tc.text = hpStr;
                    break;
                case HudMarkerComponent::Type::Status:
                    tc.text = statusStr;
                    break;
                case HudMarkerComponent::Type::Message:
                    tc.text    = state.message;
                    tc.visible = !state.message.empty();
                    break;
            }
        });
    }
};
