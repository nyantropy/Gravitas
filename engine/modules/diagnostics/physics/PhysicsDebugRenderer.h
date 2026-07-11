#pragma once

#include "ECSControllerSystem.hpp"

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
};
