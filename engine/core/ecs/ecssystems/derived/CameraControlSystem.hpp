#pragma once

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"

// a basic camera control system that allows zooming in/out, as well as orbiting around the scene
class CameraControlSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            const float zoomSpeed  = 6.0f;
            const float orbitSpeed = 1.5f;

            // use the unscaled delta time here, so we can still move the camera even if we pause the scene
            const float dt = ctx.time->unscaledDeltaTime;

            for (Entity e : world.getAllEntitiesWith<CameraDescriptionComponent, TransformComponent>())
            {
                auto& transform = world.getComponent<TransformComponent>(e);

                glm::vec3 pos = transform.position;

                // we zoom in/out here
                glm::vec3 dir = -pos;

                float dist = glm::length(dir);
                if (dist > 0.0001f)
                {
                    dir /= dist;
                }
                    

                if (ctx.input->isKeyDown(GtsKey::W))
                {
                    pos += dir * zoomSpeed * dt;
                }

                if (ctx.input->isKeyDown(GtsKey::S))
                {
                    pos -= dir * zoomSpeed * dt;
                }

                // orbit around here
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
