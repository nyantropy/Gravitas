#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "GlmConfig.h"
#include "BitmapFont.h"
#include "BitmapFontLoader.h"
#include "GtsFrameStats.h"
#include "UiCommand.h"
#include "IResourceProvider.hpp"
#include "GraphicsConstants.h"
#include "GlyphLayoutEngine.h"

// Self-contained debug overlay module appended after game UI extraction.
// Appends debug stat glyph quads to a UiCommandBuffer when enabled.
// All rendering shares UiRenderStage's pipeline and draw call — no separate
// Vulkan objects needed.
//
// Font path and atlas constants are the single source of truth — changing the
// font requires editing only the constants below.
class GtsDebugOverlay
{
public:
    // Path to the font atlas used for debug text.
    static constexpr const char* DEBUG_FONT_PATH =
        GRAVITAS_ENGINE_RESOURCES "/fonts/retrofont.png";

    // Atlas layout constants — must match the font file.
    static constexpr int   ATLAS_W     = 64;
    static constexpr int   ATLAS_H     = 40;
    static constexpr int   CELL_W      = 8;
    static constexpr int   CELL_H      = 8;
    static constexpr int   ATLAS_COLS  = 8;
    static constexpr float LINE_HEIGHT = 1.2f;
    static constexpr float FONT_SCALE  = 0.03f;

    // Screen-space position of the debug overlay (normalized, top-right area).
    static constexpr float OVERLAY_X = 0.60f;
    static constexpr float OVERLAY_Y = 0.01f;

    void init(IResourceProvider* resources)
    {
        font = BitmapFontLoader::load(
            resources,
            DEBUG_FONT_PATH,
            ATLAS_W, ATLAS_H,
            CELL_W, CELL_H, ATLAS_COLS,
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ",
            LINE_HEIGHT,
            true);
        initialised = true;
    }

    void toggle() { enabled = !enabled; }
    void setEnabled(bool e) { enabled = e; }
    bool isEnabled()  const { return enabled; }

    // Appends debug stat glyph quads to the UiCommandBuffer.
    // Called after game UI quads have been extracted into the command buffer.
    void appendToBuffer(UiCommandBuffer& buffer, const GtsFrameStats& stats) const
    {
        if (!initialised) return;

        char line[64];
        float y = OVERLAY_Y;

        snprintf(line, sizeof(line), "FPS  %d",
                 static_cast<int>(stats.fps));
        appendLine(buffer, line, OVERLAY_X, y);
        y += LINE_HEIGHT * FONT_SCALE;

        snprintf(line, sizeof(line), "DT   %.1fMS",
                 stats.frameTimeMs);
        appendLine(buffer, line, OVERLAY_X, y);
        y += LINE_HEIGHT * FONT_SCALE;

        snprintf(line, sizeof(line), "VIS  %d / %d",
                 static_cast<int>(stats.visibleObjects),
                 static_cast<int>(stats.totalObjects));
        appendLine(buffer, line, OVERLAY_X, y);
        y += LINE_HEIGHT * FONT_SCALE;

        snprintf(line, sizeof(line), "TRIS %d",
                 static_cast<int>(stats.triangleCount));
        appendLine(buffer, line, OVERLAY_X, y);
    }

private:
    BitmapFont font;
    bool       enabled     = false;
    bool       initialised = false;

    void appendLine(UiCommandBuffer& buffer, const std::string& text,
                    float x, float y,
                    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}) const
    {
        GlyphLayoutEngine::appendUiText(buffer, font, text, x, y, FONT_SCALE, color);
    }
};
