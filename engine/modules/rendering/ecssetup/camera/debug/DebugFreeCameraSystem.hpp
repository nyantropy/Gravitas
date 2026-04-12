#pragma once

#include <cmath>

#include "GlmConfig.h"

#include "CameraControlOverrideComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"
#include "DebugCameraStateComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "Entity.h"
#include "TransformComponent.h"

class DebugFreeCameraSystem : public ECSControllerSystem
{
public:
    static constexpr float MOVE_SPEED   = 5.0f;
    static constexpr float ROTATE_SPEED = glm::radians(90.0f);
    static constexpr float MIN_PITCH    = glm::radians(-89.0f);
    static constexpr float MAX_PITCH    = glm::radians( 89.0f);

    void update(const EcsControllerContext& ctx) override
    {
        auto& state = ensureState(ctx);

        if (ctx.input->isPressed("engine.debug_camera_toggle"))
        {
            if (!state.active) activate(ctx, state);
            else deactivate(ctx, state);
        }

        if (!state.active) return;

        updateActiveDebugCamera(ctx, state);
    }

private:
    float yaw          = 0.0f;
    float pitch        = 0.0f;

    static Entity invalidEntity()
    {
        return Entity{ std::numeric_limits<entity_id_type>::max() };
    }

    DebugCameraStateComponent& ensureState(const EcsControllerContext& ctx)
    {
        if (!ctx.world.hasAny<DebugCameraStateComponent>())
        {
            auto& state = ctx.world.createSingleton<DebugCameraStateComponent>();
            state.debugCameraEntity = createDebugCameraEntity(ctx);
            return state;
        }

        auto& state = ctx.world.getSingleton<DebugCameraStateComponent>();
        if (!ctx.world.hasComponent<CameraDescriptionComponent>(state.debugCameraEntity))
            state.debugCameraEntity = createDebugCameraEntity(ctx);

        return state;
    }

    Entity createDebugCameraEntity(const EcsControllerContext& ctx)
    {
        Entity e = ctx.world.createEntity();

        CameraDescriptionComponent desc;
        desc.active      = false;
        desc.fov         = glm::radians(70.0f);
        desc.aspectRatio = ctx.windowAspectRatio;
        desc.nearClip    = 0.05f;
        desc.farClip     = 500.0f;
        desc.target      = {0.0f, 0.0f, -1.0f};
        ctx.world.addComponent(e, desc);

        ctx.world.addComponent(e, TransformComponent{});
        ctx.world.addComponent(e, CameraControlOverrideComponent{});
        ctx.world.addComponent(e, CameraOverrideComponent{});
        ctx.world.addComponent(e, CameraGpuComponent{});

        return e;
    }

