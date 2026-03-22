#pragma once

#include "GlmConfig.h"
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"
#include "TetrisCameraControlComponent.hpp"

// Telephoto camera GPU system — inspired by the flat, compressed look of
// Tetris Effect Connected, biased right so the scoreboard stays in frame.
//
// This system owns the view / projection matrices only.
// It reads orbit and zoom intent from TetrisCameraControlComponent,
// which is written each frame by TetrisCameraControlSystem.
// Input is never read here.
class TetrisCameraSystem : public ECSControllerSystem
{
    static constexpr float GRID_WIDTH       = 10.0f;
    static constexpr float GRID_HEIGHT      = 20.0f;
    static constexpr float FOV              = glm::radians(7.0f);

    // Camera is centered on the board with no lateral bias so both the left
    // hold panel and the right preview/stats panel have equal visible room.
    static constexpr float FRAMING_OFFSET_X = 0.0f;

    public:
        void update(ECSWorld& world, SceneContext&) override
        {
            world.forEach<CameraDescriptionComponent,
                          CameraOverrideComponent,
                          TetrisCameraControlComponent>(
                [&](Entity e, CameraDescriptionComponent& desc,
                    CameraOverrideComponent&, TetrisCameraControlComponent& control)
                {
                    if (!world.hasComponent<CameraGpuComponent>(e))
                        world.addComponent(e, CameraGpuComponent{});

                    auto& gpu = world.getComponent<CameraGpuComponent>(e);

                    const glm::vec3 boardCenter =
                        glm::vec3(GRID_WIDTH * 0.5f, GRID_HEIGHT * 0.5f, 0.0f);

                    // Rotate base forward (0,0,-1) around world-up Y by orbitAngle.
                    // R_y(θ) * (0,0,-1) = (-sinθ, 0, -cosθ).
                    // At θ=0 this gives (0,0,-1), matching the original hard-coded orientation.
                    const float sinA        = std::sin(control.orbitAngle);
                    const float cosA        = std::cos(control.orbitAngle);
                    const glm::vec3 forward =
                        glm::normalize(glm::vec3(-sinA, 0.0f, -cosA));
                    const glm::vec3 up      = glm::vec3(0.0f, 1.0f, 0.0f);
                    const glm::vec3 right   = glm::normalize(glm::cross(forward, up));

                    // Shift the framing target instead of just moving the camera
                    const glm::vec3 framedCenter =
                        boardCenter + right * FRAMING_OFFSET_X;

                    const glm::vec3 position =
                        framedCenter - forward * control.zoomDistance;

                    gpu.viewMatrix =
                        glm::lookAt(position, framedCenter, up);

                    gpu.projMatrix =
                        glm::perspective(
                            FOV,
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