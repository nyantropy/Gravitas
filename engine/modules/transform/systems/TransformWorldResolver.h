#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ECSWorld.hpp"
#include "HierarchyComponent.h"
#include "TransformComponent.h"
#include "TransformInvalidationLifecycle.h"
#include "WorldTransformComponent.h"

namespace gts::transform
{
    struct TransformResolveMetrics
    {
        uint32_t queuedTransforms = 0;
        uint32_t processedTransforms = 0;
        uint32_t updatedWorldTransforms = 0;
        uint32_t changedRoots = 0;
        uint32_t workItems = 0;
        uint32_t batches = 0;
        uint32_t hierarchyLevels = 0;
        uint32_t maxBatchSize = 0;
        uint32_t duplicateWorkRemoved = 0;
        float    averageBatchSize = 0.0f;
        float    cpuTimeMs = 0.0f;
        float    workCollectionCpuMs = 0.0f;
        float    hierarchySchedulingCpuMs = 0.0f;
        float    inputGatherCpuMs = 0.0f;
        float    matrixCalculationCpuMs = 0.0f;
        float    changedListEmissionCpuMs = 0.0f;
        float    queueChildrenCpuMs = 0.0f;
        float    resolveWorldCpuMs = 0.0f;
        float    publishWorldCpuMs = 0.0f;
    };

    class TransformWorldResolver
    {
        using TimePoint = std::chrono::steady_clock::time_point;

    public:
        static void setDetailedMetricsEnabled(bool enabled)
        {
            detailedMetricsEnabled = enabled;
        }

        TransformResolveMetrics resolve(ECSWorld& world)
        {
            const auto startTime = std::chrono::steady_clock::now();
            TransformInvalidationState& invalidation = transformInvalidationState(world);
            const uint32_t queuedTransforms =
                static_cast<uint32_t>(invalidation.transformDirtyEntities.size());

            if (invalidation.transformDirtyEntities.empty())
                return metrics(startTime, {});

            const bool detailed = detailedMetricsEnabled;

            TransformResolveMetrics result;
            result.queuedTransforms = queuedTransforms;
            result.changedRoots = queuedTransforms;

            const auto collectStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            collectWorkItems(world, invalidation, result);
            if (detailed)
                result.workCollectionCpuMs =
                    elapsedMs(collectStart, std::chrono::steady_clock::now());

            const auto scheduleStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            scheduleBatches(result);
            if (detailed)
                result.hierarchySchedulingCpuMs =
                    elapsedMs(scheduleStart, std::chrono::steady_clock::now());

            const auto gatherStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            prepareResultStorage();
            if (detailed)
                result.inputGatherCpuMs =
                    elapsedMs(gatherStart, std::chrono::steady_clock::now());

            const auto resolveStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            calculateWorldMatrices(world);
            if (detailed)
                result.matrixCalculationCpuMs =
                    elapsedMs(resolveStart, std::chrono::steady_clock::now());
            result.resolveWorldCpuMs = result.matrixCalculationCpuMs;

            const auto publishStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            publishWorldTransforms(world, result);
            if (detailed)
                result.publishWorldCpuMs =
                    elapsedMs(publishStart, std::chrono::steady_clock::now());

            clearTransformDirtyQueue(invalidation);
            return metrics(startTime, result);
        }

    private:
        static constexpr uint32_t InvalidResultIndex = std::numeric_limits<uint32_t>::max();
        static constexpr uint32_t TargetBatchSize = 1024;

        struct TransformWorkItem
        {
            Entity   entity = INVALID_ENTITY;
            Entity   parent = INVALID_ENTITY;
            uint32_t hierarchyDepth = 0;
        };

        struct TransformResult
        {
            Entity    entity = INVALID_ENTITY;
            glm::mat4 matrix = glm::mat4(1.0f);
        };

        struct BatchRange
        {
            uint32_t begin = 0;
            uint32_t end = 0;
            uint32_t hierarchyDepth = 0;
        };

