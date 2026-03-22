#pragma once

#include <vector>
#include <unordered_map>

#include "ECSWorld.hpp"
#include "IGtsExtractor.h"
#include "UITextComponent.h"
#include "TextCommand.h"
#include "BitmapFont.h"
#include "GlyphLayoutEngine.h"

// Reads all visible UITextComponent entities from the ECS world each frame,
// lays out glyphs in screen space (Y increases downward), and batches the
// results into one TextCommandList per atlas texture.
//
// Coordinate convention (screen-space, Y-down):
//   (0, 0) = top-left, (1, 1) = bottom-right.
//   '\n' : cursor.x = ui.x, cursor.y += lineHeight * scale.
//   Quad : top-left = (x0, y0), bottom-right = (x1, y1), y0 < y1.
class UITextCommandExtractor : public IGtsExtractor<std::vector<TextCommandList>>
{
public:
    std::vector<TextCommandList> extract(const GtsExtractorContext& ctx) override
    {
        std::unordered_map<texture_id_type, TextCommandList> batches;

        ctx.world.forEach<UITextComponent>([&](Entity, UITextComponent& ui)
        {
            if (!ui.visible || !ui.font || ui.text.empty())
                return;

            const BitmapFont& font = *ui.font;

            TextCommandList& cmd = batches[font.atlasTexture];
            cmd.textureID = font.atlasTexture;

            GlyphLayoutEngine::appendText(cmd, font, ui.text, ui.x, ui.y, ui.scale);
        });

        std::vector<TextCommandList> result;
        result.reserve(batches.size());
        for (auto& [id, cmd] : batches)
            if (!cmd.vertices.empty())
                result.push_back(std::move(cmd));

        return result;
    }
};
