#pragma once

#include "ECSControllerSystem.hpp"
#include "TransformWorldResolver.h"

namespace gts::transform
{
    class TransformSystem : public ECSControllerSystem
    {
        public:
        using Metrics = TransformResolveMetrics;

        static Metrics getLastMetrics();

        static void setDetailedMetricsEnabled(bool enabled);

        void update(const EcsControllerContext& ctx) override;

        private:
        static inline Metrics  lastMetrics{};
        TransformWorldResolver resolver;
    };
} // namespace gts::transform
