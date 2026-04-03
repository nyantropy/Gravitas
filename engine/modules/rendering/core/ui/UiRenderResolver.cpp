#include "UiRenderResolver.h"

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
}

UiCommandBuffer UiRenderResolver::buildCommandBuffer(
    const UiVisualList& visualList,
    IResourceProvider* resources,
    const std::unordered_map<UiHandle, BitmapFont*>& textBindings,
    int viewportWidth,
    int viewportHeight) const
{
    UiCommandBuffer buffer;

    for (const UiVisualPrimitive& primitive : visualList.primitives)
    {
        std::visit([&](const auto& value)
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, UiRectPrimitive>)
            {
                const UiRect snapped = snapRectToPixels(value.bounds, viewportWidth, viewportHeight);
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

                const UiRect snapped = snapRectToPixels(value.bounds, viewportWidth, viewportHeight);
                buffer.addTexturedQuad(snapped.x,
                                       snapped.y,
                                       snapped.width,
                                       snapped.height,
                                       textureId,
                                       toGlm(value.tint));
            }
            else if constexpr (std::is_same_v<T, UiTextPrimitive>)
            {
                const auto it = textBindings.find(value.source);
                if (it == textBindings.end() || !it->second) return;

                const float snappedX = snapToPixel(value.bounds.x, viewportWidth);
                const float snappedY = snapToPixel(value.bounds.y, viewportHeight);

                GlyphLayoutEngine::appendUiText(buffer,
                                                *it->second,
                                                value.text,
                                                snappedX,
                                                snappedY,
                                                value.scale,
                                                toGlm(value.color));
            }
        }, primitive);
    }

    return buffer;
}
