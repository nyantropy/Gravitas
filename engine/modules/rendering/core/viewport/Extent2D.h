#pragma once

#include <cstdint>

// simple struct to hold width/height
struct Extent2D
{
    uint32_t width = 1;
    uint32_t height = 1;

    bool operator==(const Extent2D&) const = default;
};
