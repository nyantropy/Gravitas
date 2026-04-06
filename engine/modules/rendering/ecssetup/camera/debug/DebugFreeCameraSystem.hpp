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
#include "GtsAction.h"
#include "GtsKey.h"
#include "SceneContext.h"
#include "TransformComponent.h"

class DebugFreeCameraSystem : public ECSControllerSystem
{
public:
    static constexpr float MOVE_SPEED   = 5.0f;
    static constexpr float ROTATE_SPEED = glm::radians(90.0f);
    static constexpr float MIN_PITCH    = glm::radians(-89.0f);
    static constexpr float MAX_PITCH    = glm::radians( 89.0f);

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        bindDefaults(ctx);

        auto& state = ensureState(world, ctx);

        if (ctx.actions->isActionPressed(GtsAction::ToggleDebugCamera))
        {
            if (!state.active) activate(world, ctx, state);
            else deactivate(world, ctx, state);
        }

        if (!state.active) return;

        updateActiveDebugCamera(world, ctx, state);
    }

private:
    bool defaultsBound = false;
    float yaw          = 0.0f;
    float pitch        = 0.0f;

    static Entity invalidEntity()
    {
        return Entity{ std::numeric_limits<entity_id_type>::max() };
    }

    void bindDefaults(SceneContext& ctx)
    {
        if (defaultsBound) return;

        ctx.actions->bind(GtsAction::ToggleDebugCamera, GtsKey::F4);
        defaultsBound = true;
    }

    DebugCameraStateComponent& ensureState(ECSWorld& world, SceneContext& ctx)
    {
        if (!world.hasAny<DebugCameraStateComponent>())
        {
            auto& state = world.createSingleton<DebugCameraStateComponent>();
            state.debugCameraEntity = createDebugCameraEntity(world, ctx);
            return state;
        }

        auto& state = world.getSingleton<DebugCameraStateComponent>();
        if (!world.hasComponent<CameraDescriptionComponent>(state.debugCameraEntity))
            state.debugCameraEntity = createDebugCameraEntity(world, ctx);

        return state;
    }

    Entity createDebugCameraEntity(ECSWorld& world, SceneContext& ctx)
    {
        Entity e = world.createEntity();

        CameraDescriptionComponent desc;
        desc.active      = false;
        desc.fov         = glm::radians(70.0f);
        desc.aspectRatio = ctx.windowAspectRatio;
        desc.nearClip    = 0.05f;
        desc.farClip     = 500.0f;
        desc.target      = {0.0f, 0.0f, -1.0f};
        world.addComponent(e, desc);

        world.addComponent(e, TransformComponent{});
        world.addComponent(e, CameraControlOverrideComponent{});
        world.addComponent(e, CameraOverrideComponent{});
        world.addComponent(e, CameraGpuComponent{});

        return e;
    }

    void activate(ECSWorld& world, SceneContext& ctx, DebugCameraStateComponent& state)
    {
        Entity currentCamera = invalidEntity();
        world.forEach<CameraDescriptionComponent>([&](Entity e, CameraDescriptionComponent& desc)
        {
            if (desc.active && e != state.debugCameraEntity && currentCamera == invalidEntity())
                currentCamera = e;
        });

        if (currentCamera == invalidEntity()) return;

        state.previousActiveCamera = currentCamera;
        state.active = true;

        auto& srcDesc = world.getComponent<CameraDescriptionComponent>(currentCamera);
        auto& dstDesc = world.getComponent<CameraDescriptionComponent>(state.debugCameraEntity);
        auto& dstTr   = world.getComponent<TransformComponent>(state.debugCameraEntity);

        glm::vec3 srcPos = world.hasComponent<TransformComponent>(currentCamera)
            ? world.getComponent<TransformComponent>(currentCamera).position
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

        if (world.hasComponent<CameraGpuComponent>(currentCamera))
        {
            auto& srcGpu = world.getComponent<CameraGpuComponent>(currentCamera);
            srcGpu.active = false;
            srcGpu.dirty  = true;
        }

        yaw   = std::atan2(-forward.x, -forward.z);
        pitch = glm::clamp(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)), MIN_PITCH, MAX_PITCH);
    }

    void deactivate(ECSWorld& world, SceneContext& ctx, DebugCameraStateComponent& state)
    {
        state.active = false;

        if (world.hasComponent<CameraDescriptionComponent>(state.debugCameraEntity))
            world.getComponent<CameraDescriptionComponent>(state.debugCameraEntity).active = false;

        if (world.hasComponent<CameraGpuComponent>(state.debugCameraEntity))
        {
            auto& debugGpu = world.getComponent<CameraGpuComponent>(state.debugCameraEntity);
            debugGpu.active = false;
            debugGpu.dirty  = true;
        }

        if (state.previousActiveCamera != invalidEntity()
            && world.hasComponent<CameraDescriptionComponent>(state.previousActiveCamera))
        {
            auto& restoredDesc = world.getComponent<CameraDescriptionComponent>(state.previousActiveCamera);
            restoredDesc.active      = true;
            restoredDesc.aspectRatio = ctx.windowAspectRatio;

            if (world.hasComponent<CameraGpuComponent>(state.previousActiveCamera))
            {
                auto& restoredGpu = world.getComponent<CameraGpuComponent>(state.previousActiveCamera);
                restoredGpu.active = true;
                restoredGpu.dirty  = true;

                if (!world.hasComponent<CameraOverrideComponent>(state.previousActiveCamera))
                {
                    const glm::vec3 restoredPos = world.hasComponent<TransformComponent>(state.previousActiveCamera)
                        ? world.getComponent<TransformComponent>(state.previousActiveCamera).position
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

    void updateActiveDebugCamera(ECSWorld& world, SceneContext& ctx, DebugCameraStateComponent& state)
    {
        if (!world.hasComponent<CameraDescriptionComponent>(state.debugCameraEntity)
            || !world.hasComponent<TransformComponent>(state.debugCameraEntity))
            return;

        auto& desc = world.getComponent<CameraDescriptionComponent>(state.debugCameraEntity);
        auto& tr   = world.getComponent<TransformComponent>(state.debugCameraEntity);
        auto& gpu  = world.getComponent<CameraGpuComponent>(state.debugCameraEntity);
        auto* input = ctx.inputSource;

        const float dt = ctx.time->unscaledDeltaTime;

        if (input->isKeyDown(GtsKey::Q)) yaw += ROTATE_SPEED * dt;
        if (input->isKeyDown(GtsKey::E)) yaw -= ROTATE_SPEED * dt;
        if (input->isKeyDown(GtsKey::ArrowUp))   pitch += ROTATE_SPEED * dt;
        if (input->isKeyDown(GtsKey::ArrowDown)) pitch -= ROTATE_SPEED * dt;

        pitch = glm::clamp(pitch, MIN_PITCH, MAX_PITCH);

        const glm::vec3 forward{
            std::cos(pitch) * -std::sin(yaw),
            std::sin(pitch),
            std::cos(pitch) * -std::cos(yaw)
        };
        const glm::vec3 hForward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
        const glm::vec3 right = glm::normalize(glm::cross(hForward, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (input->isKeyDown(GtsKey::W)) tr.position += forward * MOVE_SPEED * dt;
        if (input->isKeyDown(GtsKey::S)) tr.position -= forward * MOVE_SPEED * dt;
        if (input->isKeyDown(GtsKey::A)) tr.position -= right   * MOVE_SPEED * dt;
        if (input->isKeyDown(GtsKey::D)) tr.position += right   * MOVE_SPEED * dt;
        if (input->isKeyDown(GtsKey::R)) tr.position.y += MOVE_SPEED * dt;
        if (input->isKeyDown(GtsKey::F)) tr.position.y -= MOVE_SPEED * dt;

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
