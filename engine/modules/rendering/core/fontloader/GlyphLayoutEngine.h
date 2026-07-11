#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "GlmConfig.h"

#include "WorldTextComponent.h"
#include "BitmapFont.h"
#include "Vertex.h"
#include "UiCommand.h"
#include "UiTypes.h"

// Converts a WorldTextComponent's string into flat world-space quad geometry
// using an already resolved BitmapFont. Used by WorldTextBindingSystem.
//
// Coordinate convention (world-space, entity local XY plane):
//   X increases rightward, Y increases upward.
//   '\n' : cursor.x = 0, cursor.y -= lineHeight * scale.
//   Quad : top-left = (x0, y0), bottom-right = (x1, y1) with y0 > y1.
namespace GlyphLayoutEngine
{
    inline UiRect intersectRect(const UiRect& a, const UiRect& b)
    {
        const float left = std::max(a.x, b.x);
        const float top = std::max(a.y, b.y);
        const float right = std::min(a.x + a.width, b.x + b.width);
        const float bottom = std::min(a.y + a.height, b.y + b.height);

        if (right <= left || bottom <= top)
            return {left, top, 0.0f, 0.0f};

        return {left, top, right - left, bottom - top};
    }

    inline bool isEmptyRect(const UiRect& rect)
    {
        return rect.width <= 0.0f || rect.height <= 0.0f;
    }

    inline float glyphAdvance(const BitmapFont& font, char ch, float scale)
    {
        const auto it = font.glyphs.find(ch);
        if (it == font.glyphs.end())
            return font.lineHeight * 0.5f * scale;
        return it->second.advance * scale;
    }

    inline float measureUiTextLine(const BitmapFont& font, const std::string& text, float scale)
    {
        float width = 0.0f;
        for (char ch : text)
        {
            if (ch == '\n')
                break;
            width += glyphAdvance(font, ch, scale);
        }
        return width;
    }

