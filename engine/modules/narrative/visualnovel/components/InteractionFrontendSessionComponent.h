#pragma once

#include <cstdint>
#include <string>

namespace gts::vn
{
    enum class InteractionFrontendMode : uint8_t
    {
        None = 0,
        Dialogue,
        MerchantTrade,
        Inventory,
        Quest,
        Crafting,
        Training,
        Banking,
        Gifting
    };

    struct InteractionFrontendSessionComponent
    {
        bool active = false;
        InteractionFrontendMode mode = InteractionFrontendMode::None;
        std::string npcId;
        std::string npcName;
        std::string merchantId;
        std::string speakerName;
        std::string feedbackText;
        uint64_t revision = 1;

        bool isInteractionViewActive() const
        {
            return active && mode != InteractionFrontendMode::None && mode != InteractionFrontendMode::Dialogue;
        }

        void clear()
        {
            active = false;
            mode = InteractionFrontendMode::None;
            npcId.clear();
            npcName.clear();
            merchantId.clear();
            speakerName.clear();
            feedbackText.clear();
            ++revision;
        }
    };
} // namespace gts::vn
