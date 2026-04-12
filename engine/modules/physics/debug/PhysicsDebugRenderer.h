#pragma once

#include <unordered_map>
#include <vector>

#include "ECSControllerSystem.hpp"
#include "Entity.h"

class PhysicsDebugRenderer : public ECSControllerSystem
{
public:
    static constexpr int RINGS_PER_COLLIDER    = 3;
    static constexpr int SEGMENTS_PER_RING     = 12;
    static constexpr int SEGMENTS_PER_COLLIDER = RINGS_PER_COLLIDER * SEGMENTS_PER_RING;

    explicit PhysicsDebugRenderer(bool enabled = true)
        : enabled(enabled)
    {
    }

    void update(const EcsControllerContext& ctx) override;

private:
    bool enabled = true;
    std::unordered_map<entity_id_type, std::vector<Entity>> debugEntitiesByCollider;
};
