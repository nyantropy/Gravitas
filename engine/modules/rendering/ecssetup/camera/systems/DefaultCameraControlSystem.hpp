#pragma once

#include "GlmConfig.h"
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "GtsAction.h"
#include "GtsKey.h"

#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "DebugCameraStateComponent.h"
#include "TransformComponent.h"

// Baseline orbit camera installed automatically by installRendererFeature().
// Binds arrow keys to zoom/orbit on first update.
// Skips any camera entity marked with CameraControlOverrideComponent so that
// application-specific control systems take over.
// Note: CameraOverrideComponent (GPU matrix ownership) is orthogonal and not checked here.
class DefaultCameraControlSystem : public ECSControllerSystem
{
    bool defaultsBound = false;

    void bindDefaults(const EcsControllerContext& ctx)
    {
        ctx.actions->bind(GtsAction::ZoomIn,     GtsKey::ArrowUp);
        ctx.actions->bind(GtsAction::ZoomOut,    GtsKey::ArrowDown);
        ctx.actions->bind(GtsAction::OrbitLeft,  GtsKey::ArrowLeft);
        ctx.actions->bind(GtsAction::OrbitRight, GtsKey::ArrowRight);
    }

public:
    void update(const EcsControllerContext& ctx) override
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

        if (ctx.world.hasAny<DebugCameraStateComponent>()
            && ctx.world.getSingleton<DebugCameraStateComponent>().active)
            return;

        for (Entity e : ctx.world.getAllEntitiesWith<CameraDescriptionComponent, TransformComponent>())
        {
            if (ctx.world.hasComponent<CameraControlOverrideComponent>(e))
                continue;

            auto& transform = ctx.world.getComponent<TransformComponent>(e);

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
