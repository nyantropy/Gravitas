#pragma once
#include "ECSWorld.hpp"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "ECSSystem.hpp"
#include "GtsKey.h"
#include "gtsinput.h"

// a default camera control system
// can be replaced by a more dedicated system 
class CameraControlSystem : public ECSSystem
{
    public:
        void update(ECSWorld& world, float dt) override
        {
            for (Entity e : world.getAllEntitiesWith<CameraComponent, TransformComponent>())
            {
                auto& transform = world.getComponent<TransformComponent>(e);

                const float speed = 5.0f;

                // just some random transformations to check if the logic works
                if (gtsinput::isKeyDown(GtsKey::W))
                    transform.position.z -= speed * dt;

                if (gtsinput::isKeyDown(GtsKey::S))
                    transform.position.z += speed * dt;

                if (gtsinput::isKeyDown(GtsKey::A))
                    transform.position.x -= speed * dt;

                if (gtsinput::isKeyDown(GtsKey::D))
                    transform.position.x += speed * dt;
            }
        }
};
