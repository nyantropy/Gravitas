#pragma once

#include "ECSControllerSystem.hpp"
#include "SceneContext.h"
#include "TransformComponent.h"

#include "inventory/KeyCollectibleComponent.h"

class KeyRotationSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt = ctx.time->unscaledDeltaTime;

        world.forEach<KeyCollectibleComponent, TransformComponent>(
            [dt](Entity, KeyCollectibleComponent& key, TransformComponent& transform)
        {
            transform.rotation.y += key.rotationSpeed * dt;
        });
    }
};
