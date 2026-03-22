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
#include "GlyphLayoutEngine.h"

// Extracts UiImageComponent and UITextComponent entities from the ECS world each frame.
// Produces a UiCommandBuffer for UiRenderStage.
class UiCommandExtractor : public IGtsExtractor<UiCommandBuffer>
{
public:
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

            GlyphLayoutEngine::appendUiText(buffer, *ui.font, ui.text, ui.x, ui.y, ui.scale);
        });

        return buffer;
    }
};
