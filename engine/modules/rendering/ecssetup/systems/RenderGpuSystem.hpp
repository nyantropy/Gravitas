#pragma once

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
// Model matrices are recomputed every controller tick from TransformComponent.
// This keeps GPU-facing state engine-owned and removes the need for gameplay
// code to manually toggle RenderGpuComponent dirty flags.
// A per-frame memoization cache ensures each ancestor is walked at most once,
// and a visiting set breaks any malformed cycles.
class RenderGpuSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext&) override
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
            rc.modelMatrix   = computeWorldMatrix(computeWorldMatrix, e);
            rc.dirty         = false;
            rc.readyToRender = true;
        });
    }
};
