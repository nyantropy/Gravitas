#pragma once

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsAction.h"

#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "TetrisCameraControlComponent.hpp"

// Handles camera control input for the Tetris telephoto camera.
//
// This system owns ONLY the control intent (orbit angle, zoom distance).
// It does NOT compute view/projection matrices — that is TetrisCameraSystem's job.
//
// The system runs only on entities with CameraControlOverrideComponent,
// which prevents DefaultCameraControlSystem from also processing them.
class TetrisCameraControlSystem : public ECSControllerSystem
{
    static constexpr float ZOOM_SPEED  = 30.0f;   // world units per second
    static constexpr float ORBIT_SPEED = 1.5f;    // radians per second

public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        // use unscaled dt so the camera responds even when the simulation is paused
        const float dt = ctx.time->unscaledDeltaTime;

        world.forEach<CameraDescriptionComponent,
                      CameraControlOverrideComponent,
                      TetrisCameraControlComponent>(
            [&](Entity, CameraDescriptionComponent&, CameraControlOverrideComponent&,
                TetrisCameraControlComponent& control)
            {
                if (ctx.actions->isActionActive(GtsAction::ZoomIn))
                    control.zoomDistance -= ZOOM_SPEED * dt;

                if (ctx.actions->isActionActive(GtsAction::ZoomOut))
                    control.zoomDistance += ZOOM_SPEED * dt;

                if (ctx.actions->isActionActive(GtsAction::OrbitLeft))
                    control.orbitAngle += ORBIT_SPEED * dt;

                if (ctx.actions->isActionActive(GtsAction::OrbitRight))
                    control.orbitAngle -= ORBIT_SPEED * dt;
            }
        );
    }
};
