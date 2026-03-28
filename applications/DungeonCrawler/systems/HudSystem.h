#pragma once

#include <string>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "UiTree.h"
#include "UiTextDesc.h"
#include "HudMarkerComponent.h"
#include "DungeonGameStateComponent.h"

// Controller system — updates UiTree text elements each frame from game state.
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
            UiTextDesc desc;
            desc.font    = marker.font;
            desc.x       = marker.x;
            desc.y       = marker.y;
            desc.scale   = marker.scale;
            desc.visible = true;

            switch (marker.type)
            {
                case HudMarkerComponent::Type::Health:
                    desc.text = hpStr;
                    break;
                case HudMarkerComponent::Type::Status:
                    desc.text = statusStr;
                    break;
                case HudMarkerComponent::Type::Message:
                    desc.text    = state.message;
                    desc.visible = !state.message.empty();
                    break;
            }

            ctx.ui->update(marker.uiHandle, desc);
        });
    }
};
