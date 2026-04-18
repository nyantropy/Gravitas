#pragma once

#include <algorithm>
#include <chrono>
#include <vector>

#include "EngineConfig.h"
#include "RenderCommand.h"
#include "RenderExtractionSnapshot.h"

class RenderCommandExtractor
{
public:
    struct Metrics
    {
        float    extractCpuMs       = 0.0f;
        float    sortCpuMs          = 0.0f;
        uint32_t visitedRenderables = 0;
        uint32_t totalRenderables   = 0;
        uint32_t visibleRenderables = 0;
        uint32_t updatedCommands    = 0;
        uint32_t cachedCommands     = 0;
        bool     sortedThisFrame    = false;
    };

    Metrics getLastMetrics() const
    {
        return lastMetrics;
    }

    const std::vector<RenderCommand>& extract(const RenderExtractionSnapshot& snapshot)
    {
        const auto start = std::chrono::steady_clock::now();

        frameStamp += 1;
        int totalRenderables   = 0;
        int visibleRenderables = 0;
        int updatedCommands    = 0;
        bool visibilityStatesChanged = false;

        nextOpaqueSlotOrder.clear();
        nextTransparentSlotOrder.clear();
        nextOpaqueSlotOrder.reserve(snapshot.renderables.size());
        nextTransparentSlotOrder.reserve(snapshot.renderables.size());

        for (const RenderableSnapshot& renderable : snapshot.renderables)
        {
            const ssbo_id_type slot = renderable.objectSSBOSlot;

            ++totalRenderables;

            CachedCommandState& cached = commandCache[slot];
            if (!cached.initialised)
                occupiedSlots.push_back(slot);
            cached.lastSeenFrame = frameStamp;

            const bool opaque = renderable.alpha >= 1.0f;
            const bool visible = renderable.visible;
            const bool sortKeyChanged = !cached.initialised
                || cached.sortKey != renderable.sortKey;
            const bool needsUpdate = !cacheInitialised
                || !cached.initialised
                || cached.command.meshID != renderable.meshID
                || cached.command.textureID != renderable.textureID
                || cached.command.objectSSBOSlot != renderable.objectSSBOSlot
                || cached.command.alpha != renderable.alpha
                || cached.command.doubleSided != renderable.doubleSided
                || cached.command.cameraViewID != snapshot.cameraViewID;

            if (!cached.initialised || cached.visible != visible)
                visibilityStatesChanged = true;

            cached.visible = visible;

            if (needsUpdate)
            {
                if (sortKeyChanged)
                    sortOrderDirty = true;

                cached.command.meshID         = renderable.meshID;
                cached.command.textureID      = renderable.textureID;
                cached.command.objectSSBOSlot = renderable.objectSSBOSlot;
                cached.command.cameraViewID   = snapshot.cameraViewID;
                cached.command.alpha          = renderable.alpha;
                cached.command.doubleSided    = renderable.doubleSided;
                cached.sortKey                = renderable.sortKey;
                cached.initialised            = true;
                updatedCommands += 1;
            }

            if (visible)
                ++visibleRenderables;
            if (opaque)
                nextOpaqueSlotOrder.push_back(slot);
            else
                nextTransparentSlotOrder.push_back(slot);
        }

        const bool staleEntriesPruned = pruneStaleSlots();
        const bool visibilityChanged = !cacheInitialised
            || updatedCommands > 0
            || sortOrderDirty
            || staleEntriesPruned
            || visibilityStatesChanged;
        lastMetrics.sortCpuMs = 0.0f;
        lastMetrics.sortedThisFrame = false;

        if (sortOrderDirty || !cacheInitialised)
        {
            const auto sortStart = std::chrono::steady_clock::now();
            opaqueSlotOrder = nextOpaqueSlotOrder;
            transparentSlotOrder = nextTransparentSlotOrder;

            // sortKey encodes (doubleSided << 63 | meshID << 32 | textureID) — no fallback needed.
            std::sort(opaqueSlotOrder.begin(), opaqueSlotOrder.end(),
                [&](ssbo_id_type lhs, ssbo_id_type rhs)
                {
                    return commandCache[lhs].sortKey < commandCache[rhs].sortKey;
                });
            sortOrderDirty = false;

            const auto sortEnd = std::chrono::steady_clock::now();
            lastMetrics.sortCpuMs =
                std::chrono::duration<float, std::milli>(sortEnd - sortStart).count();
            lastMetrics.sortedThisFrame = !opaqueSlotOrder.empty();
        }

        if (visibilityChanged || !cacheInitialised)
        {
            cachedVisibleCommands.clear();
            cachedVisibleCommands.reserve(opaqueSlotOrder.size() + transparentSlotOrder.size());
            for (ssbo_id_type slot : opaqueSlotOrder)
            {
                if (commandCache[slot].visible)
                    cachedVisibleCommands.push_back(commandCache[slot].command);
            }
            for (ssbo_id_type slot : transparentSlotOrder)
            {
                if (commandCache[slot].visible)
                    cachedVisibleCommands.push_back(commandCache[slot].command);
            }
        }

        lastTotalRenderables   = totalRenderables;
        lastVisibleRenderables = visibleRenderables;
        cacheInitialised = true;
        const auto end = std::chrono::steady_clock::now();
        lastMetrics.extractCpuMs =
            std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.visitedRenderables = static_cast<uint32_t>(snapshot.renderables.size());
        lastMetrics.totalRenderables = static_cast<uint32_t>(totalRenderables);
        lastMetrics.visibleRenderables = static_cast<uint32_t>(visibleRenderables);
        lastMetrics.updatedCommands = static_cast<uint32_t>(updatedCommands);
        lastMetrics.cachedCommands  = static_cast<uint32_t>(cachedVisibleCommands.size());

        return cachedVisibleCommands;
    }

