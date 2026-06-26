#pragma once

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "ECSControllerSystem.hpp"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "RenderInvalidationLifecycle.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"
#include "GraphicsConstants.h"

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
        uint32_t queuedTransformDirty;
        uint32_t processedTransformDirty;
        float    cpuTimeMs;
    };

    static Metrics getLastMetrics()
    {
        return lastMetrics;
    }

    void update(const EcsControllerContext& ctx) override
    {
        const auto startTime = std::chrono::steady_clock::now();
        gts::rendering::RenderInvalidationState& invalidation =
            gts::rendering::renderInvalidationState(ctx.world);

        if (invalidation.transformDirtyEntities.empty())
        {
            const auto endTime = std::chrono::steady_clock::now();
            lastMetrics.totalRenderables = knownRenderableCount;
            lastMetrics.updatedRenderables = 0;
            lastMetrics.queuedTransformDirty = 0;
            lastMetrics.processedTransformDirty = 0;
            lastMetrics.cpuTimeMs =
                std::chrono::duration<float, std::milli>(endTime - startTime).count();
            return;
        }

        // Per-frame cache used only for hierarchy traversal: entity id → resolved NodeState.
        // Kept small — most scenes are dominated by root renderables.
        traversalCache.clear();
        traversalCache.reserve(nonRenderableTransformCache.size());

        // In-progress set for cycle detection during hierarchy traversal.
        traversalVisiting.clear();
        traversalVisiting.reserve(transformCache.size());

        frameStamp += 1;
        const uint32_t queuedTransformDirty =
            static_cast<uint32_t>(invalidation.transformDirtyEntities.size());
        uint32_t processedTransformDirty = 0;
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
            auto it = traversalCache.find(e.id);
            if (it != traversalCache.end())
                return it->second;

            // Cycle guard: if we are already visiting this entity, we have a cycle — break with identity.
            if (!traversalVisiting.insert(e.id).second)
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

            traversalVisiting.erase(e.id);
            traversalCache[e.id] = state;
            return state;
        };

        for (size_t dirtyIndex = 0;
             dirtyIndex < invalidation.transformDirtyEntities.size();
             ++dirtyIndex)
        {
            Entity e{invalidation.transformDirtyEntities[dirtyIndex]};
            processedTransformDirty += 1;

            enqueueHierarchyChildren(ctx.world, e);

            if (!ctx.world.hasComponent<RenderGpuComponent>(e)
                || !ctx.world.hasComponent<RenderDirtyComponent>(e)
                || !ctx.world.hasComponent<TransformComponent>(e))
            {
                continue;
            }

            RenderGpuComponent& rc = ctx.world.getComponent<RenderGpuComponent>(e);
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                continue;

            RenderDirtyComponent& dirty = ctx.world.getComponent<RenderDirtyComponent>(e);
            TransformComponent& transform = ctx.world.getComponent<TransformComponent>(e);
            const bool hasHierarchy = ctx.world.hasComponent<HierarchyComponent>(e);

            if (!hasHierarchy)
            {
                CachedTransform& cached = transformCache[rc.objectSSBOSlot];

                if (cached.lastSeenFrame != 0 && cached.lastSeenFrame != frameStamp - 1)
                    cached.initialized = false;
                cached.lastSeenFrame = frameStamp;

                const bool localChanged = dirty.transformDirty
                    || !cached.initialized
                    || differs(cached.position, transform.position)
                    || differs(cached.rotation, transform.rotation)
                    || differs(cached.scale,    transform.scale);

                if (!localChanged && !rc.dirty && rc.readyToRender)
                    continue;

                if (localChanged)
                {
                    cached.modelMatrix = transform.getModelMatrix();
                    cached.position    = transform.position;
                    cached.rotation    = transform.rotation;
                    cached.scale       = transform.scale;
                    cached.initialized = true;
                }

                rc.modelMatrix   = cached.modelMatrix;
                rc.dirty         = false;
                rc.readyToRender = true;
                rc.commandDirty  = true;
                dirty.transformDirty = true;
                gts::rendering::queueRenderSnapshotDirty(ctx.world, e);
                updatedRenderables += 1;
                continue;
            }

            const NodeState state = computeWorldState(computeWorldState, e);
            if (!state.changed && !rc.dirty && rc.readyToRender)
                continue;

            rc.modelMatrix   = state.worldMatrix;
            rc.dirty         = false;
            rc.readyToRender = true;
            rc.commandDirty  = true;
            dirty.transformDirty = true;
            gts::rendering::queueRenderSnapshotDirty(ctx.world, e);
            updatedRenderables += 1;
        }

        gts::rendering::clearTransformDirtyQueue(invalidation);
        const auto endTime = std::chrono::steady_clock::now();
        knownRenderableCount = std::max(knownRenderableCount, processedTransformDirty);
        lastMetrics.totalRenderables   = knownRenderableCount;
        lastMetrics.updatedRenderables = updatedRenderables;
        lastMetrics.queuedTransformDirty = queuedTransformDirty;
        lastMetrics.processedTransformDirty = processedTransformDirty;
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

    static inline Metrics lastMetrics{};

    std::vector<CachedTransform> transformCache =
        std::vector<CachedTransform>(GraphicsConstants::MAX_RENDERABLE_OBJECTS);
    std::unordered_map<entity_id_type, CachedTransform> nonRenderableTransformCache;
    // Promoted from locals — capacity retained across frames, eliminating per-frame heap allocation.
    std::unordered_map<entity_id_type, NodeState>  traversalCache;
    std::unordered_set<entity_id_type>             traversalVisiting;
    uint64_t frameStamp = 0;
    uint32_t knownRenderableCount = 0;

    static bool differs(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
    }

    static void enqueueHierarchyChildren(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<HierarchyComponent>(entity))
            return;

        const HierarchyComponent& hierarchy = world.getComponent<HierarchyComponent>(entity);
        for (Entity child : hierarchy.children)
            gts::rendering::queueRenderTransformDirty(world, child);
    }
};
