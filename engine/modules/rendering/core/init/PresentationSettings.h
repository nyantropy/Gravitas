#pragma once

#include <array>
#include "types/EnumName.h"

enum class PresentModePreference
{
    // present without waiting for vertical refresh
    Immediate = 0,

    // wait for vertical refresh, but replace a pending frame with a newer one instead of building a queue
    Mailbox,

    // queue frames for vertical refresh, normal vsync behavior basically
    Fifo
};

inline constexpr std::array presentModePreferenceNames{
    gts::EnumName{PresentModePreference::Immediate, "immediate"},
    gts::EnumName{PresentModePreference::Mailbox, "mailbox"},
    gts::EnumName{PresentModePreference::Fifo, "fifo"}
};

// how completed frames are presented
struct PresentationSettings
{
    PresentModePreference mode = PresentModePreference::Fifo;
};
