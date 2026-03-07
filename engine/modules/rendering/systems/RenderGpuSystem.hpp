#pragma once

#include <unordered_map>
#include <unordered_set>

#include "ECSSimulationSystem.hpp"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"

// Simulation system that keeps RenderGpuComponent::modelMatrix in sync with
// TransformComponent, accounting for parent-child hierarchies.
//
// Entities without a HierarchyComponent (or whose parent is INVALID_ENTITY)
// behave exactly as before: the matrix is only recomputed when dirty == true.
//
// Entities with a valid parent always recompute so that parent-transform changes
// propagate automatically without requiring every child to be marked dirty.
// A per-frame memoization cache ensures each ancestor is walked at most once,
// and a visiting set breaks any malformed cycles.
class RenderGpuSystem : public ECSSimulationSystem
{
public:
    void update(ECSWorld& world, float) override
    {
        // Per-frame cache: entity id → resolved world-space model matrix
        std::unordered_map<entity_id_type, glm::mat4> cache;
        // In-progress set for cycle detection
        std::unordered_set<entity_id_type> visiting;

        // Recursively compute the world-space matrix for any entity.
        // Works for entities that may not themselves have a RenderGpuComponent
        // (e.g. invisible transform-only parents).
        auto computeWorldMatrix = [&](auto& self, Entity e) -> glm::mat4
        {
            // Return cached result if available
            auto it = cache.find(e.id);
            if (it != cache.end())
                return it->second;

            // Cycle guard: if we are already visiting this entity, we have a cycle — break with identity
            if (!visiting.insert(e.id).second)
                return glm::mat4(1.0f);

            glm::mat4 local = glm::mat4(1.0f);
            if (world.hasComponent<TransformComponent>(e))
                local = world.getComponent<TransformComponent>(e).getModelMatrix();

            glm::mat4 result = local;
            if (world.hasComponent<HierarchyComponent>(e))
            {
                Entity parent = world.getComponent<HierarchyComponent>(e).parent;
                if (parent != INVALID_ENTITY)
                    result = self(self, parent) * local;
            }

            visiting.erase(e.id);
            cache[e.id] = result;
            return result;
        };

        world.forEach<RenderGpuComponent, TransformComponent>([&](Entity e, RenderGpuComponent& rc, TransformComponent&)
        {
            // Entities with a live parent always recompute so parent transforms propagate.
            // Root entities (no parent) only recompute when their dirty flag is set.
            bool hasParent = world.hasComponent<HierarchyComponent>(e) &&
                             world.getComponent<HierarchyComponent>(e).parent != INVALID_ENTITY;

            if (!rc.dirty && !hasParent)
                return;

            rc.modelMatrix   = computeWorldMatrix(computeWorldMatrix, e);
            rc.dirty         = false;
            rc.readyToRender = true;
        });
    }
};
