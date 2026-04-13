#pragma once

#include <cstdio>
#include <string>
#include <vulkan/vulkan.h>

#include "GlmConfig.h"
#include "BitmapFont.h"
#include "BitmapFontLoader.h"
#include "RetroFontAtlas.h"
#include "GtsFrameStats.h"
#include "UiCommand.h"
#include "IResourceProvider.hpp"
#include "GraphicsConstants.h"
#include "GlyphLayoutEngine.h"

// ---------------------------------------------------------------------------
// RollingStat — lightweight sliding-window statistics, fully stack-allocated.
// ---------------------------------------------------------------------------
struct RollingStat
{
    static constexpr int WINDOW = 120;

    float buf[WINDOW] = {};
    int   head  = 0;
    int   count = 0;
    float sum   = 0.0f;
    float cur   = 0.0f;

    void push(float v)
    {
        cur   = v;
        sum  -= buf[head];   // evict oldest sample from sum
        buf[head] = v;
        head  = (head + 1) % WINDOW;
        if (count < WINDOW) ++count;
        sum  += v;
    }

    float current() const { return cur; }
    float average() const { return count > 0 ? sum / static_cast<float>(count) : 0.0f; }
    float max() const
    {
        float m = 0.0f;
        for (int i = 0; i < count; ++i)
            if (buf[i] > m) m = buf[i];
        return m;
    }
};

// ---------------------------------------------------------------------------
// OverlayPage — four focused views cycled with Tab while F3 is open.
// ---------------------------------------------------------------------------
enum class OverlayPage : int
{
    Summary        = 0,
    CpuProfiling   = 1,
    RenderPipeline = 2,
    UiEngine       = 3,
    COUNT          = 4
};

// ---------------------------------------------------------------------------
// GtsDebugOverlay — multi-page profiling HUD.
//
// Screen layout: OVERLAY_X=0.60 → ~15 chars per line at FONT_SCALE=0.026.
// All page renders stay within that constraint.
//
// Call update() every frame regardless of visibility so rolling averages
// are warm when the overlay is first toggled on.
// ---------------------------------------------------------------------------
class GtsDebugOverlay
{
public:
    static constexpr const char* DEBUG_FONT_PATH =
        GRAVITAS_ENGINE_RESOURCES "/fonts/retrofont.png";

    static constexpr int   ATLAS_W    = RetroFontAtlas::ATLAS_W;
    static constexpr int   ATLAS_H    = RetroFontAtlas::ATLAS_H;
    static constexpr int   CELL_W     = RetroFontAtlas::CELL_W;
    static constexpr int   CELL_H     = RetroFontAtlas::CELL_H;
    static constexpr int   ATLAS_COLS = RetroFontAtlas::ATLAS_COLS;
    static constexpr float LINE_HEIGHT = RetroFontAtlas::LINE_HEIGHT;
    static constexpr float FONT_SCALE  = 0.026f;

    static constexpr float OVERLAY_X = 0.60f;
    static constexpr float OVERLAY_Y = 0.01f;

    void init(IResourceProvider* resources)
    {
        font = BitmapFontLoader::load(
            resources,
            DEBUG_FONT_PATH,
            ATLAS_W, ATLAS_H,
            CELL_W, CELL_H, ATLAS_COLS,
            std::string(RetroFontAtlas::CHAR_ORDER),
            LINE_HEIGHT,
            true);
        initialised = true;
    }

    void toggle()          { enabled = !enabled; }
    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const  { return enabled; }

    // Advance to next page — wraps around.
    void nextPage()
    {
        int p = static_cast<int>(currentPage) + 1;
        if (p >= static_cast<int>(OverlayPage::COUNT))
            p = 0;
        currentPage = static_cast<OverlayPage>(p);
    }

    // Push current frame's stats into rolling windows.
    // Called every frame from UiRenderStage regardless of enabled state.
    void update(const GtsFrameStats& s)
    {
        rsFrameTime.push(s.frameTimeMs);
        rsFrameCpu .push(s.frameCpuMs);
        rsGpu      .push(s.gpuFrameMs);
        rsSim      .push(s.simulationCpuMs);
        rsCtrl     .push(s.controllerCpuMs);
        rsSnap     .push(s.snapshotBuildCpuMs);
        rsVis      .push(s.visibilityCpuMs);
        rsExt      .push(s.renderExtractCpuMs);
        rsSort     .push(s.renderExtractSortCpuMs);
        rsUi       .push(s.uiCpuMs);
        rsSubmit   .push(s.renderSubmitCpuMs);
    }

