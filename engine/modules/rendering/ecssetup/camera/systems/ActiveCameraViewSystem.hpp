#pragma once

#include <limits>

#include "ActiveCameraViewStateComponent.h"
#include "CameraGpuComponent.h"
#include "ECSControllerSystem.hpp"
#include "RenderCameraSelectionComponent.h"

class ActiveCameraViewSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        ActiveCameraViewStateComponent* state = nullptr;
        if (ctx.world.hasAny<ActiveCameraViewStateComponent>())
            state = &ctx.world.getSingleton<ActiveCameraViewStateComponent>();
        else
            state = &ctx.world.createSingleton<ActiveCameraViewStateComponent>();

        state->activeCamera   = Entity{ std::numeric_limits<entity_id_type>::max() };
        state->viewMatrix     = glm::mat4(1.0f);
        state->projMatrix     = glm::mat4(1.0f);
        state->viewProjMatrix = glm::mat4(1.0f);
        state->valid          = false;

        if (ctx.world.hasAny<RenderCameraSelectionComponent>())
        {
            const Entity preferred =
                ctx.world.getSingleton<RenderCameraSelectionComponent>().preferredCamera;
            if (preferred.id != std::numeric_limits<entity_id_type>::max()
                && ctx.world.hasComponent<CameraGpuComponent>(preferred))
            {
                const CameraGpuComponent& camera = ctx.world.getComponent<CameraGpuComponent>(preferred);
                if (camera.active)
                {
                    state->activeCamera    = preferred;
                    state->viewMatrix      = camera.viewMatrix;
                    state->projMatrix      = camera.projMatrix;
                    state->viewProjMatrix  = camera.projMatrix * camera.viewMatrix;
                    state->valid           = true;
                    return;
                }
            }
        }

        ctx.world.forEach<CameraGpuComponent>(
            [&](Entity entity, CameraGpuComponent& camera)
        {
            if (state->valid || !camera.active)
                return;

            state->activeCamera    = entity;
            state->viewMatrix      = camera.viewMatrix;
            state->projMatrix      = camera.projMatrix;
            state->viewProjMatrix  = camera.projMatrix * camera.viewMatrix;
            state->valid           = true;
        });
    }
};
