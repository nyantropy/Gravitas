#pragma once

#include <algorithm>
#include <limits>
#include <optional>

#include "BoundsComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolRaycast.h"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolStateComponent.h"
#include "TransformComponent.h"

namespace gts::tools
{
    class EngineToolWorldPickerSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            if (!ctx.world.hasAny<EngineToolStateComponent>())
                return;

            EngineToolStateComponent& state = ctx.world.getSingleton<EngineToolStateComponent>();
            if (!state.visible)
            {
                state.hoveredEntity = invalidToolEntity();
                return;
            }

            if (!ctx.world.hasAny<EngineToolInputCaptureComponent>()
                || !ctx.world.hasAny<ActiveCameraViewStateComponent>())
            {
                state.hoveredEntity = invalidToolEntity();
                return;
            }

            const EngineToolInputCaptureComponent& capture =
                ctx.world.getSingleton<EngineToolInputCaptureComponent>();
            if (capture.pointerOverToolUi || capture.toolUiPressed || capture.worldConsumed)
            {
                state.hoveredEntity = invalidToolEntity();
                return;
            }

            const ActiveCameraViewStateComponent& camera =
                ctx.world.getSingleton<ActiveCameraViewStateComponent>();
            if (!camera.valid)
            {
                state.hoveredEntity = invalidToolEntity();
                return;
            }

            const std::optional<EngineToolPickRay> ray = buildToolPickRay(camera, capture.pointerX, capture.pointerY);
            if (!ray)
            {
                state.hoveredEntity = invalidToolEntity();
                return;
            }

            const PickResult pick = pickEntity(ctx.world, *ray);
            state.hoveredEntity = pick.entity;

            if (capture.primaryPressed && isValidToolEntity(pick.entity))
            {
                state.selectedEntity = pick.entity;
                state.selectionSource = EngineToolSelectionSource::WorldPick;
                state.selectionChangedThisFrame = true;
                state.status = "SELECTED " + entityDisplayName(ctx.world, pick.entity);
            }
        }

    private:
        struct PickResult
        {
            Entity entity{InvalidToolEntityId};
            float distance = std::numeric_limits<float>::max();
        };

        static PickResult pickEntity(ECSWorld& world, const EngineToolPickRay& ray)
        {
            PickResult best;
            world.forEach<TransformComponent, BoundsComponent>(
                [&](Entity entity, TransformComponent& transform, BoundsComponent& bounds)
                {
                    if (isToolInternalEntity(world, entity) || !entitySelectable(world, entity))
                        return;

                    const std::optional<float> distance = intersectLocalBounds(ray, transform, bounds);
                    if (!distance || *distance < 0.0f || *distance >= best.distance)
                        return;

                    best.entity = entity;
                    best.distance = *distance;
                });
            return best;
        }
    };
}
