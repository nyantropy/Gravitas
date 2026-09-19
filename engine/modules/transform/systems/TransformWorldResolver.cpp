#include "TransformWorldResolver.h"

#include <algorithm>
#include <cstddef>

#include "ECSWorld.hpp"
#include "HierarchyComponent.h"
#include "TransformComponent.h"
#include "TransformInvalidationLifecycle.h"
#include "WorldTransformComponent.h"
#include "../runtime/detail/TransformInvalidationState.h"

namespace gts::transform
{
    void TransformWorldResolver::setDetailedMetricsEnabled(bool enabled)
    {
        detailedMetricsEnabled = enabled;
    }

    TransformResolveMetrics TransformWorldResolver::resolve(ECSWorld& world)
    {
        const auto                  startTime    = std::chrono::steady_clock::now();
        TransformInvalidationState& invalidation = transformInvalidationState(world);
        const uint32_t queuedTransforms          = static_cast<uint32_t>(invalidation.transformDirtyEntities.size());

        if (invalidation.transformDirtyEntities.empty())
            return metrics(startTime, {});

        const bool detailed = detailedMetricsEnabled;

        TransformResolveMetrics result;
        result.queuedTransforms = queuedTransforms;
        result.changedRoots     = queuedTransforms;

        const auto collectStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        collectWorkItems(world, invalidation, result);
        if (detailed)
            result.workCollectionCpuMs = elapsedMs(collectStart, std::chrono::steady_clock::now());

        const auto scheduleStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        scheduleBatches(result);
        if (detailed)
            result.hierarchySchedulingCpuMs = elapsedMs(scheduleStart, std::chrono::steady_clock::now());

        const auto gatherStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        prepareResultStorage();
        if (detailed)
            result.inputGatherCpuMs = elapsedMs(gatherStart, std::chrono::steady_clock::now());

        const auto resolveStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        calculateWorldMatrices(world);
        if (detailed)
            result.matrixCalculationCpuMs = elapsedMs(resolveStart, std::chrono::steady_clock::now());
        result.resolveWorldCpuMs = result.matrixCalculationCpuMs;

        const auto publishStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
        publishWorldTransforms(world, result);
        if (detailed)
            result.publishWorldCpuMs = elapsedMs(publishStart, std::chrono::steady_clock::now());

        clearTransformDirtyQueue(invalidation);
        return metrics(startTime, result);
    }

    float TransformWorldResolver::elapsedMs(TimePoint start, TimePoint end)
    {
        return std::chrono::duration<float, std::milli>(end - start).count();
    }

    void TransformWorldResolver::collectWorkItems(ECSWorld&                   world,
                                                  TransformInvalidationState& invalidation,
                                                  TransformResolveMetrics&    result)
    {
        workItems.clear();
        float      queueChildrenCpuMs = 0.0f;
        const bool detailed           = detailedMetricsEnabled;

        for (size_t dirtyIndex = 0; dirtyIndex < invalidation.transformDirtyEntities.size(); ++dirtyIndex)
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
        result.workItems          = static_cast<uint32_t>(workItems.size());
    }

    uint32_t TransformWorldResolver::hierarchyDepth(ECSWorld& world, Entity entity)
    {
        uint32_t depth   = 0;
        Entity   current = entity;
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

    void TransformWorldResolver::scheduleBatches(TransformResolveMetrics& result)
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
        size_t   levelBegin = 0;
        while (levelBegin < workItems.size())
        {
            const uint32_t depth    = workItems[levelBegin].hierarchyDepth;
            size_t         levelEnd = levelBegin + 1;
            while (levelEnd < workItems.size() && workItems[levelEnd].hierarchyDepth == depth)
                ++levelEnd;

            ++levelCount;
            for (size_t begin = levelBegin; begin < levelEnd; begin += TargetBatchSize)
            {
                const size_t end = std::min(begin + TargetBatchSize, levelEnd);
                batchRanges.push_back({static_cast<uint32_t>(begin), static_cast<uint32_t>(end), depth});
                result.maxBatchSize = std::max(result.maxBatchSize, static_cast<uint32_t>(end - begin));
            }
            levelBegin = levelEnd;
        }

        result.hierarchyLevels = levelCount;
        result.batches         = static_cast<uint32_t>(batchRanges.size());
        result.averageBatchSize =
            result.batches == 0 ? 0.0f : static_cast<float>(workItems.size()) / static_cast<float>(result.batches);
    }

