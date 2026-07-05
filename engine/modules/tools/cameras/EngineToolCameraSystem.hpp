#pragma once

#include <cmath>
#include <limits>

#include "GlmConfig.h"

#include "ActiveCameraViewStateComponent.h"
#include "CameraControlOverrideComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolCameraStateComponent.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolPreviewCameraComponent.h"
#include "EngineToolStateComponent.h"
#include "RenderViewportComponent.h"
#include "ToolEntityLabelComponent.h"
#include "TransformComponent.h"

namespace gts::tools
{
    class EngineToolCameraSystem : public ECSControllerSystem
    {
        public:
        static constexpr float MOVE_SPEED       = 11.25f;
        static constexpr float ROTATE_SPEED     = glm::radians(90.0f);
        static constexpr float MOUSE_LOOK_SPEED = 0.0025f;
        static constexpr float MIN_PITCH        = glm::radians(-89.0f);
        static constexpr float MAX_PITCH        = glm::radians(89.0f);

        static Entity invalidEntity()
        {
            return Entity{std::numeric_limits<entity_id_type>::max()};
        }

        static Entity createToolCameraEntity(ECSWorld& world, float aspectRatio)
        {
            Entity entity = world.createEntity();

            CameraDescriptionComponent camera;
            camera.active      = false;
            camera.fov         = glm::radians(70.0f);
            camera.aspectRatio = aspectRatio;
            camera.nearClip    = 0.05f;
            camera.farClip     = 500.0f;
            camera.target      = {0.0f, 0.0f, -1.0f};

            ToolEntityLabelComponent label;
            label.name       = "Tool Camera";
            label.category   = "Tools";
            label.selectable = false;

            world.addComponent(entity, TransformComponent{});
            world.addComponent(entity, camera);
            world.addComponent(entity, CameraControlOverrideComponent{});
            world.addComponent(entity, CameraOverrideComponent{});
            world.addComponent(entity, label);
            return entity;
        }

        static void ensureToolCameraState(ECSWorld& world, float aspectRatio)
        {
            if (world.hasAny<EngineToolCameraStateComponent>())
            {
                EngineToolCameraStateComponent& state = world.getSingleton<EngineToolCameraStateComponent>();
                if (isValidCameraEntity(world, state.toolCameraEntity))
                    return;

                state.toolCameraEntity     = createToolCameraEntity(world, aspectRatio);
                state.active               = false;
                state.previousActiveCamera = invalidEntity();
                return;
            }

            EngineToolCameraStateComponent& state = world.createSingleton<EngineToolCameraStateComponent>();
            state.toolCameraEntity                = createToolCameraEntity(world, aspectRatio);
        }

        void update(const EcsControllerContext& ctx) override
        {
            const float activeAspect = resolveAspect(ctx);
            const bool  toolsVisible = ctx.world.hasAny<EngineToolStateComponent>() &&
                                       ctx.world.getSingleton<EngineToolStateComponent>().visible;

            if (!toolsVisible)
            {
                if (ctx.world.hasAny<EngineToolCameraStateComponent>())
                {
                    EngineToolCameraStateComponent& cameraState =
                        ctx.world.getSingleton<EngineToolCameraStateComponent>();
                    if (cameraState.active)
                        deactivate(ctx, cameraState, activeAspect);
                }
                return;
            }

            ensureToolCameraState(ctx.world, activeAspect);
            EngineToolCameraStateComponent& cameraState = ctx.world.getSingleton<EngineToolCameraStateComponent>();

            if (!cameraState.active)
                activate(ctx, cameraState, activeAspect);

            updateActiveToolCamera(ctx, cameraState, activeAspect);
        }

        private:
        float  yaw            = 0.0f;
        float  pitch          = 0.0f;
        double previousMouseX = 0.0;
        double previousMouseY = 0.0;
        bool   mouseLookReady = false;

        static bool isValidCameraEntity(ECSWorld& world, Entity entity)
        {
            return entity.id != std::numeric_limits<entity_id_type>::max() &&
                   world.hasComponent<TransformComponent>(entity) &&
                   world.hasComponent<CameraDescriptionComponent>(entity);
        }

