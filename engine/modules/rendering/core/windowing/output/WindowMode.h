#pragma once

#include <array>
#include "types/EnumName.h"

enum class WindowMode
{
    Windowed,             // Resizable window with title bar; freely draggable.
    BorderlessFullscreen, // Borderless window sized to the primary monitor;
                          // not draggable, does not minimise on focus loss.
    Fullscreen            // Exclusive fullscreen; minimises on focus loss.
};

inline constexpr std::array windowModeNames{
    gts::EnumName{WindowMode::Windowed, "windowed"},
    gts::EnumName{WindowMode::BorderlessFullscreen, "borderless_fullscreen"},
    gts::EnumName{WindowMode::Fullscreen, "fullscreen"}
};
