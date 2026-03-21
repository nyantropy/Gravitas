#pragma once

#include <cmath>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsKey.h"

#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "CameraOverrideComponent.h"
#include "CameraGpuComponent.h"
#include "TransformComponent.h"

// Free-fly camera controller for GtsScene2.
// Entities must have CameraControlOverrideComponent so DefaultCameraControlSystem skips them.
// Also requires CameraOverrideComponent so CameraGpuSystem skips them; this system writes
// matrices directly to CameraGpuComponent each frame.
//
// Controls:
//   W / S     — move forward / backward along look direction
//   A / D     — strafe left / right
//   Q / E     — yaw left / right
//   R / F     — move up / down (world Y)
class FreeFlyCamera : public ECSControllerSystem
{
public:
    static constexpr float MOVE_SPEED   = 5.0f;               // units per second
    static constexpr float ROTATE_SPEED = glm::radians(90.0f); // radians per second

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt  = ctx.time->unscaledDeltaTime;
        auto* input     = ctx.inputSource;

        for (Entity e : world.getAllEntitiesWith<CameraDescriptionComponent,
                                                 CameraControlOverrideComponent,
                                                 CameraOverrideComponent,
                                                 TransformComponent>())
        {
            auto& tr   = world.getComponent<TransformComponent>(e);
            auto& desc = world.getComponent<CameraDescriptionComponent>(e);

            // Yaw rotation
            if (input->isKeyDown(GtsKey::Q)) yaw += ROTATE_SPEED * dt;
            if (input->isKeyDown(GtsKey::E)) yaw -= ROTATE_SPEED * dt;

            // Look direction from yaw (XZ plane only — no pitch)
            glm::vec3 forward{
                -std::sin(yaw),
                 0.0f,
                -std::cos(yaw)
            };
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

            // Translate
            if (input->isKeyDown(GtsKey::W)) tr.position += forward * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::S)) tr.position -= forward * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::A)) tr.position -= right   * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::D)) tr.position += right   * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::R)) tr.position.y += MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::T)) tr.position.y -= MOVE_SPEED * dt;

            // Write view/proj directly into CameraGpuComponent
            if (!world.hasComponent<CameraGpuComponent>(e))
                world.addComponent(e, CameraGpuComponent{});

            auto& gpu = world.getComponent<CameraGpuComponent>(e);

            glm::vec3 lookAt = tr.position + forward;
            gpu.viewMatrix = glm::lookAt(tr.position, lookAt, glm::vec3(0.0f, 1.0f, 0.0f));
            gpu.projMatrix = glm::perspective(
                desc.fov,
                desc.aspectRatio > 0.0f ? desc.aspectRatio : ctx.windowAspectRatio,
                desc.nearClip,
                desc.farClip);
            // Flip Y for Vulkan NDC
            gpu.projMatrix[1][1] *= -1.0f;

            gpu.active = desc.active;
            gpu.dirty  = true;
        }
    }

private:
    float yaw = 0.0f;  // radians; 0 = looking along -Z
};
