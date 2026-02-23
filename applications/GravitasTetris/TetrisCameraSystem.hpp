#pragma once
#include "ECSSimulationSystem.hpp"
#include "ECSWorld.hpp"
#include "CameraComponent.h"
#include "CameraGpuComponent.h"
#include "TransformComponent.h"
#include <gtc/matrix_transform.hpp>

// an attempt to get a flat 2d view with glm::ortho was made, but it didnt work, so this needs further work
class TetrisCameraSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float dt) override
        {
            for (Entity e : world.getAllEntitiesWith<CameraComponent, CameraGpuComponent, TransformComponent>())
            {
                auto& cam = world.getComponent<CameraComponent>(e);
                auto& gpu = world.getComponent<CameraGpuComponent>(e);
                auto& transform = world.getComponent<TransformComponent>(e);

                float gridWidth  = 10.0f;
                float gridHeight = 20.0f;
                float zDistance  = 30.0f;

                transform.position = glm::vec3(gridWidth / 2.0f, gridHeight / 2.0f, zDistance);
                cam.target          = glm::vec3(gridWidth / 2.0f, gridHeight / 2.0f, 0.0f);
                cam.up              = glm::vec3(0.0f, 1.0f, 0.0f);

                cam.view = glm::lookAt(
                    transform.position,
                    cam.target,
                    cam.up
                );

                cam.fov = glm::radians(45.0f);
                cam.aspectRatio = 800.0f / 800.0f;
                cam.nearClip = 0.1f;
                cam.farClip  = 1000.0f;

                cam.projection = glm::perspective(cam.fov, cam.aspectRatio, cam.nearClip, cam.farClip);
                cam.projection[1][1] *= -1;

                // mark dirty so CameraGpuDataSystem picks up the new matrices
                gpu.dirty = true;
            }
        }
};
