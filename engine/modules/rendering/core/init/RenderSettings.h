#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "../viewport/Extent2D.h"

enum class RenderResolutionMode
{
    MatchOutput = 0,
    Fixed = 1,
    ScaleOutput = 2
};

struct RenderResolutionSettings
{
    RenderResolutionMode mode = RenderResolutionMode::MatchOutput;
    Extent2D fixedExtent = {1920, 1080};
    float scale = 1.0f;
};

// how the scene is rendered
struct RenderSettings
{
    RenderResolutionSettings resolution;

    // enable frustum culling - entities without a BoundsComponent are never culled regardless of this flag
    // set to false to disable culling globally (useful for debugging)
    // belongs here because it is not part of the main draw pipeline as of right now, instead being a cpu pass before drawcalls execute
    bool frustumCullingEnabled = true;
};

inline std::optional<Extent2D> resolveRenderExtent(const RenderResolutionSettings& settings,
                                                Extent2D output)
{
    if (output.width == 0 || output.height == 0)
        return std::nullopt;

    switch (settings.mode)
    {
        case RenderResolutionMode::MatchOutput:
            return output;
        case RenderResolutionMode::Fixed:
            if (settings.fixedExtent.width == 0 || settings.fixedExtent.height == 0)
                return std::nullopt;
            return settings.fixedExtent;
        case RenderResolutionMode::ScaleOutput:
        {
            if (!std::isfinite(settings.scale) || settings.scale <= 0.0f)
                return std::nullopt;
            const double width = std::max(1.0, std::round(output.width * double(settings.scale)));
            const double height = std::max(1.0, std::round(output.height * double(settings.scale)));
            if (width > std::numeric_limits<uint32_t>::max()
                || height > std::numeric_limits<uint32_t>::max())
                return std::nullopt;
            return Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        }
    }
    return std::nullopt;
}