    int  getLastTotalRenderables()   const { return lastTotalRenderables; }
    int  getLastVisibleRenderables() const { return lastVisibleRenderables; }

private:
    struct CachedCommandState
    {
        RenderCommand command;
        uint64_t      sortKey       = 0;
        uint64_t      lastSeenFrame = 0;
        bool          visible       = false;
        bool          initialised   = false;
    };

    int                            lastTotalRenderables = 0;
    int                            lastVisibleRenderables = 0;
    Metrics                        lastMetrics;
    // Flat cache indexed by SSBO slot — O(1) access, contiguous memory, no hash overhead.
    // Sized to MAX_RENDERABLE_OBJECTS; stale entries detected via frame-stamp, pruned via occupiedSlots.
    std::vector<CachedCommandState> commandCache =
        std::vector<CachedCommandState>(EngineConfig::MAX_RENDERABLE_OBJECTS);
    std::vector<ssbo_id_type>      occupiedSlots;
    std::vector<ssbo_id_type>      opaqueSlotOrder;
    std::vector<ssbo_id_type>      transparentSlotOrder;
    std::vector<ssbo_id_type>      nextOpaqueSlotOrder;
    std::vector<ssbo_id_type>      nextTransparentSlotOrder;
    std::vector<RenderCommand>     cachedVisibleCommands;
    uint64_t                       frameStamp = 0;
    bool                           cacheInitialised = false;
    bool                           sortOrderDirty = true;

    bool pruneStaleSlots()
    {
        bool pruned = false;
        size_t write = 0;
        for (size_t read = 0; read < occupiedSlots.size(); ++read)
        {
            const ssbo_id_type slot = occupiedSlots[read];
            if (commandCache[slot].lastSeenFrame == frameStamp)
            {
                occupiedSlots[write++] = slot;
                continue;
            }

            commandCache[slot] = CachedCommandState{};
            pruned = true;
        }
        occupiedSlots.resize(write);

        if (pruned)
            sortOrderDirty = true;

        return pruned;
    }
};
