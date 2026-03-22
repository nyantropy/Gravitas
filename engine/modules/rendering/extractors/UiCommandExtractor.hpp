#pragma once

#include "ECSWorld.hpp"
#include "IGtsExtractor.h"
#include "UiCommand.h"
#include "UiImageComponent.h"

// Extracts UiImageComponent entities from the ECS world each frame and
// produces a UiCommandBuffer for UiRenderStage.
class UiCommandExtractor : public IGtsExtractor<UiCommandBuffer>
{
public:
    UiCommandBuffer extract(const GtsExtractorContext& ctx) override
    {
        UiCommandBuffer buffer;

        ctx.world.forEach<UiImageComponent>([&](Entity, UiImageComponent& img)
        {
            if (!img.visible || img.textureID == 0)
                return;

            // width covers (width * viewportWidth) pixels.
            // height must cover (width * viewportWidth / imageAspect) pixels.
            // In normalized height: divide pixel height by viewportHeight.
            // = width * viewportAspect / imageAspect
            float h = (img.width * ctx.viewportAspect) / img.imageAspect;

            buffer.addTexturedQuad(img.x, img.y, img.width, h, img.textureID, img.tint);
        });

        return buffer;
    }
};
