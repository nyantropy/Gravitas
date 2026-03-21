#pragma once

#include <vector>
#include <unordered_map>

#include "ECSWorld.hpp"
#include "UITextComponent.h"
#include "UICommand.h"
#include "BitmapFont.h"

// Reads all visible UITextComponent entities from the ECS world each frame,
// lays out glyphs in screen space (Y increases downward), and batches the
// results into one UICommandList per atlas texture.
//
// Coordinate convention (screen-space, Y-down):
//   (0, 0) = top-left, (1, 1) = bottom-right.
//   '\n' : cursor.x = ui.x, cursor.y += lineHeight * scale.
//   Quad : top-left = (x0, y0), bottom-right = (x1, y1), y0 < y1.
class UICommandExtractor
{
public:
    std::vector<UICommandList> extract(ECSWorld& world)
    {
        std::unordered_map<texture_id_type, UICommandList> batches;

        world.forEach<UITextComponent>([&](Entity, UITextComponent& ui)
        {
            if (!ui.visible || !ui.font || ui.text.empty())
                return;

            const BitmapFont& font  = *ui.font;
            const float       scale = ui.scale;

            UICommandList& cmd = batches[font.atlasTexture];
            cmd.textureID = font.atlasTexture;

            float cursorX = ui.x;
            float cursorY = ui.y;

            for (char ch : ui.text)
            {
                if (ch == '\n')
                {
                    cursorX  = ui.x;
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
                    // Screen-space quad.  bearing.y is positive-up in atlas metrics;
                    // subtract it to push text toward the top (smaller screen y).
                    float x0 = cursorX + g.bearing.x * scale;
                    float y0 = cursorY - (g.bearing.y - g.size.y) * scale;
                    float x1 = x0 + g.size.x * scale;
                    float y1 = y0 + g.size.y * scale;

                    auto base = static_cast<uint32_t>(cmd.vertices.size());

                    cmd.vertices.push_back({{x0, y0}, {g.uvMin.x, g.uvMin.y}}); // TL
                    cmd.vertices.push_back({{x1, y0}, {g.uvMax.x, g.uvMin.y}}); // TR
                    cmd.vertices.push_back({{x0, y1}, {g.uvMin.x, g.uvMax.y}}); // BL
                    cmd.vertices.push_back({{x1, y1}, {g.uvMax.x, g.uvMax.y}}); // BR

                    // Two CCW triangles (TL→BL→TR, TR→BL→BR) — cull mode NONE.
                    cmd.indices.push_back(base + 0);
                    cmd.indices.push_back(base + 2);
                    cmd.indices.push_back(base + 1);
                    cmd.indices.push_back(base + 1);
                    cmd.indices.push_back(base + 2);
                    cmd.indices.push_back(base + 3);
                }

                cursorX += g.advance * scale;
            }
        });

        std::vector<UICommandList> result;
        result.reserve(batches.size());
        for (auto& [id, cmd] : batches)
            if (!cmd.vertices.empty())
                result.push_back(std::move(cmd));

        return result;
    }
};
