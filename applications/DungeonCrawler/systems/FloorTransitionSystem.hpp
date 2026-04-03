#pragma once

#include <cmath>

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "FloorTransitionStateComponent.h"
#include "FloorTransitionTriggerComponent.h"
#include "PlayerComponent.h"
#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"
#include "DungeonFloorSingleton.h"
#include "DungeonManager.h"

// Single owner of all floor-transition logic.
// When the player steps onto a FloorTransitionTriggerComponent tile:
//   1. Instantly teleports the player grid position to the arrival tile.
//   2. Plays a camera animation from the old eye position to the new one.
//   3. Drives the active camera directly while active — PlayerCameraSystem
//      and PlayerMovementSystem both return early while ts.active is set.
class FloorTransitionSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        auto& ts = world.getSingleton<FloorTransitionStateComponent>();

        if (!ts.active)
            checkForTrigger(world, ts);
        else
            updateTransition(world, ctx, ts);
    }

private:
    // ── Trigger detection ─────────────────────────────────────────────────
    void checkForTrigger(ECSWorld& world, FloorTransitionStateComponent& ts)
    {
        int       playerX       = -1;
        int       playerZ       = -1;
        bool      canTrigger    = false;
        glm::vec3 playerWorldPos = {};

        world.forEach<PlayerComponent, CameraDescriptionComponent, TransformComponent>(
            [&](Entity, PlayerComponent& p, CameraDescriptionComponent& desc,
                TransformComponent& ptc)
        {
            if (!desc.active || p.inTransition) return;
            playerX        = p.gridX;
            playerZ        = p.gridZ;
            playerWorldPos = ptc.position;
            canTrigger     = true;
        });

        if (!canTrigger || playerX < 0) return;

        auto& sg = world.getSingleton<DungeonFloorSingleton>();

        world.forEach<FloorTransitionTriggerComponent, TransformComponent>(
            [&](Entity, FloorTransitionTriggerComponent& trigger,
                TransformComponent& triggerTc)
        {
            if (ts.active) return;

            // Subtract the current floor's world offset to convert the trigger's
            // world position back to local grid coordinates for comparison.
            const glm::vec3& curOffset = sg.floorWorldOffset[sg.currentFloorIndex];
            const int tx = static_cast<int>(triggerTc.position.x - curOffset.x);
            const int tz = static_cast<int>(triggerTc.position.z - curOffset.z);
            if (playerX != tx || playerZ != tz) return;

            if (trigger.targetFloor < 0 || trigger.targetFloor > 3 ||
                !sg.allFloors[trigger.targetFloor]) return;

            const GeneratedFloor* dest = sg.allFloors[trigger.targetFloor];

            glm::ivec2 arrival = trigger.arrivalGrid;
            if (arrival.x < 0)
                arrival = dest->playerStart;

            const glm::vec3& destOffset = sg.floorWorldOffset[trigger.targetFloor];
            const glm::vec3 arrivalWorld = {
                arrival.x + destOffset.x + 0.5f,
                destOffset.y + 0.5f,
                arrival.y + destOffset.z + 0.5f
            };

            ts.active      = true;
            ts.type        = trigger.type;
            ts.progress    = 0.0f;
            ts.targetFloor = trigger.targetFloor;
            ts.arrivalGrid = arrival;

            setupCameraAnimation(ts, playerWorldPos, arrivalWorld, trigger.type);

            // Instantly teleport the player to the arrival position so
            // walkability checks use the destination floor from this frame on.
            world.forEach<PlayerComponent, TransformComponent>(
                [&](Entity, PlayerComponent& p, TransformComponent& ptc)
            {
                p.gridX        = arrival.x;
                p.gridZ        = arrival.y;
                p.fromPosition = arrivalWorld;
                p.toPosition   = arrivalWorld;
                p.inTransition = false;
                ptc.position   = arrivalWorld;
            });

            sg.floor             = dest;
            sg.currentFloorIndex = trigger.targetFloor;
            if (sg.run) sg.run->moveToFloor(trigger.targetFloor);
        });
    }

    // ── Camera animation setup per transition type ────────────────────────
    static void setupCameraAnimation(FloorTransitionStateComponent& ts,
                                      const glm::vec3& from,
                                      const glm::vec3& to,
                                      FloorTransitionType type)
    {
        ts.camStart = from;
        ts.camEnd   = to;

        switch (type)
        {
        case FloorTransitionType::Stairs:
        {
            // Look in the direction of travel — works for both A→B and B→A
            const glm::vec3 dir = glm::normalize(to - from);
            ts.lookAtStart = from + dir;
            ts.lookAtEnd   = to   + dir;
            break;
        }

        case FloorTransitionType::Hole:
            // Fall straight down, looking down
            ts.camEnd      = {from.x, to.y, from.z};
            ts.lookAtStart = from + glm::vec3(0.0f, -1.0f, 0.0f);
            ts.lookAtEnd   = ts.camEnd + glm::vec3(0.0f, -1.0f, 0.0f);
            break;

        case FloorTransitionType::Collapse:
            // Same drop as Hole but with shake applied in updateTransition
            ts.camEnd      = {from.x, to.y, from.z};
            ts.lookAtStart = from + glm::vec3(0.0f, -1.0f, 0.0f);
            ts.lookAtEnd   = ts.camEnd + glm::vec3(0.0f, -1.0f, 0.0f);
            break;
        }
    }

    // ── Transition update — called every frame while ts.active ────────────
    void updateTransition(ECSWorld& world, SceneContext& ctx,
                           FloorTransitionStateComponent& ts)
    {
        ts.progress += ctx.time->unscaledDeltaTime / ts.getDuration();
        ts.progress  = glm::clamp(ts.progress, 0.0f, 1.0f);

        const float t    = ts.progress;
        const float ease = t * t * (3.0f - 2.0f * t); // smoothstep

        glm::vec3 camPos;
        glm::vec3 lookAt;

        switch (ts.type)
        {
        case FloorTransitionType::Stairs:
            // Linear path follows the gentle ramp slope
            camPos = glm::mix(ts.camStart, ts.camEnd, ease);
            lookAt = glm::mix(ts.lookAtStart, ts.lookAtEnd, ease);
            break;
        case FloorTransitionType::Hole:
            // Linear drop
            camPos = glm::mix(ts.camStart, ts.camEnd, ease);
            lookAt = glm::mix(ts.lookAtStart, ts.lookAtEnd, ease);
            break;

        case FloorTransitionType::Collapse:
        {
            // Drop with horizontal shake in first half
            camPos = glm::mix(ts.camStart, ts.camEnd, ease);
            if (t < 0.5f)
            {
                const float shake = std::sin(t * 40.0f) * 0.05f * (1.0f - t * 2.0f);
                camPos.x += shake;
                camPos.z += shake * 0.5f;
            }
            lookAt = glm::mix(ts.lookAtStart, ts.lookAtEnd, ease);
            break;
        }
        }

        // Drive the active camera directly
        world.forEach<CameraDescriptionComponent, TransformComponent>(
            [&](Entity, CameraDescriptionComponent& desc, TransformComponent& tc)
        {
            if (!desc.active) return;
            desc.aspectRatio = ctx.windowAspectRatio;
            tc.position      = camPos;
            desc.target      = lookAt;
        });

        if (ts.progress >= 1.0f)
        {
            ts.active   = false;
            ts.progress = 0.0f;
        }
    }

};
