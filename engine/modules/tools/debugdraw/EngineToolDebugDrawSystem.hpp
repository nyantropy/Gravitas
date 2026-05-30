#pragma once

#include <optional>

#include "ActiveCameraViewStateComponent.h"
#include "BoundsComponent.h"
#include "DebugDrawPrimitives.h"
#include "DebugDrawSettingsComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineGizmoStateComponent.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolRaycast.h"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolStateComponent.h"
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

            const float thickness = settings.lineThickness;
            if (settings.allBounds)
                drawAllBounds(ctx.world, state, thickness);

            if (settings.selectedBounds)
                drawSelectedBounds(ctx.world, state, thickness * 1.35f);

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
        static void drawAllBounds(ECSWorld& world,
                                  const EngineToolStateComponent& state,
                                  float thickness)
        {
            world.forEach<TransformComponent, BoundsComponent>(
                [&](Entity entity, TransformComponent& transform, BoundsComponent& bounds)
                {
                    if (isToolInternalEntity(world, entity))
                        return;

                    const bool selected = isValidToolEntity(state.selectedEntity)
                        && entity.id == state.selectedEntity.id;
                    gts::debugdraw::bounds(world,
                                           transform,
                                           bounds,
                                           selected
                                               ? gts::debugdraw::DebugDrawColor::Yellow
                                               : gts::debugdraw::DebugDrawColor::Grey,
                                           selected ? thickness * 1.35f : thickness);
                });
        }

        static void drawSelectedBounds(ECSWorld& world,
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

            gts::debugdraw::bounds(world,
                                   world.getComponent<TransformComponent>(state.selectedEntity),
                                   world.getComponent<BoundsComponent>(state.selectedEntity),
                                   gts::debugdraw::DebugDrawColor::Yellow,
                                   thickness);
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
    };
}
