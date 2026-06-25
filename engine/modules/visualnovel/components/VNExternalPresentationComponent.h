#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "VNTypes.h"

namespace gts::vn
{
    struct VNExternalSprite
    {
        std::string id;
        VNSprite sprite;
        float appearDurationSeconds = 0.0f;
        gts::tween::TweenEase appearEase = gts::tween::TweenEase::SmoothStep;
    };

    struct VNExternalSpriteAnimation
    {
        std::string spriteId;
        float durationSeconds = 0.0f;
        float intensity = 0.012f;
        float frequency = 18.0f;
        gts::tween::TweenEase ease = gts::tween::TweenEase::EaseOutQuad;
    };

    struct VNExternalPresentationComponent
    {
        bool active = false;
        VNBackground background;
        float dimmingAlpha = 0.0f;
        bool suppressSceneRendering = false;
        bool suppressParticles = false;
        std::vector<VNExternalSprite> sprites;
        std::vector<VNExternalSpriteAnimation> animationRequests;
        uint64_t revision = 1;

        void markDirty()
        {
            ++revision;
        }

        void clear()
        {
            active = false;
            background = VNBackground{};
            dimmingAlpha = 0.0f;
            suppressSceneRendering = false;
            suppressParticles = false;
            sprites.clear();
            animationRequests.clear();
            markDirty();
        }
    };
} // namespace gts::vn
