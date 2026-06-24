#include "UiRenderResolver.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <vector>

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

    glm::vec2 mixVec2(glm::vec2 from, glm::vec2 to, float t)
    {
        return from + (to - from) * t;
    }

    void emitTexturedQuadClipped(UiCommandBuffer& buffer,
                                 const UiRect& bounds,
                                 const UiRect& clip,
                                 texture_id_type textureId,
                                 glm::vec2 uvMin,
                                 glm::vec2 uvMax,
                                 glm::vec4 tint,
                                 int viewportWidth,
                                 int viewportHeight)
    {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        const UiRect clipped = intersectRect(bounds, clip);
        if (isEmptyRect(clipped))
            return;

        const float leftT = (clipped.x - bounds.x) / bounds.width;
        const float rightT = (clipped.x + clipped.width - bounds.x) / bounds.width;
        const float topT = (clipped.y - bounds.y) / bounds.height;
        const float bottomT = (clipped.y + clipped.height - bounds.y) / bounds.height;

        const glm::vec2 clippedUvMin = {
            mixVec2(uvMin, uvMax, leftT).x,
            mixVec2(uvMin, uvMax, topT).y
        };
        const glm::vec2 clippedUvMax = {
            mixVec2(uvMin, uvMax, rightT).x,
            mixVec2(uvMin, uvMax, bottomT).y
        };

        const UiRect snapped = snapRectToPixels(clipped, viewportWidth, viewportHeight);
        buffer.addTexturedQuadUv(snapped.x,
                                 snapped.y,
                                 snapped.width,
                                 snapped.height,
                                 textureId,
                                 clippedUvMin,
                                 clippedUvMax,
                                 tint);
    }

    struct UiAxisNineSlice
    {
        float dst[4] = {};
        float uv[4] = {};
    };

    UiAxisNineSlice makeAxisSlice(float origin, float size, float leading, float trailing)
    {
        leading = std::clamp(leading, 0.0f, 0.49f);
        trailing = std::clamp(trailing, 0.0f, 0.49f);
        const float total = leading + trailing;
        if (total > 0.98f)
        {
            const float scale = 0.98f / total;
            leading *= scale;
            trailing *= scale;
        }

        UiAxisNineSlice axis;
        axis.dst[0] = origin;
        axis.dst[1] = origin + size * leading;
        axis.dst[2] = origin + size - size * trailing;
        axis.dst[3] = origin + size;
        axis.uv[0] = 0.0f;
        axis.uv[1] = leading;
        axis.uv[2] = 1.0f - trailing;
        axis.uv[3] = 1.0f;
        return axis;
    }

    void emitNineSlice(UiCommandBuffer& buffer,
                       const UiNineSlicePrimitive& value,
                       texture_id_type textureId,
                       int viewportWidth,
                       int viewportHeight)
    {
        if (value.bounds.width <= 0.0f || value.bounds.height <= 0.0f)
            return;

        const UiRect clipped = intersectRect(value.bounds, value.clipRect);
        if (isEmptyRect(clipped))
            return;

        const UiAxisNineSlice x = makeAxisSlice(value.bounds.x,
                                                value.bounds.width,
                                                value.slice.left,
                                                value.slice.right);
        const UiAxisNineSlice y = makeAxisSlice(value.bounds.y,
                                                value.bounds.height,
                                                value.slice.top,
                                                value.slice.bottom);
        const glm::vec4 tint = toGlm(value.tint);

        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                const UiRect segment = {
                    x.dst[column],
                    y.dst[row],
                    std::max(0.0f, x.dst[column + 1] - x.dst[column]),
                    std::max(0.0f, y.dst[row + 1] - y.dst[row])
                };

                emitTexturedQuadClipped(buffer,
                                        segment,
                                        value.clipRect,
                                        textureId,
                                        {x.uv[column], y.uv[row]},
                                        {x.uv[column + 1], y.uv[row + 1]},
                                        tint,
                                        viewportWidth,
                                        viewportHeight);
            }
        }
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

    enum class ClipEdge
    {
        Left,
        Right,
        Top,
        Bottom
    };

    bool insideEdge(glm::vec2 point, ClipEdge edge, const UiRect& clip)
    {
        switch (edge)
        {
            case ClipEdge::Left:   return point.x >= clip.x;
            case ClipEdge::Right:  return point.x <= clip.x + clip.width;
            case ClipEdge::Top:    return point.y >= clip.y;
            case ClipEdge::Bottom: return point.y <= clip.y + clip.height;
        }
        return false;
    }

    glm::vec2 edgeIntersection(glm::vec2 start, glm::vec2 end, ClipEdge edge, const UiRect& clip)
    {
        const glm::vec2 delta = end - start;
        float t = 0.0f;

        switch (edge)
        {
            case ClipEdge::Left:
                if (std::abs(delta.x) <= 0.000001f) return start;
                t = (clip.x - start.x) / delta.x;
                break;
            case ClipEdge::Right:
                if (std::abs(delta.x) <= 0.000001f) return start;
                t = (clip.x + clip.width - start.x) / delta.x;
                break;
            case ClipEdge::Top:
                if (std::abs(delta.y) <= 0.000001f) return start;
                t = (clip.y - start.y) / delta.y;
                break;
            case ClipEdge::Bottom:
                if (std::abs(delta.y) <= 0.000001f) return start;
                t = (clip.y + clip.height - start.y) / delta.y;
                break;
        }

        return start + delta * std::clamp(t, 0.0f, 1.0f);
    }

    std::vector<glm::vec2> clipPolygonEdge(const std::vector<glm::vec2>& polygon,
                                           ClipEdge edge,
                                           const UiRect& clip)
    {
        std::vector<glm::vec2> result;
        if (polygon.empty())
            return result;

        glm::vec2 previous = polygon.back();
        bool previousInside = insideEdge(previous, edge, clip);

        for (glm::vec2 current : polygon)
        {
            const bool currentInside = insideEdge(current, edge, clip);
            if (currentInside)
            {
                if (!previousInside)
                    result.push_back(edgeIntersection(previous, current, edge, clip));
                result.push_back(current);
            }
            else if (previousInside)
            {
                result.push_back(edgeIntersection(previous, current, edge, clip));
            }

            previous = current;
            previousInside = currentInside;
        }

        return result;
    }

    std::vector<glm::vec2> clipPolygonToRect(std::vector<glm::vec2> polygon, const UiRect& clip)
    {
        polygon = clipPolygonEdge(polygon, ClipEdge::Left, clip);
        polygon = clipPolygonEdge(polygon, ClipEdge::Right, clip);
        polygon = clipPolygonEdge(polygon, ClipEdge::Top, clip);
        polygon = clipPolygonEdge(polygon, ClipEdge::Bottom, clip);
        return polygon;
    }

    void emitClippedCircle(UiCommandBuffer& buffer,
                           const UiCirclePrimitive& value,
                           int viewportWidth,
                           int viewportHeight)
    {
        const UiRect clippedBounds = intersectRect(value.bounds, value.clipRect);
        if (isEmptyRect(clippedBounds))
            return;

        if (value.bounds.width <= 0.0f || value.bounds.height <= 0.0f)
            return;

        constexpr float TwoPi = 6.28318530717958647692f;
        const float centerX = value.bounds.x + value.bounds.width * 0.5f;
        const float centerY = value.bounds.y + value.bounds.height * 0.5f;
        const float radiusX = value.bounds.width * 0.5f;
        const float radiusY = value.bounds.height * 0.5f;
        const int segments = std::clamp(value.segments, 12, 96);

        std::vector<glm::vec2> points;
        points.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i)
        {
            const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back({
                centerX + std::cos(angle) * radiusX,
                centerY + std::sin(angle) * radiusY
            });
        }

        std::vector<glm::vec2> clipped = clipPolygonToRect(std::move(points), value.clipRect);
        if (clipped.size() < 3)
            return;

        for (glm::vec2& point : clipped)
            point = snapPointToPixels(point, viewportWidth, viewportHeight);

        buffer.addColoredPolygon(clipped, toGlm(value.color));
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

                if (std::abs(value.rotation) > 0.000001f)
                {
                    const UiRect snapped = snapRectToPixels(value.bounds, viewportWidth, viewportHeight);
                    buffer.addTexturedQuadUvRotated(snapped.x,
                                                    snapped.y,
                                                    snapped.width,
                                                    snapped.height,
                                                    textureId,
                                                    {0.0f, 0.0f},
                                                    {1.0f, 1.0f},
                                                    value.rotation,
                                                    toGlm(value.tint));
                    return;
                }

                emitTexturedQuadClipped(buffer,
                                        value.bounds,
                                        value.clipRect,
                                        textureId,
                                        {0.0f, 0.0f},
                                        {1.0f, 1.0f},
                                        toGlm(value.tint),
                                        viewportWidth,
                                        viewportHeight);
            }
            else if constexpr (std::is_same_v<T, UiNineSlicePrimitive>)
            {
                if (!resources || value.imageAsset.empty()) return;

                const texture_id_type textureId = resources->requestTexture(value.imageAsset);
                if (textureId == 0) return;

                emitNineSlice(buffer, value, textureId, viewportWidth, viewportHeight);
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
            else if constexpr (std::is_same_v<T, UiCirclePrimitive>)
            {
                emitClippedCircle(buffer, value, viewportWidth, viewportHeight);
            }
        }, primitive);
    }
}