        std::vector<TransformWorkItem> workItems;
        std::vector<TransformResult>   resolvedTransforms;
        std::vector<BatchRange>        batchRanges;
        std::vector<uint32_t>          resultIndexByEntity;
        std::vector<entity_id_type>    resultIndexTouchedEntities;
        static inline bool detailedMetricsEnabled = false;

        static float elapsedMs(TimePoint start, TimePoint end)
        {
            return std::chrono::duration<float, std::milli>(end - start).count();
        }

        void collectWorkItems(ECSWorld& world,
                              TransformInvalidationState& invalidation,
                              TransformResolveMetrics& result)
        {
            workItems.clear();
            float queueChildrenCpuMs = 0.0f;
            const bool detailed = detailedMetricsEnabled;

            for (size_t dirtyIndex = 0;
                 dirtyIndex < invalidation.transformDirtyEntities.size();
                 ++dirtyIndex)
            {
                Entity entity{invalidation.transformDirtyEntities[dirtyIndex]};
                result.processedTransforms += 1;

                const auto queueStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
                queueChildren(world, entity);
                if (detailed)
                    queueChildrenCpuMs += elapsedMs(queueStart, std::chrono::steady_clock::now());

                if (!world.hasComponent<TransformComponent>(entity))
                    continue;

                Entity parent = INVALID_ENTITY;
                if (world.hasComponent<HierarchyComponent>(entity))
                    parent = world.getComponent<HierarchyComponent>(entity).parent;

                workItems.push_back({entity, parent, hierarchyDepth(world, entity)});
            }

            result.queueChildrenCpuMs = queueChildrenCpuMs;
            result.workItems = static_cast<uint32_t>(workItems.size());
        }

        static uint32_t hierarchyDepth(ECSWorld& world, Entity entity)
        {
            uint32_t depth = 0;
            Entity current = entity;
            while (current != INVALID_ENTITY && world.hasComponent<HierarchyComponent>(current))
            {
                const Entity parent = world.getComponent<HierarchyComponent>(current).parent;
                if (parent == INVALID_ENTITY || parent.id == current.id)
                    break;

                ++depth;
                current = parent;
                if (depth > 4096)
                    break;
            }
            return depth;
        }

        void scheduleBatches(TransformResolveMetrics& result)
        {
            batchRanges.clear();
            if (workItems.empty())
                return;

            std::stable_sort(workItems.begin(),
                             workItems.end(),
                             [](const TransformWorkItem& lhs, const TransformWorkItem& rhs)
                             {
                                 if (lhs.hierarchyDepth != rhs.hierarchyDepth)
                                     return lhs.hierarchyDepth < rhs.hierarchyDepth;
                                 return lhs.entity.id < rhs.entity.id;
                             });

            uint32_t levelCount = 0;
            size_t levelBegin = 0;
            while (levelBegin < workItems.size())
            {
                const uint32_t depth = workItems[levelBegin].hierarchyDepth;
                size_t levelEnd = levelBegin + 1;
                while (levelEnd < workItems.size() && workItems[levelEnd].hierarchyDepth == depth)
                    ++levelEnd;

                ++levelCount;
                for (size_t begin = levelBegin; begin < levelEnd; begin += TargetBatchSize)
                {
                    const size_t end = std::min(begin + TargetBatchSize, levelEnd);
                    batchRanges.push_back({
                        static_cast<uint32_t>(begin),
                        static_cast<uint32_t>(end),
                        depth
                    });
                    result.maxBatchSize = std::max(
                        result.maxBatchSize,
                        static_cast<uint32_t>(end - begin));
                }
                levelBegin = levelEnd;
            }

            result.hierarchyLevels = levelCount;
            result.batches = static_cast<uint32_t>(batchRanges.size());
            result.averageBatchSize = result.batches == 0
                ? 0.0f
                : static_cast<float>(workItems.size()) / static_cast<float>(result.batches);
        }

        void prepareResultStorage()
        {
            for (entity_id_type entityId : resultIndexTouchedEntities)
            {
                const size_t index = static_cast<size_t>(entityId);
                if (index < resultIndexByEntity.size())
                    resultIndexByEntity[index] = InvalidResultIndex;
            }
            resultIndexTouchedEntities.clear();

            resolvedTransforms.clear();
            resolvedTransforms.reserve(workItems.size());
        }

