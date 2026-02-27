#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "ECSSimulationSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"

// A telephoto camera system, aimed to replicate the same effect as in
// Tetris Effect Connected, but biased to the right so the scoreboard
// stays inside the frame.
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

                    // --- Telephoto parameters (flat / compressed look)
                    const float distance = 180.0f;
                    const float fov      = glm::radians(7.0f);

                    // How much to shift the framing to the right (world units)
                    const float framingOffsetX = 4.0f;

                    const glm::vec3 boardCenter =
                        glm::vec3(gridWidth * 0.5f, gridHeight * 0.5f, 0.0f);

                    const glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
                    const glm::vec3 up      = glm::vec3(0.0f, 1.0f,  0.0f);
                    const glm::vec3 right   = glm::normalize(glm::cross(forward, up));

                    // Shift the framing target instead of just moving the camera
                    const glm::vec3 framedCenter =
                        boardCenter + right * framingOffsetX;

                    const glm::vec3 position =
                        framedCenter - forward * distance;

                    gpu.viewMatrix =
                        glm::lookAt(position, framedCenter, up);

                    gpu.projMatrix =
                        glm::perspective(
                            fov,
                            desc.aspectRatio,
                            desc.nearClip,
                            desc.farClip
                        );

                    // Vulkan Y-flip
                    gpu.projMatrix[1][1] *= -1.0f;

                    gpu.active = desc.active;
                    gpu.dirty  = true;
                }
            );
        }
};