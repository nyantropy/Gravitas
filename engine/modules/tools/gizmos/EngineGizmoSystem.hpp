#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

#include "ActiveCameraViewStateComponent.h"
#include "DebugDrawPrimitives.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineGizmoStateComponent.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolRaycast.h"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolStateComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformMatrixHelpers.h"
#include "WorldTransformComponent.h"

namespace gts::tools
{
    class EngineGizmoSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            EngineGizmoStateComponent& gizmo = ensureGizmoState(ctx.world);
            gizmo.hoveredAxis = EngineGizmoAxis::None;

            if (!ctx.world.hasAny<EngineToolStateComponent>()
                || !ctx.world.hasAny<EngineToolInputCaptureComponent>()
                || !ctx.world.hasAny<ActiveCameraViewStateComponent>())
            {
                clearDrag(gizmo);
                return;
            }

            const EngineToolStateComponent& toolState = ctx.world.getSingleton<EngineToolStateComponent>();
            EngineToolInputCaptureComponent& capture = ctx.world.getSingleton<EngineToolInputCaptureComponent>();
            const ActiveCameraViewStateComponent& camera = ctx.world.getSingleton<ActiveCameraViewStateComponent>();

            if (!toolState.visible || !gizmo.enabled || !camera.valid)
            {
                clearDrag(gizmo);
                return;
            }

            if (!isValidToolEntity(toolState.selectedEntity)
                || isToolInternalEntity(ctx.world, toolState.selectedEntity)
                || !ctx.world.hasComponent<TransformComponent>(toolState.selectedEntity)
                || !ctx.world.hasComponent<WorldTransformComponent>(toolState.selectedEntity))
            {
                clearDrag(gizmo);
                return;
            }

            TransformComponent& transform = ctx.world.getComponent<TransformComponent>(toolState.selectedEntity);
            const glm::mat4& worldMatrix =
                ctx.world.getComponent<WorldTransformComponent>(toolState.selectedEntity).matrix;

            if (capture.pointerOverToolUi || capture.toolUiPressed)
            {
                if (!isDragging(gizmo))
                {
                    drawGizmo(ctx.world, gizmo, worldMatrix);
                    return;
                }
            }

            if (!capture.pointerOverViewport && !isDragging(gizmo))
            {
                drawGizmo(ctx.world, gizmo, worldMatrix);
                return;
            }

            const std::optional<EngineToolPickRay> ray =
                buildToolPickRay(camera, capture.viewportPointerX, capture.viewportPointerY);
            if (!ray)
            {
                drawGizmo(ctx.world, gizmo, worldMatrix);
                return;
            }

            if (isDragging(gizmo))
            {
                capture.worldConsumed = true;
                updateDrag(ctx.world, gizmo, capture, toolState.selectedEntity, transform, *ray);
                drawGizmo(ctx.world, gizmo, worldMatrix);
                return;
            }

            gizmo.hoveredAxis = pickAxis(gizmo, worldMatrix, *ray);
            if (gizmo.hoveredAxis != EngineGizmoAxis::None)
                capture.worldConsumed = true;

            if (capture.primaryPressed && gizmo.hoveredAxis != EngineGizmoAxis::None)
            {
                beginDrag(gizmo, capture, worldMatrix, *ray, toolState.selectedEntity);
                capture.worldConsumed = true;
            }