        void calculateWorldMatrices(ECSWorld& world)
        {
            for (const BatchRange& batch : batchRanges)
            {
                for (uint32_t index = batch.begin; index < batch.end; ++index)
                {
                    const TransformWorkItem& item = workItems[index];
                    glm::mat4 matrix = world.getComponent<TransformComponent>(item.entity).getModelMatrix();
                    if (item.parent != INVALID_ENTITY)
                        matrix = parentWorldMatrix(world, item.parent) * matrix;

                    const uint32_t resultIndex = static_cast<uint32_t>(resolvedTransforms.size());
                    resolvedTransforms.push_back({item.entity, matrix});
                    rememberResultIndex(item.entity, resultIndex);
                }
            }
        }

        glm::mat4 parentWorldMatrix(ECSWorld& world, Entity parent) const
        {
            const uint32_t resultIndex = resultIndexFor(parent);
            if (resultIndex != InvalidResultIndex)
                return resolvedTransforms[resultIndex].matrix;

            if (world.hasComponent<WorldTransformComponent>(parent))
                return world.getComponent<WorldTransformComponent>(parent).matrix;

            if (world.hasComponent<TransformComponent>(parent))
                return world.getComponent<TransformComponent>(parent).getModelMatrix();

            return glm::mat4(1.0f);
        }

        void rememberResultIndex(Entity entity, uint32_t resultIndex)
        {
            const size_t index = static_cast<size_t>(entity.id);
            if (index >= resultIndexByEntity.size())
                resultIndexByEntity.resize(index + 1, InvalidResultIndex);

            if (resultIndexByEntity[index] == InvalidResultIndex)
                resultIndexTouchedEntities.push_back(entity.id);
            resultIndexByEntity[index] = resultIndex;
        }

        uint32_t resultIndexFor(Entity entity) const
        {
            const size_t index = static_cast<size_t>(entity.id);
            if (index >= resultIndexByEntity.size())
                return InvalidResultIndex;
            return resultIndexByEntity[index];
        }

        static void publishWorldTransformStorage(ECSWorld& world, Entity entity, const glm::mat4& matrix)
        {
            if (!world.hasComponent<WorldTransformComponent>(entity))
            {
                WorldTransformComponent worldTransform;
                worldTransform.matrix = matrix;
                worldTransform.version = 1;
                world.addComponent(entity, worldTransform);
                return;
            }

            WorldTransformComponent& worldTransform = world.getComponent<WorldTransformComponent>(entity);
            worldTransform.matrix = matrix;
            worldTransform.version = nextWorldTransformVersion(worldTransform.version);
        }

        void publishWorldTransforms(ECSWorld& world, TransformResolveMetrics& result)
        {
            const bool detailed = detailedMetricsEnabled;
            for (const TransformResult& resolved : resolvedTransforms)
            {
                publishWorldTransformStorage(world, resolved.entity, resolved.matrix);

                const auto emitStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
                notifyWorldTransformPublished(world, resolved.entity);
                if (detailed)
                    result.changedListEmissionCpuMs +=
                        elapsedMs(emitStart, std::chrono::steady_clock::now());

                result.updatedWorldTransforms += 1;
            }
        }

        static uint32_t nextWorldTransformVersion(uint32_t version)
        {
            return version == std::numeric_limits<uint32_t>::max() ? 1 : version + 1;
        }

        static void queueChildren(ECSWorld& world, Entity entity)
        {
            if (!world.hasComponent<HierarchyComponent>(entity))
                return;

            const HierarchyComponent& hierarchy = world.getComponent<HierarchyComponent>(entity);
            for (Entity child : hierarchy.children)
                queueTransformDirty(world, child);
        }

        static TransformResolveMetrics metrics(TimePoint startTime, TransformResolveMetrics result)
        {
            result.cpuTimeMs = elapsedMs(startTime, std::chrono::steady_clock::now());
            return result;
        }
    };
}