        static glm::vec3 safeForward(const glm::vec3& value)
        {
            if (glm::dot(value, value) <= 0.000001f)
                return {0.0f, 0.0f, -1.0f};
            return glm::normalize(value);
        }

        static Entity findActiveCamera(ECSWorld& world, Entity excluded)
        {
            Entity active = invalidEntity();
            world.forEach<CameraDescriptionComponent>(
                [&](Entity entity, CameraDescriptionComponent& camera)
                {
                    if (active.id != invalidEntity().id || entity.id == excluded.id || !camera.active)
                    {
                        return;
                    }

                    active = entity;
                });
            return active;
        }

        static float resolveAspect(const EcsControllerContext& ctx)
        {
            if (ctx.world.hasAny<RenderViewportComponent>())
            {
                const RenderViewportComponent& viewport = ctx.world.getSingleton<RenderViewportComponent>();
                if (viewport.sceneViewport.valid())
                    return viewport.sceneViewport.aspect();
            }

            return ctx.windowAspectRatio > 0.0f ? ctx.windowAspectRatio : 1.0f;
        }

        static void setCameraActive(ECSWorld& world, Entity entity, bool active, float aspectRatio)
        {
            if (entity.id == invalidEntity().id || !world.hasComponent<CameraDescriptionComponent>(entity))
            {
                return;
            }

            CameraDescriptionComponent& camera = world.getComponent<CameraDescriptionComponent>(entity);
            camera.active                      = active;
            camera.aspectRatio                 = aspectRatio;

            if (world.hasComponent<CameraGpuComponent>(entity))
            {
                CameraGpuComponent& gpu = world.getComponent<CameraGpuComponent>(entity);
                gpu.active              = active;
                gpu.dirty               = true;

                if (active && !world.hasComponent<CameraOverrideComponent>(entity))
                    writeDefaultGpuMatrices(world, entity, camera, gpu);
            }
        }

        static void writeDefaultGpuMatrices(ECSWorld&                         world,
                                            Entity                            entity,
                                            const CameraDescriptionComponent& camera,
                                            CameraGpuComponent&               gpu)
        {
            const glm::vec3 position = world.hasComponent<TransformComponent>(entity)
                                           ? world.getComponent<TransformComponent>(entity).position
                                           : glm::vec3(0.0f);
            gpu.viewMatrix           = glm::lookAt(position, camera.target, camera.up);
            gpu.projMatrix = glm::perspective(camera.fov, camera.aspectRatio, camera.nearClip, camera.farClip);
            gpu.projMatrix[1][1] *= -1.0f;
        }

        static void syncActiveCameraView(ECSWorld& world, Entity entity, const glm::mat4& view, const glm::mat4& proj)
        {
            ActiveCameraViewStateComponent* state = nullptr;
            if (world.hasAny<ActiveCameraViewStateComponent>())
                state = &world.getSingleton<ActiveCameraViewStateComponent>();
            else
                state = &world.createSingleton<ActiveCameraViewStateComponent>();

            state->activeCamera   = entity;
            state->viewMatrix     = view;
            state->projMatrix     = proj;
            state->viewProjMatrix = proj * view;
            state->valid          = entity.id != invalidEntity().id;
        }

        static void syncActiveCameraViewFromEntity(ECSWorld& world, Entity entity)
        {
            if (entity.id == invalidEntity().id || !world.hasComponent<CameraDescriptionComponent>(entity))
            {
                syncActiveCameraView(world, invalidEntity(), glm::mat4(1.0f), glm::mat4(1.0f));
                return;
            }

            if (world.hasComponent<CameraGpuComponent>(entity))
            {
                const CameraGpuComponent& gpu = world.getComponent<CameraGpuComponent>(entity);
                syncActiveCameraView(world, entity, gpu.viewMatrix, gpu.projMatrix);
                return;
            }

            const CameraDescriptionComponent& camera   = world.getComponent<CameraDescriptionComponent>(entity);
            const glm::vec3                   position = world.hasComponent<TransformComponent>(entity)
                                                             ? world.getComponent<TransformComponent>(entity).position
                                                             : glm::vec3(0.0f);
            glm::mat4                         view     = glm::lookAt(position, camera.target, camera.up);
            glm::mat4 proj = glm::perspective(camera.fov, camera.aspectRatio, camera.nearClip, camera.farClip);
            proj[1][1] *= -1.0f;
            syncActiveCameraView(world, entity, view, proj);
        }

