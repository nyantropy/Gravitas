#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "ECSSimulationSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"

// a telephoto camera system, aimed to replicate the same effect as in the game Tetris Effect Connect
class TetrisCameraSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float) override
        {
            world.forEach<CameraDescriptionComponent, CameraOverrideComponent>(
                [&](Entity e, CameraDescriptionComponent& desc, CameraOverrideComponent&)
                {
                    if (!world.hasComponent<CameraGpuComponent>(e))
                        world.addComponent(e, CameraGpuComponent{});

                    auto& gpu = world.getComponent<CameraGpuComponent>(e);

                    constexpr float gridWidth  = 10.0f;
                    constexpr float gridHeight = 20.0f;

                    // --- Telephoto parameters (key to Tetris Effect look)
                    const float distance = 180.0f;                 // much farther
                    const float fov      = glm::radians(7.0f);     // very narrow FOV

                    const glm::vec3 boardCenter =
                        glm::vec3(gridWidth * 0.5f, gridHeight * 0.5f, 0.0f);

                    const glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
                    const glm::vec3 up      = glm::vec3(0.0f, 1.0f,  0.0f);

                    const glm::vec3 position =
                        boardCenter - forward * distance;

                    gpu.viewMatrix =
                        glm::lookAt(position, boardCenter, up);

                    gpu.projMatrix =
                        glm::perspective(
                            fov,
                            desc.aspectRatio,
                            desc.nearClip,
                            desc.farClip
                        );

                    gpu.projMatrix[1][1] *= -1.0f;

                    gpu.active = desc.active;
                    gpu.dirty  = true;
                }
            );
        }
};