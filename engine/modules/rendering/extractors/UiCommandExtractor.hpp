#pragma once

#include <vector>
#include <unordered_map>

#include "ECSWorld.hpp"
#include "IGtsExtractor.h"
#include "UiCommand.h"
#include "UiImageComponent.h"
#include "UITextComponent.h"
#include "BitmapFont.h"
#include "GtsExtractorContext.h"
#include "GtsDebugOverlay.h"
#include "IResourceProvider.hpp"

// Extracts UiImageComponent and UITextComponent entities from the ECS world each frame,
// and appends debug overlay if enabled.  Produces a UiCommandBuffer for UiRenderStage.
class UiCommandExtractor : public IGtsExtractor<UiCommandBuffer>
{
public:
    void init(IResourceProvider* resources)
    {
        debugOverlay.init(resources);
    }

    GtsDebugOverlay& getDebugOverlay() { return debugOverlay; }

    UiCommandBuffer extract(const GtsExtractorContext& ctx) override
    {
        UiCommandBuffer buffer;

        // Image quads
        ctx.world.forEach<UiImageComponent>([&](Entity, UiImageComponent& img)
        {
            if (!img.visible || img.textureID == 0)
                return;

            float h = (img.width * ctx.viewportAspect) / img.imageAspect;
            buffer.addTexturedQuad(img.x, img.y, img.width, h, img.textureID, img.tint);
        });

        // Screen-space text
        ctx.world.forEach<UITextComponent>([&](Entity, UITextComponent& ui)
        {
            if (!ui.visible || !ui.font || ui.text.empty())
                return;

            appendTextToBuffer(buffer, *ui.font, ui.text, ui.x, ui.y, ui.scale);
        });

        // Debug overlay
        if (debugOverlay.isEnabled() && ctx.frameStats)
            debugOverlay.appendToBuffer(buffer, *ctx.frameStats);

        return buffer;
    }

private:
    GtsDebugOverlay debugOverlay;

    static void appendTextToBuffer(UiCommandBuffer& buffer,
                                   const BitmapFont& font,
                                   const std::string& text,
                                   float x, float y, float scale,
                                   glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        std::vector<UiVertex>   verts;
        std::vector<uint32_t>   indices;

        float cursorX = x;
        float cursorY = y;

        for (char ch : text)
        {
            if (ch == '\n')
            {
                cursorX  = x;
                cursorY += font.lineHeight * scale;
                continue;
            }

            auto it = font.glyphs.find(ch);
            if (it == font.glyphs.end())
            {
                cursorX += font.lineHeight * 0.5f * scale;
                continue;
            }

            const GlyphInfo& g = it->second;

            if (ch != ' ')
            {
                float x0 = cursorX + g.bearing.x * scale;
                float y0 = cursorY - (g.bearing.y - g.size.y) * scale;
                float x1 = x0 + g.size.x * scale;
                float y1 = y0 + g.size.y * scale;

                auto base = static_cast<uint32_t>(verts.size());

                verts.push_back({{x0, y0}, {g.uvMin.x, g.uvMin.y}, color}); // TL
                verts.push_back({{x1, y0}, {g.uvMax.x, g.uvMin.y}, color}); // TR
                verts.push_back({{x0, y1}, {g.uvMin.x, g.uvMax.y}, color}); // BL
                verts.push_back({{x1, y1}, {g.uvMax.x, g.uvMax.y}, color}); // BR

                // Two CCW triangles (TL→BL→TR, TR→BL→BR) — cull mode NONE.
                indices.push_back(base + 0);
                indices.push_back(base + 2);
                indices.push_back(base + 1);
                indices.push_back(base + 1);
                indices.push_back(base + 2);
                indices.push_back(base + 3);
            }

            cursorX += g.advance * scale;
        }

        buffer.addGlyphBatch(verts, indices, font.atlasTexture);
    }
};
