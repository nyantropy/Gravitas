#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>

#include "Entity.h"
#include "GlmConfig.h"

class ECSWorld;

namespace gts::transform
{
    struct TransformInvalidationState;

    struct TransformResolveMetrics
    {
        uint32_t queuedTransforms         = 0;
        uint32_t processedTransforms      = 0;
        uint32_t updatedWorldTransforms   = 0;
        uint32_t changedRoots             = 0;
        uint32_t workItems                = 0;
        uint32_t batches                  = 0;
        uint32_t hierarchyLevels          = 0;
        uint32_t maxBatchSize             = 0;
        uint32_t duplicateWorkRemoved     = 0;
        float    averageBatchSize         = 0.0f;
        float    cpuTimeMs                = 0.0f;
        float    workCollectionCpuMs      = 0.0f;
        float    hierarchySchedulingCpuMs = 0.0f;
        float    inputGatherCpuMs         = 0.0f;
        float    matrixCalculationCpuMs   = 0.0f;
        float    changedListEmissionCpuMs = 0.0f;
        float    queueChildrenCpuMs       = 0.0f;
        float    resolveWorldCpuMs        = 0.0f;
        float    publishWorldCpuMs        = 0.0f;
    };

    class TransformWorldResolver
    {
        using TimePoint = std::chrono::steady_clock::time_point;

        public:
        static void setDetailedMetricsEnabled(bool enabled);

        TransformResolveMetrics resolve(ECSWorld& world);

        private:
        static constexpr uint32_t InvalidResultIndex = std::numeric_limits<uint32_t>::max();
        static constexpr uint32_t TargetBatchSize    = 1024;

        struct TransformWorkItem
        {
            Entity   entity         = INVALID_ENTITY;
            Entity   parent         = INVALID_ENTITY;
            uint32_t hierarchyDepth = 0;
        };

        struct TransformResult
        {
            Entity    entity = INVALID_ENTITY;
            glm::mat4 matrix = glm::mat4(1.0f);
        };

        struct BatchRange
        {
            uint32_t begin          = 0;
            uint32_t end            = 0;
            uint32_t hierarchyDepth = 0;
        };

        std::vector<TransformWorkItem> workItems;
        std::vector<TransformResult>   resolvedTransforms;
        std::vector<BatchRange>        batchRanges;
        std::vector<uint32_t>          resultIndexByEntity;
        std::vector<entity_id_type>    resultIndexTouchedEntities;
        static inline bool             detailedMetricsEnabled = false;

        static float elapsedMs(TimePoint start, TimePoint end);

        void
        collectWorkItems(ECSWorld& world, TransformInvalidationState& invalidation, TransformResolveMetrics& result);

        static uint32_t hierarchyDepth(ECSWorld& world, Entity entity);

        void scheduleBatches(TransformResolveMetrics& result);

        void prepareResultStorage();

        void calculateWorldMatrices(ECSWorld& world);

        glm::mat4 parentWorldMatrix(ECSWorld& world, Entity parent) const;

        void rememberResultIndex(Entity entity, uint32_t resultIndex);

        uint32_t resultIndexFor(Entity entity) const;

        static void publishWorldTransformStorage(ECSWorld& world, Entity entity, const glm::mat4& matrix);

        void publishWorldTransforms(ECSWorld& world, TransformResolveMetrics& result);

        static uint32_t nextWorldTransformVersion(uint32_t version);

        static void queueChildren(ECSWorld& world, Entity entity);

        static TransformResolveMetrics metrics(TimePoint startTime, TransformResolveMetrics result);
    };
} // namespace gts::transform
