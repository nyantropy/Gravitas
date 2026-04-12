#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <vector>

#include "EngineConfig.h"
#include "FrustumCuller.h"
#include "IGtsExtractor.h"
#include "RenderCommand.h"
#include "RenderExtractionSnapshot.h"
#include "RenderExtractionSnapshotBuilder.hpp"

class RenderCommandExtractor : public IGtsExtractor<std::vector<RenderCommand>>
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

    explicit RenderCommandExtractor(bool frustumCullingEnabled = true)
        : frustumCullingEnabled(frustumCullingEnabled)
    {}

    Metrics getLastMetrics() const
    {
        return lastMetrics;
    }

    const std::vector<RenderCommand>& extract(const GtsExtractorContext& ctx) override
    {
        const RenderExtractionSnapshot& snapshot = snapshotBuilder.build(ctx.world);
        return extract(snapshot);
    }

    const std::vector<RenderCommand>& extract(const RenderExtractionSnapshot& snapshot)
    {
        const auto start = std::chrono::steady_clock::now();

        frameStamp += 1;
        int totalRenderables   = 0;
        int visibleRenderables = 0;
        int updatedCommands    = 0;

        const FrustumPlanes& frustum = resolveFrustum(snapshot);

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
            const bool visible = isRenderableVisible(frustum, renderable);
            const bool sortKeyChanged = !cached.initialised
                || cached.sortKey != renderable.sortKey;
            const bool needsUpdate = !cacheInitialised
                || !cached.initialised
                || cached.command.meshID != renderable.meshID
                || cached.command.textureID != renderable.textureID
                || cached.command.objectSSBOSlot != renderable.objectSSBOSlot
                || cached.command.alpha != renderable.alpha
                || cached.command.doubleSided != renderable.doubleSided
                || cached.command.cameraViewID != snapshot.cameraViewID
                || cached.command.modelMatrix != renderable.modelMatrix;

            cached.visible = visible;

            if (needsUpdate)
            {
                if (sortKeyChanged)
                    sortOrderDirty = true;

                cached.command.meshID         = renderable.meshID;
                cached.command.textureID      = renderable.textureID;
                cached.command.objectSSBOSlot = renderable.objectSSBOSlot;
                cached.command.cameraViewID   = snapshot.cameraViewID;
                cached.command.modelMatrix    = renderable.modelMatrix;
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
        const bool frustumChanged = !lastFrustumValid || !frustumsEqual(frustum, lastEvaluatedFrustum);
        const bool visibilityChanged = !cacheInitialised
            || updatedCommands > 0
            || sortOrderDirty
            || staleEntriesPruned
            || frustumChanged;
        lastMetrics.sortCpuMs = 0.0f;
        lastMetrics.sortedThisFrame = false;

        if (sortOrderDirty || !cacheInitialised)
        {
            const auto sortStart = std::chrono::steady_clock::now();
            opaqueEntityOrder = nextOpaqueEntityOrder;
            transparentEntityOrder = nextTransparentEntityOrder;

            std::sort(opaqueEntityOrder.begin(), opaqueEntityOrder.end(),
                [&](RenderableID lhs, RenderableID rhs)
                {
                    const auto& a = commandCache[lhs];
                    const auto& b = commandCache[rhs];
                    if (a.sortKey != b.sortKey)
                        return a.sortKey < b.sortKey;

                    const auto& cmdA = a.command;
                    const auto& cmdB = b.command;
                    if (cmdA.doubleSided != cmdB.doubleSided) return !cmdA.doubleSided && cmdB.doubleSided;
                    if (cmdA.meshID      != cmdB.meshID)      return cmdA.meshID < cmdB.meshID;
                    return cmdA.textureID < cmdB.textureID;
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
        lastEvaluatedFrustum = frustum;
        lastFrustumValid = true;
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

    void setFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
    bool isFrustumCullingEnabled() const { return frustumCullingEnabled; }

    void toggleFrustumFreeze() { frustumFrozen = !frustumFrozen; }
    bool isFrustumFrozen()  const { return frustumFrozen; }

    int  getLastTotalRenderables()   const { return lastTotalRenderables; }
    int  getLastVisibleRenderables() const { return lastVisibleRenderables; }

    RenderExtractionSnapshotBuilder::Metrics getSnapshotBuildMetrics() const
    {
        return snapshotBuilder.getLastMetrics();
    }

private:
    struct CachedCommandState
    {
        RenderCommand command;
        uint64_t      sortKey       = 0;
        uint64_t      lastSeenFrame = 0;
        bool          visible       = false;
        bool          initialised   = false;
    };

    bool                           frustumCullingEnabled;
    bool                           frustumFrozen = false;
    RenderExtractionSnapshotBuilder snapshotBuilder;
    FrustumPlanes                  frozenFrustum{};
    bool                           frozenFrustumValid = false;
    FrustumPlanes                  lastEvaluatedFrustum{};
    bool                           lastFrustumValid = false;
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

    const FrustumPlanes& resolveFrustum(const RenderExtractionSnapshot& snapshot)
    {
        if (!frustumFrozen)
        {
            frozenFrustum = snapshot.frustum;
            frozenFrustumValid = true;
            return snapshot.frustum;
        }

        if (!frozenFrustumValid)
        {
            frozenFrustum = snapshot.frustum;
            frozenFrustumValid = true;
        }

        return frozenFrustum;
    }

    bool isRenderableVisible(const FrustumPlanes& frustum, const RenderableSnapshot& renderable) const
    {
        if (!frustumCullingEnabled)
            return true;

        if (!renderable.hasBounds)
            return true;

        return FrustumCuller::isVisible(frustum,
                                        renderable.worldBounds.min,
                                        renderable.worldBounds.max);
    }

    static bool frustumsEqual(const FrustumPlanes& lhs, const FrustumPlanes& rhs)
    {
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            if (lhs[i].x != rhs[i].x
                || lhs[i].y != rhs[i].y
                || lhs[i].z != rhs[i].z
                || lhs[i].w != rhs[i].w)
            {
                return false;
            }
        }

        return true;
    }
};
