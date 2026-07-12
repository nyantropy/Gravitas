#pragma once

#include "ECSControllerSystem.hpp"
#include "TransformWorldResolver.h"

namespace gts::transform
{
    class TransformSystem : public ECSControllerSystem
    {
    public:
        using Metrics = TransformResolveMetrics;

        static Metrics getLastMetrics()
        {
            return lastMetrics;
        }

        static void setDetailedMetricsEnabled(bool enabled)
        {
            TransformWorldResolver::setDetailedMetricsEnabled(enabled);
        }

        void update(const EcsControllerContext& ctx) override
        {
            lastMetrics = resolver.resolve(ctx.world);
        }

    private:
        static inline Metrics lastMetrics{};
        TransformWorldResolver resolver;
    };
}