            drawGizmo(ctx.world, gizmo, worldMatrix);
        }

    private:
        static constexpr entity_id_type InvalidEntityId = std::numeric_limits<entity_id_type>::max();

        static EngineGizmoStateComponent& ensureGizmoState(ECSWorld& world)
        {
            if (!world.hasAny<EngineGizmoStateComponent>())
                return world.createSingleton<EngineGizmoStateComponent>();
            return world.getSingleton<EngineGizmoStateComponent>();
        }

        static bool isDragging(const EngineGizmoStateComponent& gizmo)
        {
            return gizmo.activeAxis != EngineGizmoAxis::None && gizmo.activeEntity.id != InvalidEntityId;
        }

        static void clearDrag(EngineGizmoStateComponent& gizmo)
        {
            gizmo.hoveredAxis = EngineGizmoAxis::None;
            gizmo.activeAxis = EngineGizmoAxis::None;
            gizmo.activeEntity = Entity{InvalidEntityId};
        }

        static glm::vec3 safeAxis(const glm::vec3& axis, const glm::vec3& fallback)
        {
            const float length = glm::length(axis);
            return length <= 0.0001f ? fallback : axis / length;
        }

        static std::array<glm::vec3, 3> axisDirections(const EngineGizmoStateComponent& gizmo,
                                                       const glm::mat4& worldMatrix)
        {
            if (gizmo.space == EngineGizmoSpace::World)
            {
                return {
                    glm::vec3(1.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f)
                };
            }

            const glm::mat3 basis = glm::mat3(worldMatrix);
            return {
                safeAxis(basis[0], glm::vec3(1.0f, 0.0f, 0.0f)),
                safeAxis(basis[1], glm::vec3(0.0f, 1.0f, 0.0f)),
                safeAxis(basis[2], glm::vec3(0.0f, 0.0f, 1.0f))
            };
        }

        static glm::vec3 axisDirection(const EngineGizmoStateComponent& gizmo,
                                       const glm::mat4& worldMatrix,
                                       EngineGizmoAxis axis)
        {
            const auto axes = axisDirections(gizmo, worldMatrix);
            switch (axis)
            {
                case EngineGizmoAxis::X: return axes[0];
                case EngineGizmoAxis::Y: return axes[1];
                case EngineGizmoAxis::Z: return axes[2];
                case EngineGizmoAxis::None: break;
            }
            return axes[0];
        }

        static gts::debugdraw::DebugDrawColor axisColor(EngineGizmoAxis axis)
        {
            switch (axis)
            {
                case EngineGizmoAxis::X: return gts::debugdraw::DebugDrawColor::Red;
                case EngineGizmoAxis::Y: return gts::debugdraw::DebugDrawColor::Green;
                case EngineGizmoAxis::Z: return gts::debugdraw::DebugDrawColor::Blue;
                case EngineGizmoAxis::None: break;
            }
            return gts::debugdraw::DebugDrawColor::Yellow;
        }

        static void drawGizmo(ECSWorld& world,
                              const EngineGizmoStateComponent& gizmo,
                              const glm::mat4& worldMatrix)
        {
            const auto axes = axisDirections(gizmo, worldMatrix);
            const glm::vec3 origin = gts::transform::worldPositionFromMatrix(worldMatrix);
            constexpr std::array<EngineGizmoAxis, 3> axisIds{
                EngineGizmoAxis::X,
                EngineGizmoAxis::Y,
                EngineGizmoAxis::Z
            };

            for (size_t i = 0; i < axes.size(); ++i)
            {
                const EngineGizmoAxis axis = axisIds[i];
                const bool emphasized = axis == gizmo.hoveredAxis || axis == gizmo.activeAxis;
                gts::debugdraw::line(world,
                                     origin,
                                     origin + axes[i] * gizmo.handleLength,
                                     emphasized ? gts::debugdraw::DebugDrawColor::Yellow : axisColor(axis),
                                     emphasized ? 0.070f : 0.045f);
            }
        }

        static EngineGizmoAxis pickAxis(const EngineGizmoStateComponent& gizmo,
                                        const glm::mat4& worldMatrix,
                                        const EngineToolPickRay& ray)
        {
            const auto axes = axisDirections(gizmo, worldMatrix);
            const glm::vec3 origin = gts::transform::worldPositionFromMatrix(worldMatrix);
            const float distanceScale = glm::length(origin - ray.origin) * 0.015f;
            const float threshold = std::max(gizmo.pickRadius, distanceScale);

            EngineGizmoAxis bestAxis = EngineGizmoAxis::None;
            float bestDistance = std::numeric_limits<float>::max();
            constexpr std::array<EngineGizmoAxis, 3> axisIds{
                EngineGizmoAxis::X,
                EngineGizmoAxis::Y,
                EngineGizmoAxis::Z
            };

            for (size_t i = 0; i < axes.size(); ++i)
            {
                float rayT = 0.0f;
                float axisT = 0.0f;
                const float distance = distanceRayToSegment(ray,
                                                            origin,
                                                            origin + axes[i] * gizmo.handleLength,
                                                            &rayT,
                                                            &axisT);
                if (rayT < 0.0f || axisT < 0.08f || distance > threshold || distance >= bestDistance)
                    continue;

                bestAxis = axisIds[i];
                bestDistance = distance;
            }

            return bestAxis;
        }

        static glm::vec3 dragPlaneNormal(const EngineToolPickRay& ray, const glm::vec3& axis)
        {
            glm::vec3 side = glm::cross(ray.direction, axis);
            if (glm::length(side) <= 0.0001f)
                side = std::abs(axis.y) > 0.8f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

            glm::vec3 normal = glm::cross(axis, glm::normalize(side));
            if (glm::length(normal) <= 0.0001f)
                normal = glm::vec3(0.0f, 0.0f, 1.0f);
            return glm::normalize(normal);
        }

        static void beginDrag(EngineGizmoStateComponent& gizmo,
                              EngineToolInputCaptureComponent& capture,
                              const glm::mat4& worldMatrix,
                              const EngineToolPickRay& ray,
                              Entity entity)
        {
            const glm::vec3 axis = axisDirection(gizmo, worldMatrix, gizmo.hoveredAxis);
            const glm::vec3 origin = gts::transform::worldPositionFromMatrix(worldMatrix);
            const glm::vec3 normal = dragPlaneNormal(ray, axis);
            const std::optional<glm::vec3> hit = intersectRayPlane(ray, origin, normal);
            if (!hit)
                return;

            gizmo.activeAxis = gizmo.hoveredAxis;
            gizmo.activeEntity = entity;
            gizmo.dragAxisWorld = axis;
            gizmo.dragPlaneNormal = normal;
            gizmo.dragStartHit = *hit;
            gizmo.dragStartPosition = origin;
            capture.worldConsumed = true;
        }

        static glm::vec3 parentLocalPosition(ECSWorld& world, Entity entity, const glm::vec3& worldPosition)
        {
            if (!world.hasComponent<HierarchyComponent>(entity))
                return worldPosition;

            const Entity parent = world.getComponent<HierarchyComponent>(entity).parent;
            if (parent == INVALID_ENTITY)
                return worldPosition;

            const glm::mat4 parentWorldMatrix = gts::transform::resolvedWorldMatrix(world, parent);
            return glm::vec3(glm::inverse(parentWorldMatrix) * glm::vec4(worldPosition, 1.0f));
        }

        static void updateDrag(ECSWorld& world,
                               EngineGizmoStateComponent& gizmo,
                               const EngineToolInputCaptureComponent& capture,
                               Entity entity,
                               TransformComponent& transform,
                               const EngineToolPickRay& ray)
        {
            if (capture.primaryReleased || !capture.primaryDown)
            {
                clearDrag(gizmo);
                return;
            }

            const std::optional<glm::vec3> hit =
                intersectRayPlane(ray, gizmo.dragStartPosition, gizmo.dragPlaneNormal);
            if (!hit)
                return;

            float amount = glm::dot(*hit - gizmo.dragStartHit, gizmo.dragAxisWorld);
            if (gizmo.snapEnabled && gizmo.snapStep > 0.0001f)
                amount = std::round(amount / gizmo.snapStep) * gizmo.snapStep;

            const glm::vec3 worldPosition = gizmo.dragStartPosition + gizmo.dragAxisWorld * amount;
            transform.position = parentLocalPosition(world, entity, worldPosition);
            if (gizmo.activeEntity.id != InvalidEntityId)
                gts::transform::markDirty(world, gizmo.activeEntity);
        }
    };
}
