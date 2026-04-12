#pragma once

#include "FrustumCuller.h"
#include "IVisibilityStrategy.h"

class FrustumCullingStrategy : public IVisibilityStrategy
{
public:
    explicit FrustumCullingStrategy(bool enabled = true)
        : enabled(enabled)
    {}

    void filter(RenderExtractionSnapshot& snapshot) override
    {
        if (!enabled)
        {
            for (RenderableSnapshot& renderable : snapshot.renderables)
                renderable.visible = true;
            return;
        }

        const FrustumPlanes& frustum = resolveFrustum(snapshot);
        for (RenderableSnapshot& renderable : snapshot.renderables)
        {
            if (!renderable.hasBounds)
            {
                renderable.visible = true;
                continue;
            }

            renderable.visible = FrustumCuller::isVisible(
                frustum,
                renderable.worldBounds.min,
                renderable.worldBounds.max);
        }
    }

    void setEnabled(bool newEnabled) override
    {
        enabled = newEnabled;
    }

    bool isEnabled() const override
    {
        return enabled;
    }

    void toggleFreeze() override
    {
        frozen = !frozen;
    }

    bool isFrozen() const override
    {
        return frozen;
    }

private:
    bool          enabled = true;
    bool          frozen = false;
    FrustumPlanes frozenFrustum{};
    bool          frozenFrustumValid = false;

    const FrustumPlanes& resolveFrustum(const RenderExtractionSnapshot& snapshot)
    {
        if (!frozen)
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
};
