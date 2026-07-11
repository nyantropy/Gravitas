#pragma once

#include <chrono>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "HierarchyComponent.h"
#include "TransformComponent.h"
#include "TransformInvalidationLifecycle.h"
#include "WorldTransformComponent.h"

namespace gts::transform
{
    class TransformSystem : public ECSControllerSystem
    {
    public:
        struct Metrics
        {
            uint32_t queuedTransforms;
            uint32_t processedTransforms;
            uint32_t updatedWorldTransforms;
            float    cpuTimeMs;
        };

        static Metrics getLastMetrics()
        {
            return lastMetrics;
        }

        void update(const EcsControllerContext& ctx) override
        {
            const auto startTime = std::chrono::steady_clock::now();
            TransformInvalidationState& invalidation = transformInvalidationState(ctx.world);
            const uint32_t queuedTransforms =
                static_cast<uint32_t>(invalidation.transformDirtyEntities.size());

            if (invalidation.transformDirtyEntities.empty())
            {
                publishMetrics(startTime, 0, 0, 0);
                return;
            }

            resolveCache.clear();
            visiting.clear();
            uint32_t processedTransforms = 0;
            uint32_t updatedWorldTransforms = 0;

            for (size_t dirtyIndex = 0;
                 dirtyIndex < invalidation.transformDirtyEntities.size();
                 ++dirtyIndex)
            {
                Entity entity{invalidation.transformDirtyEntities[dirtyIndex]};
                processedTransforms += 1;
                queueChildren(ctx.world, entity);

                if (!ctx.world.hasComponent<TransformComponent>(entity))
                    continue;

                const ResolvedTransform resolved = resolveWorldTransform(ctx.world, entity);
                syncWorldTransform(ctx.world, entity, resolved.matrix);
                updatedWorldTransforms += 1;
            }

            clearTransformDirtyQueue(invalidation);
            publishMetrics(startTime, queuedTransforms, processedTransforms, updatedWorldTransforms);
        }

    private:
        struct ResolvedTransform
        {
            glm::mat4 matrix = glm::mat4(1.0f);
        };

        static inline Metrics lastMetrics{};

        std::unordered_map<entity_id_type, ResolvedTransform> resolveCache;
        std::unordered_set<entity_id_type> visiting;

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
                world.commands().addComponent<WorldTransformComponent>(entity, worldTransform);
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

        static void publishMetrics(const std::chrono::steady_clock::time_point& startTime,
                                   uint32_t queuedTransforms,
                                   uint32_t processedTransforms,
                                   uint32_t updatedWorldTransforms)
        {
            const auto endTime = std::chrono::steady_clock::now();
            lastMetrics.queuedTransforms = queuedTransforms;
            lastMetrics.processedTransforms = processedTransforms;
            lastMetrics.updatedWorldTransforms = updatedWorldTransforms;
            lastMetrics.cpuTimeMs =
                std::chrono::duration<float, std::milli>(endTime - startTime).count();
        }
    };
}
