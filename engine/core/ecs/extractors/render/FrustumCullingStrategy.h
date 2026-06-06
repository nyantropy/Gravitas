#pragma once

#include <cstddef>

#include "FrustumCuller.h"
#include "IVisibilityStrategy.h"

class FrustumCullingStrategy : public IVisibilityStrategy
{
    public:
    explicit FrustumCullingStrategy(bool enabled = true) : enabled(enabled) {}

    void filter(RenderExtractionSnapshot& snapshot) override
    {
        const FrustumPlanes& frustum = resolveFrustum(snapshot);
        if (cacheValid && cachedEnabled == enabled && cachedContentVersion == snapshot.contentVersion &&
            sameFrustum(cachedFrustum, frustum))
        {
            return;
        }

        bool visibilityChanged = !cacheValid;
        if (!enabled)
        {
            for (RenderableSnapshot& renderable : snapshot.renderables)
            {
                if (!renderable.visible)
                    visibilityChanged = true;
                renderable.visible = true;
            }
            finishFilter(snapshot, frustum, visibilityChanged);
            return;
        }

        for (RenderableSnapshot& renderable : snapshot.renderables)
        {
            bool visible = true;
            if (!renderable.hasBounds)
            {
                visible = true;
            }
            else
            {
                visible = FrustumCuller::isVisible(frustum, renderable.worldBounds.min, renderable.worldBounds.max);
            }

            if (renderable.visible != visible)
                visibilityChanged = true;
            renderable.visible = visible;
        }

        finishFilter(snapshot, frustum, visibilityChanged);
    }

    void setEnabled(bool newEnabled) override
    {
        if (enabled != newEnabled)
            cacheValid = false;
        enabled = newEnabled;
    }

    bool isEnabled() const override
    {
        return enabled;
    }

    void toggleFreeze() override
    {
        frozen     = !frozen;
        cacheValid = false;
    }

    bool isFrozen() const override
    {
        return frozen;
    }

    void resetCache() override
    {
        cacheValid         = false;
        frozenFrustumValid = false;
    }

    private:
    bool          enabled = true;
    bool          frozen  = false;
    FrustumPlanes frozenFrustum{};
    bool          frozenFrustumValid = false;
    FrustumPlanes cachedFrustum{};
    uint64_t      cachedContentVersion = 0;
    bool          cachedEnabled        = true;
    bool          cacheValid           = false;

    const FrustumPlanes& resolveFrustum(const RenderExtractionSnapshot& snapshot)
    {
        if (!frozen)
        {
            frozenFrustum      = snapshot.frustum;
            frozenFrustumValid = true;
            return snapshot.frustum;
        }

        if (!frozenFrustumValid)
        {
            frozenFrustum      = snapshot.frustum;
            frozenFrustumValid = true;
        }

        return frozenFrustum;
    }

    static bool sameFrustum(const FrustumPlanes& lhs, const FrustumPlanes& rhs)
    {
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            if (lhs[i] != rhs[i])
                return false;
        }
        return true;
    }

    void finishFilter(RenderExtractionSnapshot& snapshot, const FrustumPlanes& frustum, bool visibilityChanged)
    {
        if (visibilityChanged)
            snapshot.visibilityVersion += 1;

        cachedFrustum        = frustum;
        cachedContentVersion = snapshot.contentVersion;
        cachedEnabled        = enabled;
        cacheValid           = true;
    }
};
