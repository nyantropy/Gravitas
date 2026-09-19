#include "TransformSystem.hpp"

namespace gts::transform
{
    TransformSystem::Metrics TransformSystem::getLastMetrics()
    {
        return lastMetrics;
    }

    void TransformSystem::setDetailedMetricsEnabled(bool enabled)
    {
        TransformWorldResolver::setDetailedMetricsEnabled(enabled);
    }

    void TransformSystem::update(const EcsControllerContext& ctx)
    {
        lastMetrics = resolver.resolve(ctx.world);
    }
} // namespace gts::transform
