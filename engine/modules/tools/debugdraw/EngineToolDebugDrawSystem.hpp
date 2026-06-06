#pragma once

#include <array>
#include <optional>
#include <unordered_map>

#include "ActiveCameraViewStateComponent.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DebugDrawPrimitives.h"
#include "DebugDrawSettingsComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolCameraStateComponent.h"
#include "EngineGizmoStateComponent.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolRaycast.h"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolStateComponent.h"
#include "GlmConfig.h"
#include "TransformComponent.h"

namespace gts::tools
{
    class EngineToolDebugDrawSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            if (!ctx.world.hasAny<EngineToolStateComponent>())
                return;

            const EngineToolStateComponent& state = ctx.world.getSingleton<EngineToolStateComponent>();
            if (!state.visible)
                return;

            gts::debugdraw::DebugDrawSettingsComponent& settings =
                gts::debugdraw::ensureSettings(ctx.world);
            if (!settings.enabled)
                return;

            gts::debugdraw::DebugDrawQueueComponent& queue =
                gts::debugdraw::ensureQueue(ctx.world);
            const float thickness = settings.lineThickness;
            if (settings.allBounds)
                drawAllBounds(ctx.world, queue, state, thickness);

            drawCameras(ctx.world, state, thickness);

            if (settings.selectedBounds)
                drawSelectedBounds(ctx.world, queue, state, thickness * 1.35f);

            if (settings.transformAxes && !gizmoOwnsSelectedAxes(ctx.world, state))
                drawSelectedAxes(ctx.world, state, settings, thickness * 1.45f);

            if (settings.cameraFrustum && ctx.world.hasAny<ActiveCameraViewStateComponent>())
            {
                gts::debugdraw::frustum(ctx.world,
                                        ctx.world.getSingleton<ActiveCameraViewStateComponent>(),
                                        gts::debugdraw::DebugDrawColor::Purple,
                                        thickness);
            }

