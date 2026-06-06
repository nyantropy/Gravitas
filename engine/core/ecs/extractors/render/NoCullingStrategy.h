#pragma once

#include "IVisibilityStrategy.h"

class NoCullingStrategy : public IVisibilityStrategy
{
    public:
    void filter(RenderExtractionSnapshot& snapshot) override
    {
        if (cacheValid && cachedContentVersion == snapshot.contentVersion)
            return;

        bool visibilityChanged = !cacheValid;
        for (RenderableSnapshot& renderable : snapshot.renderables)
        {
            if (!renderable.visible)
                visibilityChanged = true;
            renderable.visible = true;
        }

        if (visibilityChanged)
            snapshot.visibilityVersion += 1;

        cachedContentVersion = snapshot.contentVersion;
        cacheValid           = true;
    }

    void resetCache() override
    {
        cacheValid = false;
    }

    private:
    uint64_t cachedContentVersion = 0;
    bool     cacheValid           = false;
};
