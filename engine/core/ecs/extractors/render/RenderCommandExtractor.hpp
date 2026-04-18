#pragma once

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>

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

        nextOpaqueEntityOrder.clear();
        nextTransparentEntityOrder.clear();
        nextOpaqueEntityOrder.reserve(snapshot.renderables.size());
        nextTransparentEntityOrder.reserve(snapshot.renderables.size());
        commandCache.reserve(snapshot.renderables.size());

        for (const RenderableSnapshot& renderable : snapshot.renderables)
        {
            const RenderableID renderableID = renderable.id;

            ++totalRenderables;

            CachedCommandState& cached = commandCache[renderableID];
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
                nextOpaqueEntityOrder.push_back(renderableID);
            else
                nextTransparentEntityOrder.push_back(renderableID);
        }

        const bool staleEntriesPruned = invalidateStaleSlots();
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
            opaqueEntityOrder = nextOpaqueEntityOrder;
            transparentEntityOrder = nextTransparentEntityOrder;

            // sortKey encodes (doubleSided << 63 | meshID << 32 | textureID) — no fallback needed.
            std::sort(opaqueEntityOrder.begin(), opaqueEntityOrder.end(),
                [&](RenderableID lhs, RenderableID rhs)
                {
                    return commandCache[lhs].sortKey < commandCache[rhs].sortKey;
                });
            sortOrderDirty = false;

            const auto sortEnd = std::chrono::steady_clock::now();
            lastMetrics.sortCpuMs =
                std::chrono::duration<float, std::milli>(sortEnd - sortStart).count();
            lastMetrics.sortedThisFrame = !opaqueEntityOrder.empty();
        }

        if (visibilityChanged || !cacheInitialised)
        {
            cachedVisibleCommands.clear();
            cachedVisibleCommands.reserve(opaqueEntityOrder.size() + transparentEntityOrder.size());
            for (RenderableID renderableID : opaqueEntityOrder)
            {
                if (commandCache[renderableID].visible)
                    cachedVisibleCommands.push_back(commandCache[renderableID].command);
            }
            for (RenderableID renderableID : transparentEntityOrder)
            {
                if (commandCache[renderableID].visible)
                    cachedVisibleCommands.push_back(commandCache[renderableID].command);
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
    std::unordered_map<RenderableID, CachedCommandState> commandCache;
    std::vector<RenderableID>      opaqueEntityOrder;
    std::vector<RenderableID>      transparentEntityOrder;
    std::vector<RenderableID>      nextOpaqueEntityOrder;
    std::vector<RenderableID>      nextTransparentEntityOrder;
    std::vector<RenderCommand>     cachedVisibleCommands;
    uint64_t                       frameStamp = 0;
    bool                           cacheInitialised = false;
    bool                           sortOrderDirty = true;

    bool invalidateStaleSlots()
    {
        bool pruned = false;
        for (auto it = commandCache.begin(); it != commandCache.end(); )
        {
            if (it->second.lastSeenFrame == frameStamp)
            {
                ++it;
                continue;
            }

            it = commandCache.erase(it);
            pruned = true;
        }

        if (pruned)
            sortOrderDirty = true;

        return pruned;
    }
};
