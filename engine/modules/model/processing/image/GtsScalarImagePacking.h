#pragma once
#include <array>
#include <optional>
#include "GtsImageDecode.h"

struct GtsScalarImageChannel
{
    uint32_t                 width;
    uint32_t                 height;
    std::span<const uint8_t> rgba8Pixels;
    uint32_t                 channel;
};
// Absent channels are 255. Inputs must have identical dimensions; no resampling.
bool packGtsScalarImages(const std::array<std::optional<GtsScalarImageChannel>, 4>& channels,
                         GtsDecodedImage&                                           result,
                         std::string*                                               error = nullptr);
