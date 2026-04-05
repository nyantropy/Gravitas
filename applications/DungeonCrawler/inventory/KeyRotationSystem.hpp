#pragma once

#include "ECSControllerSystem.hpp"
#include "SceneContext.h"
#include "TransformComponent.h"

#include "inventory/CollectibleComponent.h"

class KeyRotationSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt = ctx.time->unscaledDeltaTime;

        world.forEach<CollectibleComponent, TransformComponent>(
            [dt](Entity, CollectibleComponent& collectible, TransformComponent& transform)
        {
            transform.rotation.y += collectible.rotationSpeed * dt;
        });
    }
};
