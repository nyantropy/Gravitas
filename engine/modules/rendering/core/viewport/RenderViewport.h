#pragma once

#include <algorithm>

struct RenderViewportRect
{
    int x      = 0;
    int y      = 0;
    int width  = 1;
    int height = 1;

    static RenderViewportRect full(int windowWidth, int windowHeight)
    {
        return {0, 0, std::max(1, windowWidth), std::max(1, windowHeight)};
    }

    bool valid() const
    {
        return width > 0 && height > 0;
    }

    float aspect() const
    {
        return height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    }

    RenderViewportRect clampedTo(int targetWidth, int targetHeight) const
    {
        const int safeTargetWidth  = std::max(1, targetWidth);
        const int safeTargetHeight = std::max(1, targetHeight);
        const int clampedX         = std::clamp(x, 0, safeTargetWidth - 1);
        const int clampedY         = std::clamp(y, 0, safeTargetHeight - 1);
        const int maxWidth         = std::max(1, safeTargetWidth - clampedX);
        const int maxHeight        = std::max(1, safeTargetHeight - clampedY);
        return {clampedX, clampedY, std::clamp(width, 1, maxWidth), std::clamp(height, 1, maxHeight)};
    }

    bool remapPointer(float pixelX, float pixelY, float& outX, float& outY) const
    {
        const float safeWidth  = static_cast<float>(std::max(1, width));
        const float safeHeight = static_cast<float>(std::max(1, height));
        outX                   = std::clamp((pixelX - static_cast<float>(x)) / safeWidth, 0.0f, 1.0f);
        outY                   = std::clamp((pixelY - static_cast<float>(y)) / safeHeight, 0.0f, 1.0f);
        return pixelX >= static_cast<float>(x) && pixelY >= static_cast<float>(y) &&
               pixelX <= static_cast<float>(x + width) && pixelY <= static_cast<float>(y + height);
    }
};