    inline std::vector<std::string> splitExplicitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::string current;
        for (char ch : text)
        {
            if (ch == '\n')
            {
                lines.push_back(current);
                current.clear();
                continue;
            }
            current.push_back(ch);
        }
        lines.push_back(current);
        return lines;
    }

    inline void appendWordWrapped(const BitmapFont& font,
                                  std::vector<std::string>& lines,
                                  std::string& current,
                                  const std::string& word,
                                  float scale,
                                  float maxWidth)
    {
        if (word.empty())
            return;

        const std::string candidate = current.empty() ? word : current + " " + word;
        if (measureUiTextLine(font, candidate, scale) <= maxWidth)
        {
            current = candidate;
            return;
        }

        if (!current.empty())
        {
            lines.push_back(current);
            current.clear();
        }

        if (measureUiTextLine(font, word, scale) <= maxWidth)
        {
            current = word;
            return;
        }

        for (char ch : word)
        {
            const std::string candidateChar = current + ch;
            if (!current.empty() && measureUiTextLine(font, candidateChar, scale) > maxWidth)
            {
                lines.push_back(current);
                current.clear();
            }
            current.push_back(ch);
        }
    }

    inline std::vector<std::string> layoutUiTextLines(const BitmapFont& font,
                                                      const std::string& text,
                                                      float scale,
                                                      float maxWidth,
                                                      UiTextWrapMode wrapMode,
                                                      int maxLines)
    {
        std::vector<std::string> lines;
        if (text.empty())
            lines.push_back("");
        else if (wrapMode == UiTextWrapMode::None || maxWidth <= 0.0f)
        {
            lines = splitExplicitLines(text);
        }
        else
        {
            const std::vector<std::string> explicitLines = splitExplicitLines(text);
            for (const std::string& sourceLine : explicitLines)
            {
                if (sourceLine.empty())
                {
                    lines.push_back("");
                    continue;
                }

                std::string current;
                std::string word;
                for (char ch : sourceLine)
                {
                    if (ch == ' ' || ch == '\t')
                    {
                        appendWordWrapped(font, lines, current, word, scale, maxWidth);
                        word.clear();
                        continue;
                    }
                    word.push_back(ch);
                }

                appendWordWrapped(font, lines, current, word, scale, maxWidth);
                lines.push_back(current);
            }
        }

        if (maxLines > 0 && static_cast<int>(lines.size()) > maxLines)
            lines.resize(static_cast<size_t>(maxLines));

        return lines;
    }

    inline UiTextMeasurement measureUiText(const BitmapFont& font,
                                           const std::string& text,
                                           float scale,
                                           float maxWidth = 0.0f,
                                           UiTextWrapMode wrapMode = UiTextWrapMode::None,
                                           int maxLines = 0)
    {
        const std::vector<std::string> lines = layoutUiTextLines(
            font, text, scale, maxWidth, wrapMode, maxLines);

        UiTextMeasurement measurement;
        measurement.lineCount = static_cast<int>(lines.size());
        measurement.height = static_cast<float>(measurement.lineCount) * font.lineHeight * scale;

        for (const std::string& line : lines)
            measurement.width = std::max(measurement.width, measureUiTextLine(font, line, scale));

        return measurement;
    }

    inline void appendUiGlyphClipped(UiCommandBuffer& buffer,
                                     const BitmapFont& font,
                                     const GlyphInfo& glyph,
                                     float x0,
                                     float y0,
                                     float x1,
                                     float y1,
                                     const UiRect& clipRect,
                                     glm::vec4 color)
    {
        if (x1 <= x0 || y1 <= y0)
            return;

        const UiRect original = {x0, y0, x1 - x0, y1 - y0};
        const UiRect clipped = intersectRect(original, clipRect);
        if (isEmptyRect(clipped))
            return;

        const float uSpan = glyph.uvMax.x - glyph.uvMin.x;
        const float vSpan = glyph.uvMax.y - glyph.uvMin.y;
        const float leftT = (clipped.x - x0) / (x1 - x0);
        const float rightT = (clipped.x + clipped.width - x0) / (x1 - x0);
        const float topT = (clipped.y - y0) / (y1 - y0);
        const float bottomT = (clipped.y + clipped.height - y0) / (y1 - y0);

        buffer.addTexturedQuadUv(clipped.x,
                                 clipped.y,
                                 clipped.width,
                                 clipped.height,
                                 font.atlasTexture,
                                 {glyph.uvMin.x + uSpan * leftT,
                                  glyph.uvMin.y + vSpan * topT},
                                 {glyph.uvMin.x + uSpan * rightT,
                                  glyph.uvMin.y + vSpan * bottomT},
                                 color);
    }

    inline void appendUiTextLineClipped(UiCommandBuffer& buffer,
                                        const BitmapFont& font,
                                        const std::string& text,
                                        float x,
                                        float y,
                                        float scale,
                                        const UiRect& clipRect,
                                        glm::vec4 color)
    {
        float cursorX = x;

        for (char ch : text)
        {
            auto it = font.glyphs.find(ch);
            if (it == font.glyphs.end())
            {
                cursorX += font.lineHeight * 0.5f * scale;
                continue;
            }

            const GlyphInfo& g = it->second;

            if (ch != ' ')
            {
                const float x0 = cursorX + g.bearing.x * scale;
                const float y0 = y - (g.bearing.y - g.size.y) * scale;
                const float x1 = x0 + g.size.x * scale;
                const float y1 = y0 + g.size.y * scale;
                appendUiGlyphClipped(buffer, font, g, x0, y0, x1, y1, clipRect, color);
            }

            cursorX += g.advance * scale;
        }
    }

    inline void appendUiTextInRect(UiCommandBuffer& buffer,
                                   const BitmapFont& font,
                                   const std::string& text,
                                   const UiRect& bounds,
                                   const UiRect& clipRect,
                                   float scale,
                                   glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f},
                                   UiTextWrapMode wrapMode = UiTextWrapMode::None,
                                   UiHorizontalAlign horizontalAlign = UiHorizontalAlign::Left,
                                   UiVerticalAlign verticalAlign = UiVerticalAlign::Top,
                                   int maxLines = 0)
    {
        const bool hasTextBox = bounds.width > 0.0f && bounds.height > 0.0f;
        const UiRect effectiveBounds = hasTextBox
            ? bounds
            : UiRect{bounds.x, bounds.y, std::max(0.0f, clipRect.x + clipRect.width - bounds.x),
                     std::max(0.0f, clipRect.y + clipRect.height - bounds.y)};
        const UiRect effectiveClip = hasTextBox ? intersectRect(bounds, clipRect) : clipRect;
        if (isEmptyRect(effectiveClip))
            return;

        const std::vector<std::string> lines = layoutUiTextLines(
            font,
            text,
            scale,
            hasTextBox ? effectiveBounds.width : 0.0f,
            hasTextBox ? wrapMode : UiTextWrapMode::None,
            maxLines);
        if (lines.empty())
            return;

        const float lineHeight = font.lineHeight * scale;
        const float textHeight = static_cast<float>(lines.size()) * lineHeight;
        float y = effectiveBounds.y;
        if (hasTextBox)
        {
            switch (verticalAlign)
            {
                case UiVerticalAlign::Top:    break;
                case UiVerticalAlign::Middle: y += std::max(0.0f, (effectiveBounds.height - textHeight) * 0.5f); break;
                case UiVerticalAlign::Bottom: y += std::max(0.0f, effectiveBounds.height - textHeight); break;
            }
        }

        for (const std::string& line : lines)
        {
            const float lineWidth = measureUiTextLine(font, line, scale);
            float x = effectiveBounds.x;
            if (hasTextBox)
            {
                switch (horizontalAlign)
                {
                    case UiHorizontalAlign::Left:   break;
                    case UiHorizontalAlign::Center: x += std::max(0.0f, (effectiveBounds.width - lineWidth) * 0.5f); break;
                    case UiHorizontalAlign::Right:  x += std::max(0.0f, effectiveBounds.width - lineWidth); break;
                }
            }

            appendUiTextLineClipped(buffer, font, line, x, y, scale, effectiveClip, color);
            y += lineHeight;
        }
    }

    inline void build(const WorldTextComponent& text,
                      const BitmapFont& font,
                      std::vector<Vertex>& verts,
                      std::vector<uint32_t>& indices)
    {
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

                const glm::vec3 normal = {0.0f, 0.0f, 1.0f};
                const glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                const glm::vec4 white = {1.0f, 1.0f, 1.0f, 1.0f};

                verts.push_back({{x0, y0, 0.0f}, normal, tangent, white, {g.uvMin.x, g.uvMin.y}}); // TL
                verts.push_back({{x1, y0, 0.0f}, normal, tangent, white, {g.uvMax.x, g.uvMin.y}}); // TR
                verts.push_back({{x0, y1, 0.0f}, normal, tangent, white, {g.uvMin.x, g.uvMax.y}}); // BL
                verts.push_back({{x1, y1, 0.0f}, normal, tangent, white, {g.uvMax.x, g.uvMax.y}}); // BR

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

    // Builds screen-space glyph quads for UI rendering and appends them to buffer.
    // (x, y) = top-left of text in normalized viewport coords [0..1], Y-down.
    // scale  = size multiplier applied to glyph metrics.
    // color  = per-vertex RGBA tint (default white, fully opaque).
    inline void appendUiText(UiCommandBuffer& buffer,
                             const BitmapFont& font,
                             const std::string& text,
                             float x, float y, float scale,
                             glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        float cursorX = x;
        float cursorY = y;
        const uint32_t indexStart = static_cast<uint32_t>(buffer.indices.size());

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

                auto base = static_cast<uint32_t>(buffer.vertices.size());

                buffer.vertices.push_back({{x0, y0}, {g.uvMin.x, g.uvMin.y}, color}); // TL
                buffer.vertices.push_back({{x1, y0}, {g.uvMax.x, g.uvMin.y}, color}); // TR
                buffer.vertices.push_back({{x0, y1}, {g.uvMin.x, g.uvMax.y}, color}); // BL
                buffer.vertices.push_back({{x1, y1}, {g.uvMax.x, g.uvMax.y}, color}); // BR

                // Two CCW triangles (TL→BL→TR, TR→BL→BR) — cull mode NONE.
                buffer.indices.push_back(base + 0);
                buffer.indices.push_back(base + 2);
                buffer.indices.push_back(base + 1);
                buffer.indices.push_back(base + 1);
                buffer.indices.push_back(base + 2);
                buffer.indices.push_back(base + 3);
            }

            cursorX += g.advance * scale;
        }

        buffer.addDrawCommand(UiDrawType::TexturedQuad,
                              font.atlasTexture,
                              indexStart,
                              static_cast<uint32_t>(buffer.indices.size()) - indexStart);
    }
}
