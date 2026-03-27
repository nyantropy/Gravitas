#pragma once

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "PlayerComponent.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"

// Reads PlayerComponent facing and TransformComponent position,
// updates CameraDescriptionComponent so CameraGpuSystem builds the
// correct first-person view matrix each frame.
// Only runs when the player camera is active (not in debug mode).
class PlayerCameraSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        world.forEach<PlayerComponent, TransformComponent, CameraDescriptionComponent>(
            [&](Entity, PlayerComponent& player,
                TransformComponent& tc, CameraDescriptionComponent& desc)
        {
            if (!desc.active) return;

            // Keep aspect ratio current (handles window resize).
            desc.aspectRatio = ctx.windowAspectRatio;

            // Point camera in current facing direction.
            glm::vec3 fwd = facingToForward3D(player.facing);
            desc.target   = tc.position + fwd;
        });
    }

private:
    static glm::vec3 facingToForward3D(int facing)
    {
        switch (facing)
        {
            case 0: return { 0.0f, 0.0f, -1.0f}; // North
            case 1: return { 1.0f, 0.0f,  0.0f}; // East
            case 2: return { 0.0f, 0.0f,  1.0f}; // South
            case 3: return {-1.0f, 0.0f,  0.0f}; // West
            default: return {0.0f, 0.0f, -1.0f};
        }
    }
};
