#pragma once

#include <string>
#include <vector>

#include "VNTypes.h"

namespace gts::vn
{
    struct VNExternalSprite
    {
        std::string id;
        VNSprite sprite;
    };

    struct VNExternalPresentationComponent
    {
        bool active = false;
        VNBackground background;
        float dimmingAlpha = 0.0f;
        std::vector<VNExternalSprite> sprites;

        void clear()
        {
            active = false;
            background = VNBackground{};
            dimmingAlpha = 0.0f;
            sprites.clear();
        }
    };
} // namespace gts::vn
