#include "UiRenderResolver.h"

#include <type_traits>

#include "GlyphLayoutEngine.h"

namespace
{
    glm::vec4 toGlm(const UiColor& color)
    {
        return {color.r, color.g, color.b, color.a};
    }
}

UiCommandBuffer UiRenderResolver::buildCommandBuffer(
    const UiVisualList& visualList,
    IResourceProvider* resources,
    const std::unordered_map<UiHandle, BitmapFont*>& textBindings) const
{
    UiCommandBuffer buffer;

    for (const UiVisualPrimitive& primitive : visualList.primitives)
    {
        std::visit([&](const auto& value)
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, UiRectPrimitive>)
            {
                buffer.addColoredQuad(value.bounds.x,
                                      value.bounds.y,
                                      value.bounds.width,
                                      value.bounds.height,
                                      toGlm(value.color));
            }
            else if constexpr (std::is_same_v<T, UiImagePrimitive>)
            {
                if (!resources || value.imageAsset.empty()) return;

                const texture_id_type textureId = resources->requestTexture(value.imageAsset);
                if (textureId == 0) return;

                buffer.addTexturedQuad(value.bounds.x,
                                       value.bounds.y,
                                       value.bounds.width,
                                       value.bounds.height,
                                       textureId,
                                       toGlm(value.tint));
            }
            else if constexpr (std::is_same_v<T, UiTextPrimitive>)
            {
                const auto it = textBindings.find(value.source);
                if (it == textBindings.end() || !it->second) return;

                GlyphLayoutEngine::appendUiText(buffer,
                                                *it->second,
                                                value.text,
                                                value.bounds.x,
                                                value.bounds.y,
                                                value.scale,
                                                toGlm(value.color));
            }
        }, primitive);
    }

    return buffer;
}
