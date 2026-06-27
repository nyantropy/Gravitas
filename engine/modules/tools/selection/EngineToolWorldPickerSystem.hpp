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
            if (!state.visible || state.editorMode == EditorMode::ParticleEditor)
            {
                state.hoveredEntity = invalidToolEntity();
                cacheValid = false;
                return;
            }

            if (!ctx.world.hasAny<EngineToolInputCaptureComponent>()
                || !ctx.world.hasAny<ActiveCameraViewStateComponent>())
            {
                state.hoveredEntity = invalidToolEntity();
                cacheValid = false;
                return;
            }

            const EngineToolInputCaptureComponent& capture =
                ctx.world.getSingleton<EngineToolInputCaptureComponent>();
            if (capture.pointerOverToolUi || capture.toolUiPressed || capture.worldConsumed
                || !capture.pointerOverViewport)
            {
                state.hoveredEntity = invalidToolEntity();
                cacheValid = false;
                return;
            }

            const ActiveCameraViewStateComponent& camera =
                ctx.world.getSingleton<ActiveCameraViewStateComponent>();
            if (!camera.valid)
            {
                state.hoveredEntity = invalidToolEntity();
                cacheValid = false;
                return;
            }

            const bool pointerChanged =
                !cacheValid
                || capture.viewportPointerX != cachedPointerX
                || capture.viewportPointerY != cachedPointerY;
            const bool cameraChanged =
                !cacheValid
                || !sameMatrix(camera.viewProjMatrix, cachedViewProjMatrix);
            const bool shouldPick = pointerChanged || cameraChanged || capture.primaryPressed;

            if (!shouldPick)
            {
                if (cachedEntityStillPickable(ctx.world, cachedHoveredEntity))
                {
                    state.hoveredEntity = cachedHoveredEntity;
                    return;
                }

                cacheValid = false;
            }

            const std::optional<EngineToolPickRay> ray =
                buildToolPickRay(camera, capture.viewportPointerX, capture.viewportPointerY);
            if (!ray)
            {
                state.hoveredEntity = invalidToolEntity();
                cacheValid = false;
                return;
            }

            const PickResult pick = pickEntity(ctx.world, *ray);
            state.hoveredEntity = pick.entity;
            cachedHoveredEntity = pick.entity;
            cachedPointerX = capture.viewportPointerX;
            cachedPointerY = capture.viewportPointerY;
            cachedViewProjMatrix = camera.viewProjMatrix;
            cacheValid = true;

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

        bool cacheValid = false;
        float cachedPointerX = 0.0f;
        float cachedPointerY = 0.0f;
        glm::mat4 cachedViewProjMatrix = glm::mat4(1.0f);
        Entity cachedHoveredEntity{InvalidToolEntityId};

        static bool sameMatrix(const glm::mat4& lhs, const glm::mat4& rhs)
        {
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (lhs[column][row] != rhs[column][row])
                        return false;
                }
            }

            return true;
        }

        static bool cachedEntityStillPickable(ECSWorld& world, Entity entity)
        {
            if (!isValidToolEntity(entity))
                return true;

            return world.hasComponent<TransformComponent>(entity)
                && world.hasComponent<BoundsComponent>(entity)
                && !isToolInternalEntity(world, entity)
                && entitySelectable(world, entity);
        }

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
