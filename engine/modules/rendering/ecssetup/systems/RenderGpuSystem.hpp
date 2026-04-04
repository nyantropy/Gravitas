#pragma once

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "ECSControllerSystem.hpp"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"

// Controller system that keeps RenderGpuComponent::modelMatrix in sync with
// TransformComponent, accounting for parent-child hierarchies.
// Runs every frame regardless of pause state so transforms stay current.
//
// Local transform changes are detected inside this system, so gameplay does not
// need to manage RenderGpuComponent::dirty. Root entities update only when their
// own transform changes or when newly bound. Child entities also update when an
// ancestor transform changes.
// A per-frame memoization cache ensures each ancestor is walked at most once,
// and a visiting set breaks any malformed cycles.
class RenderGpuSystem : public ECSControllerSystem
{
public:
    struct Metrics
    {
        uint32_t totalRenderables;
        uint32_t updatedRenderables;
        float    cpuTimeMs;
    };

    static Metrics getLastMetrics()
    {
        return lastMetrics;
    }

    void update(ECSWorld& world, SceneContext&) override
    {
        const auto startTime = std::chrono::steady_clock::now();

        // Per-frame cache: entity id → resolved world-space model matrix
        std::unordered_map<entity_id_type, NodeState> cache;
        cache.reserve(transformCache.size());

        // In-progress set for cycle detection
        std::unordered_set<entity_id_type> visiting;
        visiting.reserve(transformCache.size());

        frameStamp += 1;
        uint32_t totalRenderables   = 0;
        uint32_t updatedRenderables = 0;

        // Recursively compute the world-space matrix for any entity.
        // Works for entities that may not themselves have a RenderGpuComponent
        // (e.g. invisible transform-only parents).
        auto computeWorldState = [&](auto& self, Entity e) -> NodeState
        {
            // Return cached result if available
            auto it = cache.find(e.id);
            if (it != cache.end())
                return it->second;

            // Cycle guard: if we are already visiting this entity, we have a cycle — break with identity
            if (!visiting.insert(e.id).second)
                return {};

            glm::mat4 local = glm::mat4(1.0f);
            bool localChanged = false;
            if (world.hasComponent<TransformComponent>(e))
            {
                const auto& transform = world.getComponent<TransformComponent>(e);
                local = transform.getModelMatrix();

                auto& cachedTransform = transformCache[e.id];
                localChanged = !cachedTransform.initialized
                    || differs(cachedTransform.position, transform.position)
                    || differs(cachedTransform.rotation, transform.rotation)
                    || differs(cachedTransform.scale, transform.scale);

                cachedTransform.position    = transform.position;
                cachedTransform.rotation    = transform.rotation;
                cachedTransform.scale       = transform.scale;
                cachedTransform.initialized = true;
                cachedTransform.lastSeenFrame = frameStamp;
            }

            NodeState state;
            state.worldMatrix = local;
            state.changed     = localChanged;
            if (world.hasComponent<HierarchyComponent>(e))
            {
                Entity parent = world.getComponent<HierarchyComponent>(e).parent;
                if (parent != INVALID_ENTITY)
                {
                    const NodeState parentState = self(self, parent);
                    state.worldMatrix = parentState.worldMatrix * local;
                    state.changed = state.changed || parentState.changed;
                }
            }

            visiting.erase(e.id);
            cache[e.id] = state;
            return state;
        };

        world.forEach<RenderGpuComponent, TransformComponent>([&](Entity e, RenderGpuComponent& rc, TransformComponent&)
        {
            totalRenderables += 1;

            const NodeState state = computeWorldState(computeWorldState, e);
            if (!state.changed && !rc.dirty && rc.readyToRender)
                return;

            rc.modelMatrix   = state.worldMatrix;
            rc.dirty         = false;
            rc.readyToRender = true;
            updatedRenderables += 1;
        });

        pruneStaleTransformCache();

        const auto endTime = std::chrono::steady_clock::now();
        lastMetrics.totalRenderables   = totalRenderables;
        lastMetrics.updatedRenderables = updatedRenderables;
        lastMetrics.cpuTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }

private:
    struct CachedTransform
    {
        glm::vec3 position      = glm::vec3(0.0f);
        glm::vec3 rotation      = glm::vec3(0.0f);
        glm::vec3 scale         = glm::vec3(1.0f);
        uint64_t  lastSeenFrame = 0;
        bool      initialized   = false;
    };

    struct NodeState
    {
        glm::mat4 worldMatrix = glm::mat4(1.0f);
        bool      changed     = true;
    };

    static inline Metrics lastMetrics{0, 0, 0.0f};

    std::unordered_map<entity_id_type, CachedTransform> transformCache;
    uint64_t frameStamp = 0;

    static bool differs(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
    }

    void pruneStaleTransformCache()
    {
        for (auto it = transformCache.begin(); it != transformCache.end(); )
        {
            if (it->second.lastSeenFrame != frameStamp)
                it = transformCache.erase(it);
            else
                ++it;
        }
    }
};
