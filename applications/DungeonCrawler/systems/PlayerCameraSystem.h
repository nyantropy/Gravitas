#pragma once

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "components/PlayerComponent.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "components/FloorTransitionStateComponent.h"

// Reads PlayerComponent transition state and updates the camera each frame.
// Interpolates visual position (translation) and yaw (rotation) over
// TRANSITION_DURATION using smoothstep easing. CameraGpuSystem then reads
// TransformComponent.position (eye position) and CameraDescriptionComponent.target
// to build the view matrix.
//
// Returns early while a floor transition is active — FloorTransitionSystem
// drives the camera directly during that window.
class PlayerCameraSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        // FloorTransitionSystem owns the camera while a transition plays
        const auto& ts = world.getSingleton<FloorTransitionStateComponent>();
        if (ts.active) return;

        world.forEach<PlayerComponent, TransformComponent, CameraDescriptionComponent>(
            [&](Entity, PlayerComponent& player,
                TransformComponent& tc, CameraDescriptionComponent& desc)
        {
            if (!desc.active) return;

            // Keep aspect ratio current (handles window resize).
            desc.aspectRatio = ctx.windowAspectRatio;

            // Normal first-person camera — smoothstep easing
            const float t    = player.transitionProgress;
            const float ease = player.inTransition ? t * t * (3.0f - 2.0f * t) : 1.0f;

            // Interpolated world position
            const glm::vec3 visualPos = player.inTransition
                ? glm::mix(player.fromPosition, player.toPosition, ease)
                : player.toPosition;

            // Interpolated yaw — shortest angular path
            float deltaYaw = player.toYaw - player.fromYaw;
            while (deltaYaw >  180.0f) deltaYaw -= 360.0f;
            while (deltaYaw < -180.0f) deltaYaw += 360.0f;
            const float visualYaw = player.fromYaw + ease * deltaYaw;

            // Convert yaw to forward direction vector.
            // Convention: North=0°, East=90°, South=180°, West=270°
            // forward = (sin(yaw), 0, -cos(yaw))
            const float yawRad = glm::radians(visualYaw);
            const glm::vec3 forward = {
                 glm::sin(yawRad),
                 0.0f,
                -glm::cos(yawRad)
            };

            tc.position = visualPos;
            desc.target = visualPos + forward;
        });
    }
};