    void TransformWorldResolver::prepareResultStorage()
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

    void TransformWorldResolver::calculateWorldMatrices(ECSWorld& world)
    {
        for (const BatchRange& batch : batchRanges)
        {
            for (uint32_t index = batch.begin; index < batch.end; ++index)
            {
                const TransformWorkItem& item   = workItems[index];
                glm::mat4                matrix = world.getComponent<TransformComponent>(item.entity).getModelMatrix();
                if (item.parent != INVALID_ENTITY)
                    matrix = parentWorldMatrix(world, item.parent) * matrix;

                const uint32_t resultIndex = static_cast<uint32_t>(resolvedTransforms.size());
                resolvedTransforms.push_back({item.entity, matrix});
                rememberResultIndex(item.entity, resultIndex);
            }
        }
    }

    glm::mat4 TransformWorldResolver::parentWorldMatrix(ECSWorld& world, Entity parent) const
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

    void TransformWorldResolver::rememberResultIndex(Entity entity, uint32_t resultIndex)
    {
        const size_t index = static_cast<size_t>(entity.id);
        if (index >= resultIndexByEntity.size())
            resultIndexByEntity.resize(index + 1, InvalidResultIndex);

        if (resultIndexByEntity[index] == InvalidResultIndex)
            resultIndexTouchedEntities.push_back(entity.id);
        resultIndexByEntity[index] = resultIndex;
    }

    uint32_t TransformWorldResolver::resultIndexFor(Entity entity) const
    {
        const size_t index = static_cast<size_t>(entity.id);
        if (index >= resultIndexByEntity.size())
            return InvalidResultIndex;
        return resultIndexByEntity[index];
    }

    void TransformWorldResolver::publishWorldTransformStorage(ECSWorld& world, Entity entity, const glm::mat4& matrix)
    {
        if (!world.hasComponent<WorldTransformComponent>(entity))
        {
            WorldTransformComponent worldTransform;
            worldTransform.matrix  = matrix;
            worldTransform.version = 1;
            world.addComponent(entity, worldTransform);
            return;
        }

        WorldTransformComponent& worldTransform = world.getComponent<WorldTransformComponent>(entity);
        worldTransform.matrix                   = matrix;
        worldTransform.version                  = nextWorldTransformVersion(worldTransform.version);
    }

    void TransformWorldResolver::publishWorldTransforms(ECSWorld& world, TransformResolveMetrics& result)
    {
        const bool detailed = detailedMetricsEnabled;
        for (const TransformResult& resolved : resolvedTransforms)
        {
            publishWorldTransformStorage(world, resolved.entity, resolved.matrix);

            const auto emitStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
            notifyWorldTransformPublished(world, resolved.entity);
            if (detailed)
                result.changedListEmissionCpuMs += elapsedMs(emitStart, std::chrono::steady_clock::now());

            result.updatedWorldTransforms += 1;
        }
    }

    uint32_t TransformWorldResolver::nextWorldTransformVersion(uint32_t version)
    {
        return version == std::numeric_limits<uint32_t>::max() ? 1 : version + 1;
    }

    void TransformWorldResolver::queueChildren(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<HierarchyComponent>(entity))
            return;

        const HierarchyComponent& hierarchy = world.getComponent<HierarchyComponent>(entity);
        for (Entity child : hierarchy.children)
            queueTransformDirty(world, child);
    }

    TransformResolveMetrics TransformWorldResolver::metrics(TimePoint startTime, TransformResolveMetrics result)
    {
        result.cpuTimeMs = elapsedMs(startTime, std::chrono::steady_clock::now());
        return result;
    }
} // namespace gts::transform
