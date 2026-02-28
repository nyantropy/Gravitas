#pragma once

// finally, a use case for a bitmask <3
enum class AnimationMode 
{
    None = 0,
    Rotate = 1 << 0,
    Translate = 1 << 1,
    Scale = 1 << 2
};