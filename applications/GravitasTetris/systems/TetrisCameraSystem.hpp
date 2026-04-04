#pragma once

#include "GlmConfig.h"
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "TransformComponent.h"
#include "TetrisCameraControlComponent.hpp"

// Telephoto camera system — inspired by the flat, compressed look of
// Tetris Effect Connected, biased right so the scoreboard stays in frame.
//
// This system owns gameplay-facing camera intent only. It writes transform and
// CameraDescriptionComponent data, then CameraGpuSystem computes the backend
// matrices from that description.
class TetrisCameraSystem : public ECSControllerSystem
{
    static constexpr float GRID_WIDTH       = 10.0f;
    static constexpr float GRID_HEIGHT      = 20.0f;
    static constexpr float FOV              = glm::radians(7.0f);

    // Camera is centered on the board with no lateral bias so both the left
    // hold panel and the right preview/stats panel have equal visible room.
    static constexpr float FRAMING_OFFSET_X = 0.0f;

    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            world.forEach<CameraDescriptionComponent,
                          CameraControlOverrideComponent,
                          TransformComponent,
                          TetrisCameraControlComponent>(
                [&](Entity,
                    CameraDescriptionComponent& desc,
                    CameraControlOverrideComponent&,
                    TransformComponent& transform,
                    TetrisCameraControlComponent& control)
                {
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

                    transform.position = position;
                    desc.target        = framedCenter;
                    desc.up            = up;
                    desc.fov           = FOV;
                    desc.aspectRatio   = ctx.windowAspectRatio;
                }
            );
        }
};