        void activate(const EcsControllerContext& ctx, EngineToolCameraStateComponent& state, float aspectRatio)
        {
            ensureToolCameraState(ctx.world, aspectRatio);

            const Entity toolCamera    = state.toolCameraEntity;
            const Entity currentCamera = findActiveCamera(ctx.world, toolCamera);
            state.previousActiveCamera = currentCamera;
            state.active               = true;
            mouseLookReady             = false;

            TransformComponent&         toolTransform = ctx.world.getComponent<TransformComponent>(toolCamera);
            CameraDescriptionComponent& toolDesc      = ctx.world.getComponent<CameraDescriptionComponent>(toolCamera);

            if (currentCamera.id != invalidEntity().id)
            {
                const CameraDescriptionComponent& sourceDesc =
                    ctx.world.getComponent<CameraDescriptionComponent>(currentCamera);
                const glm::vec3 sourcePosition =
                    ctx.world.hasComponent<TransformComponent>(currentCamera)
                        ? ctx.world.getComponent<TransformComponent>(currentCamera).position
                        : glm::vec3(0.0f);
                const glm::vec3 forward = safeForward(sourceDesc.target - sourcePosition);

                toolTransform.position = sourcePosition;
                toolDesc.fov           = sourceDesc.fov;
                toolDesc.nearClip      = sourceDesc.nearClip;
                toolDesc.farClip       = sourceDesc.farClip;
                toolDesc.up            = sourceDesc.up;
                toolDesc.target        = sourcePosition + forward;
                setCameraActive(ctx.world, currentCamera, false, aspectRatio);

                yaw   = std::atan2(-forward.x, -forward.z);
                pitch = glm::clamp(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)), MIN_PITCH, MAX_PITCH);
            }

