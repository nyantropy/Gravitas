#pragma once

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "CameraComponent.h"
#include "TransformComponent.h"

class CameraControlSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float zoomSpeed  = 6.0f;   // units per second
        const float orbitSpeed = 1.5f;   // radians per second

        const float dt = ctx.time->unscaledDeltaTime;

        for (Entity e : world.getAllEntitiesWith<CameraComponent, TransformComponent>())
        {
            auto& transform = world.getComponent<TransformComponent>(e);

            glm::vec3 pos = transform.position;

            // -----------------------------
            // Zoom towards / away from origin
            // -----------------------------
            glm::vec3 dir = -pos;

            float dist = glm::length(dir);
            if (dist > 0.0001f)
                dir /= dist;

            if (ctx.input->isKeyDown(GtsKey::W))
            {
                pos += dir * zoomSpeed * dt;
            }

            if (ctx.input->isKeyDown(GtsKey::S))
            {
                pos -= dir * zoomSpeed * dt;
            }

            // -----------------------------
            // Orbit around origin (Y axis)
            // -----------------------------
            float angle = 0.0f;

            if (ctx.input->isKeyDown(GtsKey::A))
                angle += orbitSpeed * dt;

            if (ctx.input->isKeyDown(GtsKey::D))
                angle -= orbitSpeed * dt;

            if (std::abs(angle) > 0.0f)
            {
                float c = std::cos(angle);
                float s = std::sin(angle);

                // rotate around Y
                float x = pos.x * c - pos.z * s;
                float z = pos.x * s + pos.z * c;

                pos.x = x;
                pos.z = z;
            }

            transform.position = pos;
        }
    }
};
