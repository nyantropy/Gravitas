#pragma once

#include <vector>

#include "ECSControllerSystem.hpp"
#include "Entity.h"

class PhysicsDebugRenderer : public ECSControllerSystem
{
public:
    explicit PhysicsDebugRenderer(bool enabled = true)
        : enabled(enabled)
    {
    }

    void update(ECSWorld& world, SceneContext& ctx) override;

private:
    bool                enabled = true;
    std::vector<Entity> debugEntities;
};