            toolDesc.active      = true;
            toolDesc.aspectRatio = aspectRatio;
        }

        void deactivate(const EcsControllerContext& ctx, EngineToolCameraStateComponent& state, float aspectRatio)
        {
            if (state.toolCameraEntity.id != invalidEntity().id &&
                ctx.world.hasComponent<CameraDescriptionComponent>(state.toolCameraEntity))
            {
                setCameraActive(ctx.world, state.toolCameraEntity, false, aspectRatio);
            }

            setCameraActive(ctx.world, state.previousActiveCamera, true, aspectRatio);
            syncActiveCameraViewFromEntity(ctx.world, state.previousActiveCamera);
            state.active               = false;
            state.previousActiveCamera = invalidEntity();
            mouseLookReady             = false;
        }

        void updateActiveToolCamera(const EcsControllerContext&     ctx,
                                    EngineToolCameraStateComponent& state,
                                    float                           aspectRatio)
        {
            if (!isValidCameraEntity(ctx.world, state.toolCameraEntity))
                return;

            TransformComponent&         transform = ctx.world.getComponent<TransformComponent>(state.toolCameraEntity);
            CameraDescriptionComponent& camera =
                ctx.world.getComponent<CameraDescriptionComponent>(state.toolCameraEntity);

            if (ctx.world.hasAny<EngineToolPreviewCameraComponent>())
            {
                const EngineToolPreviewCameraComponent& preview =
                    ctx.world.getSingleton<EngineToolPreviewCameraComponent>();
                if (preview.active)
                {
                    applyPreviewCamera(ctx.world, state.toolCameraEntity, transform, camera, preview, aspectRatio);
                    const glm::vec3 forward = safeForward(preview.target - preview.position);
                    yaw   = std::atan2(-forward.x, -forward.z);
                    pitch = glm::clamp(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)), MIN_PITCH, MAX_PITCH);
                    return;
                }
            }

            const float dt = ctx.time == nullptr ? 0.0f : ctx.time->unscaledDeltaTime;
            const bool keyboardCaptured = ctx.world.hasAny<EngineToolInputCaptureComponent>() &&
                                          ctx.world.getSingleton<EngineToolInputCaptureComponent>().keyboardCaptured;
            if (ctx.input != nullptr && !keyboardCaptured)
            {
                if (ctx.input->isHeld("engine.tool_camera_yaw_left"))
                    yaw += ROTATE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_yaw_right"))
                    yaw -= ROTATE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_pitch_up"))
                    pitch += ROTATE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_pitch_down"))
                    pitch -= ROTATE_SPEED * dt;
                updateMouseLook(ctx);
            }
            else
            {
                mouseLookReady = false;
            }

            pitch = glm::clamp(pitch, MIN_PITCH, MAX_PITCH);
            const glm::vec3 forward{
                std::cos(pitch) * -std::sin(yaw), std::sin(pitch), std::cos(pitch) * -std::cos(yaw)};
            const glm::vec3 hForward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
            const glm::vec3 right = glm::normalize(glm::cross(hForward, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (ctx.input != nullptr && !keyboardCaptured)
            {
                if (ctx.input->isHeld("engine.tool_camera_forward"))
                    transform.position += forward * MOVE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_backward"))
                    transform.position -= forward * MOVE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_left"))
                    transform.position -= right * MOVE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_right"))
                    transform.position += right * MOVE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_up"))
                    transform.position.y += MOVE_SPEED * dt;
                if (ctx.input->isHeld("engine.tool_camera_down"))
                    transform.position.y -= MOVE_SPEED * dt;
            }

            camera.active      = true;
            camera.aspectRatio = aspectRatio;
            camera.target      = transform.position + forward;

            const glm::mat4 view = glm::lookAt(transform.position, camera.target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4       proj = glm::perspective(camera.fov,
                                                    camera.aspectRatio > 0.0f ? camera.aspectRatio : aspectRatio,
                                                    camera.nearClip,
                                                    camera.farClip);
            proj[1][1] *= -1.0f;

            if (ctx.world.hasComponent<CameraGpuComponent>(state.toolCameraEntity))
            {
                CameraGpuComponent& gpu = ctx.world.getComponent<CameraGpuComponent>(state.toolCameraEntity);
                gpu.viewMatrix          = view;
                gpu.projMatrix          = proj;
                gpu.active              = true;
                gpu.dirty               = true;
            }

            syncActiveCameraView(ctx.world, state.toolCameraEntity, view, proj);
        }

        static void applyPreviewCamera(ECSWorld&                              world,
                                       Entity                                 entity,
                                       TransformComponent&                    transform,
                                       CameraDescriptionComponent&            camera,
                                       const EngineToolPreviewCameraComponent& preview,
                                       float                                  aspectRatio)
        {
            transform.position = preview.position;
            camera.active      = true;
            camera.aspectRatio = aspectRatio;
            camera.fov         = glm::radians(glm::clamp(preview.fov, 20.0f, 110.0f));
            camera.target      = preview.target;
            camera.up          = preview.up;

            const glm::mat4 view = glm::lookAt(transform.position, camera.target, camera.up);
            glm::mat4       proj = glm::perspective(camera.fov,
                                                    camera.aspectRatio > 0.0f ? camera.aspectRatio : aspectRatio,
                                                    camera.nearClip,
                                                    camera.farClip);
            proj[1][1] *= -1.0f;

            if (world.hasComponent<CameraGpuComponent>(entity))
            {
                CameraGpuComponent& gpu = world.getComponent<CameraGpuComponent>(entity);
                gpu.viewMatrix          = view;
                gpu.projMatrix          = proj;
                gpu.active              = true;
                gpu.dirty               = true;
            }

            syncActiveCameraView(world, entity, view, proj);
        }

        void updateMouseLook(const EcsControllerContext& ctx)
        {
            if (ctx.input == nullptr || !ctx.input->isHeld("engine.tool_camera_look"))
            {
                mouseLookReady = false;
                return;
            }

            const double mouseX = ctx.input->mouseX();
            const double mouseY = ctx.input->mouseY();
            if (mouseLookReady)
            {
                const double dx = mouseX - previousMouseX;
                const double dy = mouseY - previousMouseY;
                yaw -= static_cast<float>(dx) * MOUSE_LOOK_SPEED;
                pitch -= static_cast<float>(dy) * MOUSE_LOOK_SPEED;
            }

            previousMouseX = mouseX;
            previousMouseY = mouseY;
            mouseLookReady = true;
        }
    };
} // namespace gts::tools
