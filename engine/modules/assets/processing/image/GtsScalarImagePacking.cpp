#include "GtsScalarImagePacking.h"
#include <limits>
bool packGtsScalarImages(const std::array<std::optional<GtsScalarImageChannel>, 4>& channels,
                         GtsDecodedImage&                                           result,
                         std::string*                                               error)
{
    GtsDecodedImage packed;
    for (const auto& channel : channels)
    {
        if (!channel)
            continue;
        const auto& input = *channel;
        if (!input.width || !input.height || input.channel > 3 ||
            uint64_t(input.width) * input.height > std::numeric_limits<size_t>::max() / 4 ||
            input.rgba8Pixels.size() != uint64_t(input.width) * input.height * 4)
        {
            if (error)
                *error = "Invalid scalar image channel";
            return false;
        }
        if (packed.width && (packed.width != input.width || packed.height != input.height))
        {
            if (error)
                *error = "Scalar images must have equal dimensions; resampling is not supported";
            return false;
        }
        packed.width  = input.width;
        packed.height = input.height;
    }
    if (!packed.width)
    {
        if (error)
            *error = "Scalar packing requires at least one image";
        return false;
    }
    packed.sourceChannelCount = 4;
    packed.rgba8Pixels.assign(static_cast<size_t>(packed.width) * packed.height * 4, 255);
    for (size_t pixel = 0; pixel < packed.rgba8Pixels.size(); pixel += 4)
        for (size_t output = 0; output < 4; ++output)
            if (channels[output])
                packed.rgba8Pixels[pixel + output] = channels[output]->rgba8Pixels[pixel + channels[output]->channel];
    result = std::move(packed);
    return true;
}
