#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "ECSWorld.hpp"
#include "HierarchyComponent.h"
#include "TransformComponent.h"
#include "TransformInvalidationLifecycle.h"
#include "WorldTransformComponent.h"

namespace gts::transform
{
    struct TransformResolveMetrics
    {
        uint32_t queuedTransforms;
        uint32_t processedTransforms;
        uint32_t updatedWorldTransforms;
        float    cpuTimeMs;
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
                return metrics(startTime, 0, 0, 0);

            resolveCache.clear();
            visiting.clear();
            uint32_t processedTransforms = 0;
            uint32_t updatedWorldTransforms = 0;
            float queueChildrenCpuMs = 0.0f;
            float resolveWorldCpuMs = 0.0f;
            float publishWorldCpuMs = 0.0f;
            const bool detailed = detailedMetricsEnabled;

            for (size_t dirtyIndex = 0;
                 dirtyIndex < invalidation.transformDirtyEntities.size();
                 ++dirtyIndex)
            {
                Entity entity{invalidation.transformDirtyEntities[dirtyIndex]};
                processedTransforms += 1;
                const auto queueStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
                queueChildren(world, entity);
                if (detailed)
                {
                    const auto queueEnd = std::chrono::steady_clock::now();
                    queueChildrenCpuMs +=
                        std::chrono::duration<float, std::milli>(queueEnd - queueStart).count();
                }

                if (!world.hasComponent<TransformComponent>(entity))
                    continue;

                const auto resolveStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
                const ResolvedTransform resolved = resolveWorldTransform(world, entity);
                if (detailed)
                {
                    const auto resolveEnd = std::chrono::steady_clock::now();
                    resolveWorldCpuMs +=
                        std::chrono::duration<float, std::milli>(resolveEnd - resolveStart).count();
                }

                const auto publishStart = detailed ? std::chrono::steady_clock::now() : TimePoint{};
                syncWorldTransform(world, entity, resolved.matrix);
                if (detailed)
                {
                    const auto publishEnd = std::chrono::steady_clock::now();
                    publishWorldCpuMs +=
                        std::chrono::duration<float, std::milli>(publishEnd - publishStart).count();
                }
                updatedWorldTransforms += 1;
            }

            clearTransformDirtyQueue(invalidation);
            return metrics(startTime,
                           queuedTransforms,
                           processedTransforms,
                           updatedWorldTransforms,
                           queueChildrenCpuMs,
                           resolveWorldCpuMs,
                           publishWorldCpuMs);
        }

    private:
        struct ResolvedTransform
        {
            glm::mat4 matrix = glm::mat4(1.0f);
        };
        std::unordered_map<entity_id_type, ResolvedTransform> resolveCache;
        std::unordered_set<entity_id_type> visiting;
        static inline bool detailedMetricsEnabled = false;

        ResolvedTransform resolveWorldTransform(ECSWorld& world, Entity entity)
        {
            auto it = resolveCache.find(entity.id);
            if (it != resolveCache.end())
                return it->second;

            if (!visiting.insert(entity.id).second)
                return {};

            ResolvedTransform resolved;
            if (world.hasComponent<TransformComponent>(entity))
                resolved.matrix = world.getComponent<TransformComponent>(entity).getModelMatrix();

            if (world.hasComponent<HierarchyComponent>(entity))
            {
                const Entity parent = world.getComponent<HierarchyComponent>(entity).parent;
                if (parent != INVALID_ENTITY)
                {
                    const ResolvedTransform parentTransform = resolveWorldTransform(world, parent);
                    resolved.matrix = parentTransform.matrix * resolved.matrix;
                }
            }

            visiting.erase(entity.id);
            resolveCache[entity.id] = resolved;
            return resolved;
        }

        static void syncWorldTransform(ECSWorld& world, Entity entity, const glm::mat4& matrix)
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

        static TransformResolveMetrics metrics(const std::chrono::steady_clock::time_point& startTime,
                                               uint32_t queuedTransforms,
                                               uint32_t processedTransforms,
                                               uint32_t updatedWorldTransforms,
                                               float queueChildrenCpuMs = 0.0f,
                                               float resolveWorldCpuMs = 0.0f,
                                               float publishWorldCpuMs = 0.0f)
        {
            const auto endTime = std::chrono::steady_clock::now();
            return {
                queuedTransforms,
                processedTransforms,
                updatedWorldTransforms,
                std::chrono::duration<float, std::milli>(endTime - startTime).count(),
                queueChildrenCpuMs,
                resolveWorldCpuMs,
                publishWorldCpuMs
            };
        }
    };
}
