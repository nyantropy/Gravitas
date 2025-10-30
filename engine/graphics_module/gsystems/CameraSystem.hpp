#pragma once
#include "ECSWorld.hpp"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "ECSSystem.hpp"

class CameraSystem : public ECSSystem
{
    public:
        void update(ECSWorld& world, float dt) override
        {
            for (Entity e : world.getAllEntitiesWith<CameraComponent, TransformComponent>())
            {
                auto& cam = world.getComponent<CameraComponent>(e);
                auto& transform = world.getComponent<TransformComponent>(e);

                cam.view = glm::lookAt(
                    transform.position,
                    cam.target,
                    cam.up
                );

                cam.projection = glm::perspective(cam.fov, cam.aspectRatio, cam.nearClip, cam.farClip);
                cam.projection[1][1] *= -1;
            }
        }
};