    // Render the current page into the command buffer.
    // Pure render — does NOT update rolling stats.
    void appendToBuffer(UiCommandBuffer& buffer, const GtsFrameStats& stats) const
    {
        if (!initialised) return;

        const float la = LINE_HEIGHT * FONT_SCALE;
        float y = OVERLAY_Y;

        // Page header — yellow, shows page name and number
        {
            char hdr[32];
            const char* names[] = { "SUMMARY", "CPU PROF", "RENDER", "UI+ENG" };
            int pn = static_cast<int>(currentPage);
            snprintf(hdr, sizeof(hdr), "[%-8s]%d/4", names[pn], pn + 1);
            appendLine(buffer, hdr, OVERLAY_X, y, {1.0f, 0.9f, 0.3f, 1.0f});
            y += la * 1.35f;
        }

        switch (currentPage)
        {
            case OverlayPage::Summary:        drawSummary(buffer, stats, y, la);        break;
            case OverlayPage::CpuProfiling:   drawCpuProfiling(buffer, y, la);          break;
            case OverlayPage::RenderPipeline: drawRenderPipeline(buffer, stats, y, la); break;
            case OverlayPage::UiEngine:       drawUiEngine(buffer, stats, y, la);       break;
        }
    }

private:
    BitmapFont  font;
    bool        enabled     = false;
    bool        initialised = false;
    OverlayPage currentPage = OverlayPage::Summary;

    // Rolling stat buckets
    RollingStat rsFrameTime;
    RollingStat rsFrameCpu;
    RollingStat rsGpu;
    RollingStat rsSim;
    RollingStat rsCtrl;
    RollingStat rsSnap;
    RollingStat rsVis;
    RollingStat rsExt;
    RollingStat rsSort;
    RollingStat rsUi;
    RollingStat rsSubmit;

