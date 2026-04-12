#pragma once

#include <cmath>
#include "GlmConfig.h"

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "GtsKey.h"

#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "TransformComponent.h"

// Free-fly camera controller for GtsScene2.
// Entities must have CameraControlOverrideComponent so DefaultCameraControlSystem skips them.
// This system updates only TransformComponent and CameraDescriptionComponent.
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

    void update(const EcsControllerContext& ctx) override
    {
        const float dt  = ctx.time->unscaledDeltaTime;

        for (Entity e : ctx.world.getAllEntitiesWith<CameraDescriptionComponent,
                                                     CameraControlOverrideComponent,
                                                     TransformComponent>())
        {
            auto& tr   = ctx.world.getComponent<TransformComponent>(e);
            auto& desc = ctx.world.getComponent<CameraDescriptionComponent>(e);

            // Yaw rotation
            if (ctx.input->isHeld("gts_scene2.camera_yaw_left")) yaw += ROTATE_SPEED * dt;
            if (ctx.input->isHeld("gts_scene2.camera_yaw_right")) yaw -= ROTATE_SPEED * dt;

            // Look direction from yaw (XZ plane only — no pitch)
            glm::vec3 forward{
                -std::sin(yaw),
                 0.0f,
                -std::cos(yaw)
            };
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

            // Translate
            if (ctx.input->isHeld("gts_scene2.camera_forward")) tr.position += forward * MOVE_SPEED * dt;
            if (ctx.input->isHeld("gts_scene2.camera_backward")) tr.position -= forward * MOVE_SPEED * dt;
            if (ctx.input->isHeld("gts_scene2.camera_left")) tr.position -= right   * MOVE_SPEED * dt;
            if (ctx.input->isHeld("gts_scene2.camera_right")) tr.position += right   * MOVE_SPEED * dt;
            if (ctx.input->isHeld("gts_scene2.camera_up")) tr.position.y += MOVE_SPEED * dt;
            if (ctx.input->isHeld("gts_scene2.camera_down")) tr.position.y -= MOVE_SPEED * dt;

            desc.target      = tr.position + forward;
            desc.up          = glm::vec3(0.0f, 1.0f, 0.0f);
            desc.aspectRatio = ctx.windowAspectRatio;
        }
    }

private:
    float yaw = 0.0f;  // radians; 0 = looking along -Z
};
