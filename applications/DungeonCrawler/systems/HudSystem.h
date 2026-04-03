#pragma once

#include <string>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "UiSystem.h"
#include "HudMarkerComponent.h"
#include "DungeonGameStateComponent.h"

// Controller system — updates retained UI text nodes each frame from game state.
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

        world.forEach<HudMarkerComponent>(
            [&](Entity, HudMarkerComponent& marker)
        {
            std::string text;
            bool visible = true;

            switch (marker.type)
            {
                case HudMarkerComponent::Type::Health:
                    text = hpStr;
                    break;
                case HudMarkerComponent::Type::Status:
                    text = statusStr;
                    break;
                case HudMarkerComponent::Type::Message:
                    text = state.message;
                    visible = !state.message.empty();
                    break;
            }

            ctx.ui->setState(marker.uiHandle, UiStateFlags{
                .visible = visible,
                .enabled = false,
                .interactable = false
            });
            ctx.ui->setPayload(marker.uiHandle, UiTextData{
                text,
                {},
                {1.0f, 1.0f, 1.0f, 1.0f},
                marker.scale
            });
        });
    }
};
