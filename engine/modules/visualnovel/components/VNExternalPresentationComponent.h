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
        bool suppressSceneRendering = false;
        bool suppressParticles = false;
        std::vector<VNExternalSprite> sprites;

        void clear()
        {
            active = false;
            background = VNBackground{};
            dimmingAlpha = 0.0f;
            suppressSceneRendering = false;
            suppressParticles = false;
            sprites.clear();
        }
    };
} // namespace gts::vn
