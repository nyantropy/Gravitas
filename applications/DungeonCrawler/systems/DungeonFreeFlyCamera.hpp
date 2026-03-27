#pragma once

#include <cmath>
#include "GlmConfig.h"

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsKey.h"

#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "CameraOverrideComponent.h"
#include "CameraGpuComponent.h"
#include "TransformComponent.h"

// Free-fly debug camera for the dungeon crawler.
// Adapted from GtsScene2/FreeFlyCamera; only processes input when desc.active
// is true so keys don't interfere with the player movement system.
//
// Controls (active only when the debug camera is on):
//   W / S     — move forward / backward along look direction
//   A / D     — strafe left / right
//   Q / E     — yaw left / right
//   R / G     — move up / down (world Y)
class DungeonFreeFlyCamera : public ECSControllerSystem
{
public:
    static constexpr float MOVE_SPEED   = 5.0f;
    static constexpr float ROTATE_SPEED = glm::radians(90.0f);

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt = ctx.time->unscaledDeltaTime;
        auto* input    = ctx.inputSource;

        for (Entity e : world.getAllEntitiesWith<CameraDescriptionComponent,
                                                 CameraControlOverrideComponent,
                                                 CameraOverrideComponent,
                                                 TransformComponent>())
        {
            auto& desc = world.getComponent<CameraDescriptionComponent>(e);

            // Only drive this camera when it is the active one.
            if (!desc.active)
                continue;

            auto& tr = world.getComponent<TransformComponent>(e);

            if (input->isKeyDown(GtsKey::Q)) yaw += ROTATE_SPEED * dt;
            if (input->isKeyDown(GtsKey::E)) yaw -= ROTATE_SPEED * dt;

            glm::vec3 forward{
                -std::sin(yaw),
                 0.0f,
                -std::cos(yaw)
            };
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (input->isKeyDown(GtsKey::W)) tr.position += forward * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::S)) tr.position -= forward * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::A)) tr.position -= right   * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::D)) tr.position += right   * MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::R)) tr.position.y += MOVE_SPEED * dt;
            if (input->isKeyDown(GtsKey::G)) tr.position.y -= MOVE_SPEED * dt;

            if (!world.hasComponent<CameraGpuComponent>(e))
                world.addComponent(e, CameraGpuComponent{});

            auto& gpu = world.getComponent<CameraGpuComponent>(e);

            glm::vec3 lookAt = tr.position + forward;
            gpu.viewMatrix   = glm::lookAt(tr.position, lookAt, glm::vec3(0.0f, 1.0f, 0.0f));
            gpu.projMatrix   = glm::perspective(
                desc.fov,
                desc.aspectRatio > 0.0f ? desc.aspectRatio : ctx.windowAspectRatio,
                desc.nearClip,
                desc.farClip);
            gpu.projMatrix[1][1] *= -1.0f;

            gpu.active = true;
            gpu.dirty  = true;
        }
    }

private:
    float yaw = 0.0f; // radians; 0 = looking along -Z
};
