#pragma once

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "ECSControllerSystem.hpp"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"
#include "EngineConfig.h"

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

    void update(const EcsControllerContext& ctx) override
    {
        const auto startTime = std::chrono::steady_clock::now();

        // Per-frame cache used only for hierarchy traversal: entity id → resolved NodeState.
        // Kept small — most entities in a dungeon scene have no parent.
        std::unordered_map<entity_id_type, NodeState> cache;
        cache.reserve(nonRenderableTransformCache.size());

        // In-progress set for cycle detection during hierarchy traversal.
        std::unordered_set<entity_id_type> visiting;
        visiting.reserve(transformCache.size());

        frameStamp += 1;
        uint32_t totalRenderables   = 0;
        uint32_t updatedRenderables = 0;

        // Recursively compute the world-space matrix for any entity.
        // Works for entities that may not themselves have a RenderGpuComponent
        // (e.g. invisible transform-only parents).
        //
        // PERFORMANCE CONTRACT: getModelMatrix() is called only when the transform
        // has actually changed.  Static entities pay only the cost of three vec3
        // comparisons; no trig functions are invoked on unchanged transforms.
        auto computeWorldState = [&](auto& self, Entity e) -> NodeState
        {
            // Return cached result if available (deduplication for shared parents).
            auto it = cache.find(e.id);
            if (it != cache.end())
                return it->second;

            // Cycle guard: if we are already visiting this entity, we have a cycle — break with identity.
            if (!visiting.insert(e.id).second)
                return {};

            CachedTransform* cached = nullptr;
            if (ctx.world.hasComponent<RenderGpuComponent>(e))
            {
                const auto& rc = ctx.world.getComponent<RenderGpuComponent>(e);
                if (rc.objectSSBOSlot != RENDERABLE_SLOT_UNALLOCATED)
                    cached = &transformCache[rc.objectSSBOSlot];
            }
            if (!cached)
                cached = &nonRenderableTransformCache[e.id];

            if (cached->lastSeenFrame != 0 && cached->lastSeenFrame != frameStamp - 1)
                cached->initialized = false;
            cached->lastSeenFrame = frameStamp;

            NodeState state;
            state.changed = false;

            if (ctx.world.hasComponent<TransformComponent>(e))
            {
                const auto& transform = ctx.world.getComponent<TransformComponent>(e);

                // Detect whether the local transform has changed BEFORE calling
                // getModelMatrix().  getModelMatrix() involves sin/cos and is the
                // dominant per-entity cost — only pay it when actually needed.
                const bool localChanged = !cached->initialized
                    || differs(cached->position, transform.position)
                    || differs(cached->rotation, transform.rotation)
                    || differs(cached->scale,    transform.scale);

                if (localChanged)
                {
                    cached->modelMatrix  = transform.getModelMatrix();
                    cached->position     = transform.position;
                    cached->rotation     = transform.rotation;
                    cached->scale        = transform.scale;
                    cached->initialized  = true;
                }
                // When nothing changed, reuse cached.modelMatrix — zero trig cost.

                state.worldMatrix = cached->modelMatrix;
                state.changed     = localChanged;
            }

            if (ctx.world.hasComponent<HierarchyComponent>(e))
            {
                Entity parent = ctx.world.getComponent<HierarchyComponent>(e).parent;
                if (parent != INVALID_ENTITY)
                {
                    const NodeState parentState = self(self, parent);
                    state.worldMatrix = parentState.worldMatrix * state.worldMatrix;
                    state.changed = state.changed || parentState.changed;
                }
            }

            visiting.erase(e.id);
            cache[e.id] = state;
            return state;
        };

        ctx.world.forEach<RenderGpuComponent, TransformComponent>([&](Entity e, RenderGpuComponent& rc, TransformComponent&)
        {
            totalRenderables += 1;

            const NodeState state = computeWorldState(computeWorldState, e);
            if (!state.changed && !rc.dirty && rc.readyToRender)
                return;

            rc.modelMatrix   = state.worldMatrix;
            rc.dirty         = false;
            rc.readyToRender = true;
            rc.commandDirty  = true;
            updatedRenderables += 1;
        });
        const auto endTime = std::chrono::steady_clock::now();
        lastMetrics.totalRenderables   = totalRenderables;
        lastMetrics.updatedRenderables = updatedRenderables;
        lastMetrics.cpuTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        for (auto it = nonRenderableTransformCache.begin(); it != nonRenderableTransformCache.end();)
        {
            if (it->second.lastSeenFrame == frameStamp)
            {
                ++it;
                continue;
            }

            it = nonRenderableTransformCache.erase(it);
        }
    }

private:
    struct CachedTransform
    {
        glm::vec3 position      = glm::vec3(0.0f);
        glm::vec3 rotation      = glm::vec3(0.0f);
        glm::vec3 scale         = glm::vec3(1.0f);
        // Cached result of getModelMatrix() — reused when position/rotation/scale are unchanged.
        glm::mat4 modelMatrix   = glm::mat4(1.0f);
        uint64_t  lastSeenFrame = 0;
        bool      initialized   = false;
    };

    struct NodeState
    {
        glm::mat4 worldMatrix = glm::mat4(1.0f);
        bool      changed     = true;
    };

    static inline Metrics lastMetrics{0, 0, 0.0f};

    std::vector<CachedTransform> transformCache =
        std::vector<CachedTransform>(EngineConfig::MAX_RENDERABLE_OBJECTS);
    std::unordered_map<entity_id_type, CachedTransform> nonRenderableTransformCache;
    uint64_t frameStamp = 0;

    static bool differs(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
    }
};