            if (settings.pickRay)
                drawPickRay(ctx.world, settings);
        }

    private:
        struct CachedBoundsLines
        {
            TransformComponent transform;
            BoundsComponent bounds;
            gts::debugdraw::DebugDrawColor color = gts::debugdraw::DebugDrawColor::Grey;
            float thickness = 0.0f;
            std::array<gts::debugdraw::DebugDrawLine, 12> lines{};
            size_t lineCount = 0;
            size_t signature = 0;
            uint64_t lastSeenFrame = 0;
            bool valid = false;
        };

        std::unordered_map<entity_id_type, CachedBoundsLines> boundsLineCache;
        uint64_t boundsCacheFrame = 0;

        void drawAllBounds(ECSWorld& world,
                           gts::debugdraw::DebugDrawQueueComponent& queue,
                           const EngineToolStateComponent& state,
                           float thickness)
        {
            ++boundsCacheFrame;
            world.forEach<TransformComponent, BoundsComponent>(
                [&](Entity entity, TransformComponent& transform, BoundsComponent& bounds)
                {
                    if (isToolInternalEntity(world, entity))
                        return;

                    const bool selected = isValidToolEntity(state.selectedEntity)
                        && entity.id == state.selectedEntity.id;
                    const gts::debugdraw::DebugDrawColor color = selected
                        ? gts::debugdraw::DebugDrawColor::Yellow
                        : gts::debugdraw::DebugDrawColor::Grey;
                    const float effectiveThickness = selected ? thickness * 1.35f : thickness;
                    appendCachedBounds(queue, entity, transform, bounds, color, effectiveThickness);
                });

            pruneBoundsCache();
        }

        static void drawSelectedBounds(ECSWorld& world,
                                       gts::debugdraw::DebugDrawQueueComponent& queue,
                                       const EngineToolStateComponent& state,
                                       float thickness)
        {
            if (!isValidToolEntity(state.selectedEntity)
                || isToolInternalEntity(world, state.selectedEntity)
                || !world.hasComponent<TransformComponent>(state.selectedEntity)
                || !world.hasComponent<BoundsComponent>(state.selectedEntity))
            {
                return;
            }

            gts::debugdraw::bounds(queue,
                                   world.getComponent<TransformComponent>(state.selectedEntity),
                                   world.getComponent<BoundsComponent>(state.selectedEntity),
                                   gts::debugdraw::DebugDrawColor::Yellow,
                                   thickness);
        }

        static void drawCameras(ECSWorld& world,
                                const EngineToolStateComponent& state,
                                float thickness)
        {
            world.forEach<TransformComponent, CameraDescriptionComponent>(
                [&](Entity entity, TransformComponent& transform, CameraDescriptionComponent& camera)
                {
                    if (isCurrentViewCamera(world, entity))
                        return;

                    const gts::debugdraw::DebugDrawColor color =
                        cameraDebugColor(world, state, entity, camera);
                    const glm::mat4 view = glm::lookAt(transform.position, camera.target, camera.up);
                    glm::mat4 proj = glm::perspective(camera.fov,
                                                      camera.aspectRatio > 0.0f ? camera.aspectRatio : 1.0f,
                                                      camera.nearClip,
                                                      camera.farClip);
                    proj[1][1] *= -1.0f;

                    gts::debugdraw::frustumFromViewProj(world, proj * view, color, thickness);
                    drawCameraMarker(world, transform, camera, color, thickness * 1.45f);
                });
        }

        static gts::debugdraw::DebugDrawColor cameraDebugColor(ECSWorld& world,
                                                               const EngineToolStateComponent& state,
                                                               Entity entity,
                                                               const CameraDescriptionComponent& camera)
        {
            if (isValidToolEntity(state.selectedEntity)
                && state.selectedEntity.id == entity.id)
            {
                return gts::debugdraw::DebugDrawColor::Yellow;
            }

            if (world.hasAny<EngineToolCameraStateComponent>()
                && world.getSingleton<EngineToolCameraStateComponent>().toolCameraEntity.id == entity.id)
            {
                return gts::debugdraw::DebugDrawColor::Cyan;
            }

            return camera.active
                ? gts::debugdraw::DebugDrawColor::Green
                : gts::debugdraw::DebugDrawColor::Blue;
        }

        static bool isCurrentViewCamera(ECSWorld& world, Entity entity)
        {
            if (world.hasAny<ActiveCameraViewStateComponent>())
            {
                const ActiveCameraViewStateComponent& camera =
                    world.getSingleton<ActiveCameraViewStateComponent>();
                return camera.valid && camera.activeCamera.id == entity.id;
            }

            if (!world.hasComponent<CameraDescriptionComponent>(entity))
                return false;

            return world.getComponent<CameraDescriptionComponent>(entity).active;
        }

        static void drawCameraMarker(ECSWorld& world,
                                     const TransformComponent& transform,
                                     const CameraDescriptionComponent& camera,
                                     gts::debugdraw::DebugDrawColor color,
                                     float thickness)
        {
            const glm::vec3 forward = safeNormalized(camera.target - transform.position,
                                                     {0.0f, 0.0f, -1.0f});
            glm::vec3 right = glm::cross(forward, camera.up);
            if (glm::dot(right, right) <= 0.000001f)
                right = {1.0f, 0.0f, 0.0f};
            right = glm::normalize(right);
            const glm::vec3 up = glm::normalize(glm::cross(right, forward));

            const float size = 0.34f;
            const glm::vec3 center = transform.position;
            const glm::vec3 a = center - right * size - up * size;
            const glm::vec3 b = center + right * size - up * size;
            const glm::vec3 c = center + right * size + up * size;
            const glm::vec3 d = center - right * size + up * size;
            const glm::vec3 nose = center + forward * (size * 1.55f);

            gts::debugdraw::line(world, a, b, color, thickness);
            gts::debugdraw::line(world, b, c, color, thickness);
            gts::debugdraw::line(world, c, d, color, thickness);
            gts::debugdraw::line(world, d, a, color, thickness);
            gts::debugdraw::line(world, center, nose, color, thickness);
        }

        static void drawSelectedAxes(ECSWorld& world,
                                     const EngineToolStateComponent& state,
                                     const gts::debugdraw::DebugDrawSettingsComponent& settings,
                                     float thickness)
        {
            if (!isValidToolEntity(state.selectedEntity)
                || isToolInternalEntity(world, state.selectedEntity)
                || !world.hasComponent<TransformComponent>(state.selectedEntity))
            {
                return;
            }

            bool localSpace = false;
            float length = settings.axisLength;
            if (world.hasAny<EngineGizmoStateComponent>())
            {
                const EngineGizmoStateComponent& gizmo = world.getSingleton<EngineGizmoStateComponent>();
                localSpace = gizmo.space == EngineGizmoSpace::Local;
                length = gizmo.handleLength;
            }

            gts::debugdraw::basis(world,
                                  world.getComponent<TransformComponent>(state.selectedEntity),
                                  length,
                                  thickness,
                                  localSpace);
        }

        static bool gizmoOwnsSelectedAxes(ECSWorld& world,
                                          const EngineToolStateComponent& state)
        {
            return world.hasAny<EngineGizmoStateComponent>()
                && world.getSingleton<EngineGizmoStateComponent>().enabled
                && isValidToolEntity(state.selectedEntity)
                && !isToolInternalEntity(world, state.selectedEntity)
                && world.hasComponent<TransformComponent>(state.selectedEntity);
        }

        static glm::vec3 safeNormalized(const glm::vec3& value, const glm::vec3& fallback)
        {
            if (glm::dot(value, value) <= 0.000001f)
                return fallback;
            return glm::normalize(value);
        }

        static void drawPickRay(ECSWorld& world,
                                const gts::debugdraw::DebugDrawSettingsComponent& settings)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>()
                || !world.hasAny<ActiveCameraViewStateComponent>())
            {
                return;
            }

            const EngineToolInputCaptureComponent& capture =
                world.getSingleton<EngineToolInputCaptureComponent>();
            if (capture.pointerOverToolUi)
                return;

            const ActiveCameraViewStateComponent& camera =
                world.getSingleton<ActiveCameraViewStateComponent>();
            if (!camera.valid)
                return;

            const std::optional<EngineToolPickRay> pickRay =
                buildToolPickRay(camera, capture.pointerX, capture.pointerY);
            if (!pickRay)
                return;

            gts::debugdraw::ray(world,
                                pickRay->origin,
                                pickRay->direction,
                                settings.pickRayLength,
                                gts::debugdraw::DebugDrawColor::Cyan,
                                settings.lineThickness);
        }

        void appendCachedBounds(gts::debugdraw::DebugDrawQueueComponent& queue,
                                Entity entity,
                                const TransformComponent& transform,
                                const BoundsComponent& bounds,
                                gts::debugdraw::DebugDrawColor color,
                                float thickness)
        {
            CachedBoundsLines& cached = boundsLineCache[entity.id];
            if (!cached.valid
                || !sameTransform(cached.transform, transform)
                || !sameBounds(cached.bounds, bounds)
                || cached.color != color
                || cached.thickness != thickness)
            {
                cached.transform = transform;
                cached.bounds = bounds;
                cached.color = color;
                cached.thickness = thickness;
                cached.lineCount = gts::debugdraw::buildBoundsLines(transform,
                                                                    bounds,
                                                                    color,
                                                                    thickness,
                                                                    cached.lines);
                cached.signature = gts::debugdraw::debugDrawHashLines(cached.lines.data(),
                                                                      cached.lineCount);
                cached.valid = true;
            }

            cached.lastSeenFrame = boundsCacheFrame;
            queue.appendLines(color, cached.lines.data(), cached.lineCount, cached.signature);
        }

        void pruneBoundsCache()
        {
            for (auto it = boundsLineCache.begin(); it != boundsLineCache.end();)
            {
                if (it->second.lastSeenFrame == boundsCacheFrame)
                {
                    ++it;
                    continue;
                }

                it = boundsLineCache.erase(it);
            }
        }

        static bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }

        static bool sameTransform(const TransformComponent& lhs, const TransformComponent& rhs)
        {
            return sameVec3(lhs.position, rhs.position)
                && sameVec3(lhs.rotation, rhs.rotation)
                && sameVec3(lhs.scale, rhs.scale);
        }

        static bool sameBounds(const BoundsComponent& lhs, const BoundsComponent& rhs)
        {
            return sameVec3(lhs.min, rhs.min) && sameVec3(lhs.max, rhs.max);
        }
    };
}
