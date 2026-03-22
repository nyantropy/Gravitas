#pragma once

#include <string>
#include <vector>

#include "GlmConfig.h"

#include "QuadTextComponent.h"
#include "BitmapFont.h"
#include "UICommand.h"
#include "Vertex.h"

// Converts a QuadTextComponent's string into flat world-space quad geometry
// stored in caller-owned vertex and index vectors.  Used by TextRenderStage.
//
// Coordinate convention (world-space, entity local XY plane):
//   X increases rightward, Y increases upward.
//   '\n' : cursor.x = 0, cursor.y -= lineHeight * scale.
//   Quad : top-left = (x0, y0), bottom-right = (x1, y1) with y0 > y1.
namespace GlyphLayoutEngine
{
    inline void build(const QuadTextComponent& text,
                      std::vector<Vertex>& verts,
                      std::vector<uint32_t>& indices)
    {
        const BitmapFont& font  = *text.font;
        const float       scale = text.scale;

        float cursorX = 0.0f;
        float cursorY = 0.0f;

        for (char ch : text.text)
        {
            if (ch == '\n')
            {
                cursorX  = 0.0f;
                cursorY -= font.lineHeight * scale;
                continue;
            }

            auto it = font.glyphs.find(ch);
            if (it == font.glyphs.end())
            {
                // Space or unknown glyph: advance by half the line height.
                cursorX += font.lineHeight * 0.5f * scale;
                continue;
            }

            const GlyphInfo& g = it->second;

            if (ch != ' ')
            {
                // Quad corners in local space.
                // bearing.y is the distance from baseline to glyph top (positive = up).
                float x0 = cursorX + g.bearing.x * scale;
                float y0 = cursorY + g.bearing.y * scale;
                float x1 = x0 + g.size.x * scale;
                float y1 = y0 - g.size.y * scale;

                auto base = static_cast<uint32_t>(verts.size());

                constexpr glm::vec3 white = {1.0f, 1.0f, 1.0f};

                verts.push_back({{x0, y0, 0.0f}, white, {g.uvMin.x, g.uvMin.y}}); // TL
                verts.push_back({{x1, y0, 0.0f}, white, {g.uvMax.x, g.uvMin.y}}); // TR
                verts.push_back({{x0, y1, 0.0f}, white, {g.uvMin.x, g.uvMax.y}}); // BL
                verts.push_back({{x1, y1, 0.0f}, white, {g.uvMax.x, g.uvMax.y}}); // BR

                // Two CCW triangles (TL→BL→TR, TR→BL→BR).
                indices.push_back(base + 0);
                indices.push_back(base + 2);
                indices.push_back(base + 1);
                indices.push_back(base + 1);
                indices.push_back(base + 2);
                indices.push_back(base + 3);
            }

            cursorX += g.advance * scale;
        }
    }

    // Append quads for text into cmd, starting at (x, y) with scale.
    // '\n' advances y by font.lineHeight * scale and resets x.
    // Unknown characters advance cursor by half a line height without emitting quads.
    inline void appendText(UICommandList&     cmd,
                           const BitmapFont&  font,
                           const std::string& text,
                           float              x,
                           float              y,
                           float              scale)
    {
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
    }

    // Find or create the UICommandList matching font.atlasTexture in batches,
    // then call appendText into it.
    inline void appendTextToBatch(std::vector<UICommandList>& batches,
                                   const BitmapFont&           font,
                                   const std::string&          text,
                                   float x, float y, float scale)
    {
        UICommandList* cmd = nullptr;
        for (auto& b : batches)
            if (b.textureID == font.atlasTexture) { cmd = &b; break; }
        if (!cmd)
        {
            batches.push_back({});
            cmd = &batches.back();
            cmd->textureID = font.atlasTexture;
        }
        appendText(*cmd, font, text, x, y, scale);
    }
}
