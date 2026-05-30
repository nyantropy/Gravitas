#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "ActiveCameraViewStateComponent.h"
#include "BoundsComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolStateComponent.h"
#include "GlmConfig.h"
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
            if (capture.pointerOverToolUi || capture.toolUiPressed)
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

            const std::optional<PickRay> ray = buildRay(camera, capture.pointerX, capture.pointerY);
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
        struct PickRay
        {
            glm::vec3 origin{0.0f};
            glm::vec3 direction{0.0f, 0.0f, -1.0f};
        };

        struct PickResult
        {
            Entity entity{InvalidToolEntityId};
            float distance = std::numeric_limits<float>::max();
        };

        static std::optional<PickRay> buildRay(const ActiveCameraViewStateComponent& camera,
                                               float pointerX,
                                               float pointerY)
        {
            const glm::mat4 invViewProj = glm::inverse(camera.viewProjMatrix);
            const float ndcX = pointerX * 2.0f - 1.0f;
            const float ndcY = pointerY * 2.0f - 1.0f;

            glm::vec4 nearPoint = invViewProj * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
            glm::vec4 farPoint = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
            if (std::abs(nearPoint.w) <= 0.00001f || std::abs(farPoint.w) <= 0.00001f)
                return std::nullopt;

            nearPoint /= nearPoint.w;
            farPoint /= farPoint.w;

            const glm::vec3 origin = glm::vec3(nearPoint);
            const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
            const float length = glm::length(direction);
            if (length <= 0.00001f)
                return std::nullopt;

            return PickRay{origin, direction / length};
        }

        static PickResult pickEntity(ECSWorld& world, const PickRay& ray)
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

        static std::optional<float> intersectLocalBounds(const PickRay& worldRay,
                                                         const TransformComponent& transform,
                                                         const BoundsComponent& bounds)
        {
            const glm::mat4 inverseModel = glm::inverse(transform.getModelMatrix());
            const glm::vec3 localOrigin = glm::vec3(inverseModel * glm::vec4(worldRay.origin, 1.0f));
            const glm::vec3 localDirection = glm::vec3(inverseModel * glm::vec4(worldRay.direction, 0.0f));

            float tMin = 0.0f;
            float tMax = std::numeric_limits<float>::max();

            if (!slab(localOrigin.x, localDirection.x, bounds.min.x, bounds.max.x, tMin, tMax)) return std::nullopt;
            if (!slab(localOrigin.y, localDirection.y, bounds.min.y, bounds.max.y, tMin, tMax)) return std::nullopt;
            if (!slab(localOrigin.z, localDirection.z, bounds.min.z, bounds.max.z, tMin, tMax)) return std::nullopt;

            return tMin;
        }

        static bool slab(float origin,
                         float direction,
                         float minValue,
                         float maxValue,
                         float& tMin,
                         float& tMax)
        {
            if (std::abs(direction) <= 0.00001f)
                return origin >= minValue && origin <= maxValue;

            float t1 = (minValue - origin) / direction;
            float t2 = (maxValue - origin) / direction;
            if (t1 > t2)
                std::swap(t1, t2);

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            return tMin <= tMax;
        }
    };
}
