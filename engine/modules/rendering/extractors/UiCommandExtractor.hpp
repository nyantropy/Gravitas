#pragma once

#include "ECSWorld.hpp"
#include "UiCommand.h"
#include "UiImageComponent.h"

// Extracts UiImageComponent entities from the ECS world each frame and
// produces a UiCommandBuffer for UiRenderStage.
class UiCommandExtractor
{
public:
    UiCommandBuffer extract(ECSWorld& world)
    {
        UiCommandBuffer buffer;

        world.forEach<UiImageComponent>([&](Entity, UiImageComponent& img)
        {
            if (!img.visible || img.textureID == 0)
                return;

            buffer.addTexturedQuad(img.x, img.y, img.w, img.h, img.textureID, img.tint);
        });

        return buffer;
    }
};
