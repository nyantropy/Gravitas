#include "UiRenderResolver.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "GlyphLayoutEngine.h"

namespace
{
    glm::vec4 toGlm(const UiColor& color)
    {
        return {color.r, color.g, color.b, color.a};
    }

    float snapToPixel(float normalizedValue, int pixelSpan)
    {
        if (pixelSpan <= 0) return normalizedValue;
        return std::floor(normalizedValue * static_cast<float>(pixelSpan))
            / static_cast<float>(pixelSpan);
    }

    UiRect snapRectToPixels(const UiRect& rect, int viewportWidth, int viewportHeight)
    {
        if (viewportWidth <= 0 || viewportHeight <= 0) return rect;

        const float left = snapToPixel(rect.x, viewportWidth);
        const float top = snapToPixel(rect.y, viewportHeight);
        const float width = std::floor(rect.width * static_cast<float>(viewportWidth))
            / static_cast<float>(viewportWidth);
        const float height = std::floor(rect.height * static_cast<float>(viewportHeight))
            / static_cast<float>(viewportHeight);

        return {
            left,
            top,
            std::max(0.0f, width),
            std::max(0.0f, height)
        };
    }

    glm::vec2 snapPointToPixels(glm::vec2 point, int viewportWidth, int viewportHeight)
    {
        return {
            snapToPixel(point.x, viewportWidth),
            snapToPixel(point.y, viewportHeight)
        };
    }

    UiRect intersectRect(const UiRect& a, const UiRect& b)
    {
        const float left = std::max(a.x, b.x);
        const float top = std::max(a.y, b.y);
        const float right = std::min(a.x + a.width, b.x + b.width);
        const float bottom = std::min(a.y + a.height, b.y + b.height);

        if (right <= left || bottom <= top)
            return {left, top, 0.0f, 0.0f};

        return {left, top, right - left, bottom - top};
    }

    bool isEmptyRect(const UiRect& rect)
    {
        return rect.width <= 0.0f || rect.height <= 0.0f;
    }

    bool clipLineToRect(glm::vec2& start, glm::vec2& end, const UiRect& clip)
    {
        if (isEmptyRect(clip))
            return false;

        const float minX = clip.x;
        const float minY = clip.y;
        const float maxX = clip.x + clip.width;
        const float maxY = clip.y + clip.height;
        const glm::vec2 delta = end - start;
        float t0 = 0.0f;
        float t1 = 1.0f;

        auto clipEdge = [&](float p, float q) -> bool
        {
            if (p == 0.0f)
                return q >= 0.0f;

            const float r = q / p;
            if (p < 0.0f)
            {
                if (r > t1) return false;
                if (r > t0) t0 = r;
            }
            else
            {
                if (r < t0) return false;
                if (r < t1) t1 = r;
            }
            return true;
        };

        if (!clipEdge(-delta.x, start.x - minX)) return false;
        if (!clipEdge( delta.x, maxX - start.x)) return false;
        if (!clipEdge(-delta.y, start.y - minY)) return false;
        if (!clipEdge( delta.y, maxY - start.y)) return false;

        const glm::vec2 originalStart = start;
        start = originalStart + delta * t0;
        end = originalStart + delta * t1;
        return true;
    }
}

void UiRenderResolver::buildCommandBuffer(
    UiCommandBuffer& buffer,
    const UiVisualList& visualList,
    IResourceProvider* resources,
    const std::unordered_map<UiHandle, BitmapFont*>& textBindings,
    int viewportWidth,
    int viewportHeight) const
{
    buffer.clear();

    for (const UiVisualPrimitive& primitive : visualList.primitives)
    {
        std::visit([&](const auto& value)
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, UiRectPrimitive>)
            {
                const UiRect clipped = intersectRect(value.bounds, value.clipRect);
                if (isEmptyRect(clipped)) return;

                const UiRect snapped = snapRectToPixels(clipped, viewportWidth, viewportHeight);
                buffer.addColoredQuad(snapped.x,
                                      snapped.y,
                                      snapped.width,
                                      snapped.height,
                                      toGlm(value.color));
            }
            else if constexpr (std::is_same_v<T, UiImagePrimitive>)
            {
                if (!resources || value.imageAsset.empty()) return;

                const texture_id_type textureId = resources->requestTexture(value.imageAsset);
                if (textureId == 0) return;

                const UiRect clipped = intersectRect(value.bounds, value.clipRect);
                if (isEmptyRect(clipped)) return;

                const float leftT = (clipped.x - value.bounds.x) / value.bounds.width;
                const float rightT = (clipped.x + clipped.width - value.bounds.x) / value.bounds.width;
                const float topT = (clipped.y - value.bounds.y) / value.bounds.height;
                const float bottomT = (clipped.y + clipped.height - value.bounds.y) / value.bounds.height;
                const UiRect snapped = snapRectToPixels(clipped, viewportWidth, viewportHeight);
                buffer.addTexturedQuadUv(snapped.x,
                                         snapped.y,
                                         snapped.width,
                                         snapped.height,
                                         textureId,
                                         {leftT, topT},
                                         {rightT, bottomT},
                                         toGlm(value.tint));
            }
            else if constexpr (std::is_same_v<T, UiTextPrimitive>)
            {
                const auto it = textBindings.find(value.source);
                if (it == textBindings.end() || !it->second) return;

                const UiRect snappedBounds = snapRectToPixels(value.bounds, viewportWidth, viewportHeight);
                const UiRect snappedClip = snapRectToPixels(value.clipRect, viewportWidth, viewportHeight);

                GlyphLayoutEngine::appendUiTextInRect(buffer,
                                                      *it->second,
                                                      value.text,
                                                      snappedBounds,
                                                      snappedClip,
                                                      value.scale,
                                                      toGlm(value.color),
                                                      value.wrapMode,
                                                      value.horizontalAlign,
                                                      value.verticalAlign,
                                                      value.maxLines);
            }
            else if constexpr (std::is_same_v<T, UiLinePrimitive>)
            {
                glm::vec2 start = {value.start.x, value.start.y};
                glm::vec2 end = {value.end.x, value.end.y};
                if (!clipLineToRect(start, end, value.clipRect)) return;

                start = snapPointToPixels(start, viewportWidth, viewportHeight);
                end = snapPointToPixels(end, viewportWidth, viewportHeight);
                buffer.addColoredLine(start, end, value.thickness, toGlm(value.color));
            }
        }, primitive);
    }
}
