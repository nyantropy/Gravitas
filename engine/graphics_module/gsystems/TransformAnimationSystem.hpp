#pragma once
#include "ECSSystem.hpp"
#include "TransformComponent.h"

// this system makes all entities in the scene rotate slowly
class TransformAnimationSystem : public ECSSystem
{
    public:
        void update(ECSWorld& world, float dt) override
        {
            static float rotationAngle = 0.0f;
            rotationAngle += dt * glm::radians(45.0f);

            for (Entity e : world.getAllEntitiesWith<TransformComponent>())
            {
                auto& transform = world.getComponent<TransformComponent>(e);
                transform.rotation.y = rotationAngle;
            }
        }
};