    // -----------------------------------------------------------------------
    // Page 1 — Summary
    // -----------------------------------------------------------------------
    void drawSummary(UiCommandBuffer& buf,
                     const GtsFrameStats& s,
                     float y, float la) const
    {
        char ln[32];

        snprintf(ln, sizeof(ln), "FPS  %d", static_cast<int>(s.fps));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "DT   %.1fMS", rsFrameTime.current());
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "CPU  %.1fMS", rsFrameCpu.current());
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        if (s.gpuFrameMs > 0.0f)
            snprintf(ln, sizeof(ln), "GPU  %.1fMS", rsGpu.current());
        else
            snprintf(ln, sizeof(ln), "GPU  N/A");
        appendLine(buf, ln, OVERLAY_X, y); y += la * 1.3f;

        snprintf(ln, sizeof(ln), "VIS  %d/%d",
                 static_cast<int>(s.visibleObjects),
                 static_cast<int>(s.totalObjects));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "TRIS %d", static_cast<int>(s.triangleCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "DC   %d", static_cast<int>(s.drawCalls));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "ENTS %d", static_cast<int>(s.sceneEntityCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "SYS  %d/%d",
                 static_cast<int>(s.controllerSystemCount),
                 static_cast<int>(s.simulationSystemCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        drawFooter(buf, y + la * 0.8f);
    }

    // -----------------------------------------------------------------------
    // Page 2 — CPU Profiling  (cur / avg / max per bucket)
    //
    // Font is monospace: advance=1.0, so each char = FONT_SCALE = 0.026 wide.
    // Available: 0.40 / 0.026 ≈ 15 chars.
    // Format "%-3s %3.1f %3.1f %3.1f" → 15 chars at single-digit ms values.
    // -----------------------------------------------------------------------
    void drawCpuProfiling(UiCommandBuffer& buf,
                          float y, float la) const
    {
        char ln[32];

        appendLine(buf, "    CUR AVG MAX", OVERLAY_X, y,
                   {0.65f, 0.65f, 0.65f, 1.0f});
        y += la;

        auto row = [&](const char* label, const RollingStat& rs,
                       glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f})
        {
            snprintf(ln, sizeof(ln), "%-3s %3.1f %3.1f %3.1f",
                     label,
                     rs.current(),
                     rs.average(),
                     rs.max());
            appendLine(buf, ln, OVERLAY_X, y, col);
            y += la;
        };

        row("SIM", rsSim);
        row("CTL", rsCtrl);
        row("SNP", rsSnap);
        row("VIS", rsVis);
        row("EXT", rsExt);
        row("SRT", rsSort);
        row("UI ", rsUi);
        row("SUB", rsSubmit);

        y += la * 0.3f;
        // Frame total highlighted
        row("FRM", rsFrameCpu, {1.0f, 0.9f, 0.6f, 1.0f});

        drawFooter(buf, y + la * 0.5f);
    }

    // -----------------------------------------------------------------------
    // Page 3 — Render Pipeline
    // -----------------------------------------------------------------------
    void drawRenderPipeline(UiCommandBuffer& buf,
                            const GtsFrameStats& s,
                            float y, float la) const
    {
        char ln[32];

        snprintf(ln, sizeof(ln), "VIS  %d/%d",
                 static_cast<int>(s.visibleObjects),
                 static_cast<int>(s.totalObjects));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "TRIS %d", static_cast<int>(s.triangleCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "DC   %d", static_cast<int>(s.drawCalls));
        appendLine(buf, ln, OVERLAY_X, y); y += la * 1.3f;

        // Render command cache breakdown
        snprintf(ln, sizeof(ln), "CMD V/T %d/%d",
                 static_cast<int>(s.renderCommandVisitedCount),
                 static_cast<int>(s.renderCommandTotalCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "CMD U/S %d/%d",
                 static_cast<int>(s.renderCommandUpdatedCount),
                 static_cast<int>(s.renderCommandSortedCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "GPU UPD %d",
                 static_cast<int>(s.renderGpuUpdatedCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "PSW %d TSW %d",
                 static_cast<int>(s.pipelineSwitches),
                 static_cast<int>(s.textureSwitches));
        appendLine(buf, ln, OVERLAY_X, y); y += la * 1.3f;

        // Snapshot static/dynamic split (from previous task)
        snprintf(ln, sizeof(ln), "SNP S/D %d/%d",
                 static_cast<int>(s.snapshotStaticCount),
                 static_cast<int>(s.snapshotDynamicCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "SNP D/R %d/%d",
                 static_cast<int>(s.snapshotDirtyCount),
                 static_cast<int>(s.snapshotReusedCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la * 1.3f;

        snprintf(ln, sizeof(ln), "WRT %d/%d",
                 static_cast<int>(s.backendObjectWrites),
                 static_cast<int>(s.backendObjectWritesSkipped));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "PMOD %s", presentModeLabel(s.backendPresentMode));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        drawFooter(buf, y + la * 0.8f);
    }

    // -----------------------------------------------------------------------
    // Page 4 — UI & Engine detail
    // -----------------------------------------------------------------------
    void drawUiEngine(UiCommandBuffer& buf,
                      const GtsFrameStats& s,
                      float y, float la) const
    {
        char ln[32];
        const glm::vec4 hdrCol{1.0f, 0.9f, 0.3f, 1.0f};

        appendLine(buf, "[UI]", OVERLAY_X, y, hdrCol); y += la;

        snprintf(ln, sizeof(ln), "LYT  %.2fMS", s.uiLayoutCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "VIS  %.2fMS", s.uiVisualCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "CMD  %.2fMS", s.uiCommandCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "TOT  %.2fMS", s.uiCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "N/P/C %d/%d/%d",
                 static_cast<int>(s.uiNodeCount),
                 static_cast<int>(s.uiPrimitiveCount),
                 static_cast<int>(s.uiCommandCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "MMAP %d", static_cast<int>(s.minimapCellCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la * 1.3f;

        appendLine(buf, "[ENGINE]", OVERLAY_X, y, hdrCol); y += la;

        snprintf(ln, sizeof(ln), "SYS  %d/%d",
                 static_cast<int>(s.controllerSystemCount),
                 static_cast<int>(s.simulationSystemCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "PHYS %d", static_cast<int>(s.physicsCollisionCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "PCOL %d", static_cast<int>(s.playerCollisionCount));
        appendLine(buf, ln, OVERLAY_X, y); y += la * 1.3f;

        appendLine(buf, "[BACKEND]", OVERLAY_X, y, hdrCol); y += la;

        snprintf(ln, sizeof(ln), "FRM  %.2fMS", s.backendFrameCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "FENC %.2fMS", s.backendFenceWaitCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "ACQ  %.2fMS", s.backendAcquireCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "REC  %.2fMS", s.backendCmdRecordCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "SUBM %.2fMS", s.backendQueueSubmitCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "PRS  %.2fMS", s.backendPresentCpuMs);
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        snprintf(ln, sizeof(ln), "PMOD %s", presentModeLabel(s.backendPresentMode));
        appendLine(buf, ln, OVERLAY_X, y); y += la;

        drawFooter(buf, y + la * 0.5f);
    }

    // -----------------------------------------------------------------------
    // Shared footer — dim hint shown at the bottom of every page
    // -----------------------------------------------------------------------
    void drawFooter(UiCommandBuffer& buf, float y) const
    {
        appendLine(buf, "F3:HIDE TAB:NEXT", OVERLAY_X, y,
                   {0.45f, 0.45f, 0.45f, 1.0f});
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    static const char* presentModeLabel(uint32_t mode)
    {
        switch (static_cast<VkPresentModeKHR>(mode))
        {
            case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "IMMD";
            case VK_PRESENT_MODE_MAILBOX_KHR:      return "MBX";
            case VK_PRESENT_MODE_FIFO_KHR:         return "FIFO";
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FREL";
            default:                               return "UNK";
        }
    }

    void appendLine(UiCommandBuffer& buffer, const std::string& text,
                    float x, float y,
                    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}) const
    {
        GlyphLayoutEngine::appendUiText(buffer, font, text, x, y, FONT_SCALE, color);
    }
};