    void activate(const EcsControllerContext& ctx, DebugCameraStateComponent& state)
    {
        Entity currentCamera = invalidEntity();
        ctx.world.forEach<CameraDescriptionComponent>([&](Entity e, CameraDescriptionComponent& desc)
        {
            if (desc.active && e != state.debugCameraEntity && currentCamera == invalidEntity())
                currentCamera = e;
        });

        if (currentCamera == invalidEntity()) return;

        state.previousActiveCamera = currentCamera;
        state.active = true;

        auto& srcDesc = ctx.world.getComponent<CameraDescriptionComponent>(currentCamera);
        auto& dstDesc = ctx.world.getComponent<CameraDescriptionComponent>(state.debugCameraEntity);
        auto& dstTr   = ctx.world.getComponent<TransformComponent>(state.debugCameraEntity);

        glm::vec3 srcPos = ctx.world.hasComponent<TransformComponent>(currentCamera)
            ? ctx.world.getComponent<TransformComponent>(currentCamera).position
            : glm::vec3(0.0f);
        const glm::vec3 forward = glm::normalize(srcDesc.target - srcPos);

        dstTr.position    = srcPos;
        dstDesc.fov       = srcDesc.fov;
        dstDesc.nearClip  = srcDesc.nearClip;
        dstDesc.farClip   = srcDesc.farClip;
        dstDesc.up        = srcDesc.up;
        dstDesc.active    = true;
        dstDesc.aspectRatio = ctx.windowAspectRatio;
        dstDesc.target    = srcPos + forward;
        srcDesc.active    = false;

        if (ctx.world.hasComponent<CameraGpuComponent>(currentCamera))
        {
            auto& srcGpu = ctx.world.getComponent<CameraGpuComponent>(currentCamera);
            srcGpu.active = false;
            srcGpu.dirty  = true;
        }

        yaw   = std::atan2(-forward.x, -forward.z);
        pitch = glm::clamp(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)), MIN_PITCH, MAX_PITCH);
    }

    void deactivate(const EcsControllerContext& ctx, DebugCameraStateComponent& state)
    {
        state.active = false;

        if (ctx.world.hasComponent<CameraDescriptionComponent>(state.debugCameraEntity))
            ctx.world.getComponent<CameraDescriptionComponent>(state.debugCameraEntity).active = false;

        if (ctx.world.hasComponent<CameraGpuComponent>(state.debugCameraEntity))
        {
            auto& debugGpu = ctx.world.getComponent<CameraGpuComponent>(state.debugCameraEntity);
            debugGpu.active = false;
            debugGpu.dirty  = true;
        }

        if (state.previousActiveCamera != invalidEntity()
            && ctx.world.hasComponent<CameraDescriptionComponent>(state.previousActiveCamera))
        {
            auto& restoredDesc = ctx.world.getComponent<CameraDescriptionComponent>(state.previousActiveCamera);
            restoredDesc.active      = true;
            restoredDesc.aspectRatio = ctx.windowAspectRatio;

            if (ctx.world.hasComponent<CameraGpuComponent>(state.previousActiveCamera))
            {
                auto& restoredGpu = ctx.world.getComponent<CameraGpuComponent>(state.previousActiveCamera);
                restoredGpu.active = true;
                restoredGpu.dirty  = true;

                if (!ctx.world.hasComponent<CameraOverrideComponent>(state.previousActiveCamera))
                {
                    const glm::vec3 restoredPos = ctx.world.hasComponent<TransformComponent>(state.previousActiveCamera)
                        ? ctx.world.getComponent<TransformComponent>(state.previousActiveCamera).position
                        : glm::vec3(0.0f);

                    restoredGpu.viewMatrix = glm::lookAt(restoredPos,
                                                         restoredDesc.target,
                                                         restoredDesc.up);
                    restoredGpu.projMatrix = glm::perspective(restoredDesc.fov,
                                                              restoredDesc.aspectRatio,
                                                              restoredDesc.nearClip,
                                                              restoredDesc.farClip);
                    restoredGpu.projMatrix[1][1] *= -1.0f;
                }
            }
        }

        state.previousActiveCamera = invalidEntity();
    }

    void updateActiveDebugCamera(const EcsControllerContext& ctx, DebugCameraStateComponent& state)
    {
        if (!ctx.world.hasComponent<CameraDescriptionComponent>(state.debugCameraEntity)
            || !ctx.world.hasComponent<TransformComponent>(state.debugCameraEntity))
            return;

        auto& desc = ctx.world.getComponent<CameraDescriptionComponent>(state.debugCameraEntity);
        auto& tr   = ctx.world.getComponent<TransformComponent>(state.debugCameraEntity);
        auto& gpu  = ctx.world.getComponent<CameraGpuComponent>(state.debugCameraEntity);

        const float dt = ctx.time->unscaledDeltaTime;

        if (ctx.input->isHeld("engine.debug_camera_yaw_left")) yaw += ROTATE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_yaw_right")) yaw -= ROTATE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_pitch_up"))   pitch += ROTATE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_pitch_down")) pitch -= ROTATE_SPEED * dt;

        pitch = glm::clamp(pitch, MIN_PITCH, MAX_PITCH);

        const glm::vec3 forward{
            std::cos(pitch) * -std::sin(yaw),
            std::sin(pitch),
            std::cos(pitch) * -std::cos(yaw)
        };
        const glm::vec3 hForward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        const glm::vec3 right = glm::normalize(glm::cross(hForward, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (ctx.input->isHeld("engine.debug_camera_forward")) tr.position += forward * MOVE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_backward")) tr.position -= forward * MOVE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_left")) tr.position -= right   * MOVE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_right")) tr.position += right   * MOVE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_up")) tr.position.y += MOVE_SPEED * dt;
        if (ctx.input->isHeld("engine.debug_camera_down")) tr.position.y -= MOVE_SPEED * dt;

        desc.active      = true;
        desc.aspectRatio = ctx.windowAspectRatio;
        desc.target      = tr.position + forward;

        gpu.viewMatrix = glm::lookAt(tr.position, desc.target, glm::vec3(0.0f, 1.0f, 0.0f));
        gpu.projMatrix = glm::perspective(
            desc.fov,
            desc.aspectRatio > 0.0f ? desc.aspectRatio : ctx.windowAspectRatio,
            desc.nearClip,
            desc.farClip);
        gpu.projMatrix[1][1] *= -1.0f;
        gpu.active = true;
        gpu.dirty  = true;
    }
};
