#pragma once

#include "UiTypes.h"

namespace gts::tools
{
    struct ToolTheme
    {
        static constexpr UiColor windowBackground = {0.014f, 0.017f, 0.020f, 0.99f};
        static constexpr UiColor barBackground    = {0.025f, 0.029f, 0.034f, 0.98f};
        static constexpr UiColor railBackground   = {0.020f, 0.024f, 0.029f, 0.99f};
        static constexpr UiColor paneBackground   = {0.027f, 0.031f, 0.036f, 0.98f};
        static constexpr UiColor paneSurface      = {0.038f, 0.044f, 0.050f, 0.96f};
        static constexpr UiColor viewportBar      = {0.018f, 0.021f, 0.025f, 0.90f};
        static constexpr UiColor button           = {0.067f, 0.077f, 0.088f, 0.96f};
        static constexpr UiColor buttonHover      = {0.092f, 0.105f, 0.118f, 1.0f};
        static constexpr UiColor buttonPressed    = {0.118f, 0.137f, 0.150f, 1.0f};
        static constexpr UiColor buttonActive     = {0.105f, 0.172f, 0.210f, 1.0f};
        static constexpr UiColor toggleActive     = {0.115f, 0.205f, 0.155f, 1.0f};
        static constexpr UiColor border           = {0.135f, 0.150f, 0.165f, 0.88f};
        static constexpr UiColor accent           = {0.260f, 0.560f, 0.780f, 1.0f};
        static constexpr UiColor text             = {0.880f, 0.920f, 0.950f, 1.0f};
        static constexpr UiColor mutedText        = {0.580f, 0.650f, 0.700f, 1.0f};
        static constexpr UiColor statusText       = {0.720f, 0.780f, 0.830f, 1.0f};

        static constexpr float shellPadding     = 0.006f;
        static constexpr float panelInset       = 0.040f;
        static constexpr float panelWidth       = 0.920f;
        static constexpr float headerTextScale  = 0.0105f;
        static constexpr float bodyTextScale    = 0.0088f;
        static constexpr float smallTextScale   = 0.0078f;
        static constexpr float buttonTextScale  = 0.0082f;
        static constexpr float railIconScale    = 0.0110f;
        static constexpr float titleTextScale   = 0.0108f;
        static constexpr float rowHeight        = 0.036f;
        static constexpr float compactRowHeight = 0.031f;
        static constexpr float sliderHeight     = 0.030f;
    };
} // namespace gts::tools
