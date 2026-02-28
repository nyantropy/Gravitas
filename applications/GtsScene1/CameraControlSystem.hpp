#pragma once

#include <glm.hpp>
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsAction.h"
#include "GtsKey.h"

#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"

// Camera control system.
// Default bindings (W/S = zoom, A/D = orbit) are registered automatically
// on the first update call, so no scene-side setup is required.
// Override bindings via ctx.actions->clearBindings() + bind() after onLoad
// if the application needs different keys.
class CameraControlSystem : public ECSControllerSystem
{
    bool defaultsBound = false;

    void bindDefaults(SceneContext& ctx)
    {
        ctx.actions->bind(GtsAction::ZoomIn,     GtsKey::W);
        ctx.actions->bind(GtsAction::ZoomOut,    GtsKey::S);
        ctx.actions->bind(GtsAction::OrbitLeft,  GtsKey::A);
        ctx.actions->bind(GtsAction::OrbitRight, GtsKey::D);
    }

public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (!defaultsBound)
        {
            bindDefaults(ctx);
            defaultsBound = true;
        }

        const float zoomSpeed  = 6.0f;
        const float orbitSpeed = 1.5f;

        // use unscaled dt so the camera responds even when the scene is paused
        const float dt = ctx.time->unscaledDeltaTime;

        for (Entity e : world.getAllEntitiesWith<CameraDescriptionComponent, TransformComponent>())
        {
            auto& transform = world.getComponent<TransformComponent>(e);

            glm::vec3 pos = transform.position;
            glm::vec3 dir = -pos;
            float dist = glm::length(dir);
            if (dist > 0.0001f)
                dir /= dist;

            if (ctx.actions->isActionActive(GtsAction::ZoomIn))
                pos += dir * zoomSpeed * dt;

            if (ctx.actions->isActionActive(GtsAction::ZoomOut))
                pos -= dir * zoomSpeed * dt;

            float angle = 0.0f;
            if (ctx.actions->isActionActive(GtsAction::OrbitLeft))
                angle += orbitSpeed * dt;
            if (ctx.actions->isActionActive(GtsAction::OrbitRight))
                angle -= orbitSpeed * dt;

            if (std::abs(angle) > 0.0f)
            {
                float c = std::cos(angle);
                float s = std::sin(angle);
                float x = pos.x * c - pos.z * s;
                float z = pos.x * s + pos.z * c;
                pos.x = x;
                pos.z = z;
            }

            transform.position = pos;
        }
    }
};
