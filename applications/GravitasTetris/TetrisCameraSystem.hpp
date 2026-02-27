#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "ECSSimulationSystem.hpp"
#include "CameraOverrideComponent.h"
#include "CameraGpuComponent.h"

// Custom camera system for the Tetris scene.
//
// Operates exclusively on entities carrying CameraOverrideComponent —
// CameraGpuSystem will not touch these.  Writes a 2D orthographic
// projection and a flat look-at view directly into CameraGpuComponent.
//
// Must be registered before installRendererFeature() so it runs ahead of
// CameraGpuSystem in the simulation pipeline.
class TetrisCameraSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float) override
        {
            world.forEach<CameraOverrideComponent, CameraGpuComponent>(
                [](Entity, CameraOverrideComponent&, CameraGpuComponent& gpu)
                {
                    constexpr float gridWidth  = 10.0f;
                    constexpr float gridHeight = 20.0f;
                    constexpr float margin     = 1.5f;

                    // Center of the play field
                    const float cx = gridWidth  * 0.5f;
                    const float cy = gridHeight * 0.5f;

                    // Half-extents including margin
                    const float halfW = cx + margin;
                    const float halfH = cy + margin;

                    // Flat 2D view — camera sits directly in front of the field at z=1
                    gpu.viewMatrix = glm::lookAt(
                        glm::vec3(cx, cy, 1.0f),   // eye
                        glm::vec3(cx, cy, 0.0f),   // center
                        glm::vec3(0.0f, 1.0f, 0.0f) // up
                    );

                    // Symmetric orthographic projection centred on the field
                    gpu.projMatrix = glm::ortho(
                        cx - halfW, cx + halfW,   // left,   right
                        cy - halfH, cy + halfH,   // bottom, top
                        0.1f,       100.0f         // near,   far
                    );
                    gpu.projMatrix[1][1] *= -1;   // Vulkan Y-flip

                    gpu.active = true;
                    gpu.dirty  = true;
                }
            );
        }
};
