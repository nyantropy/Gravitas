#pragma once

#include <cstdint>

// finally, a use case for a bitmask <3
enum class TransformAnimationMode : uint32_t
{
    None      = 0,
    Rotate    = 1 << 0,
    Translate = 1 << 1,
    Scale     = 1 << 2
};
