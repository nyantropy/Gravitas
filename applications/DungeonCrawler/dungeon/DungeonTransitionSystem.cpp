#include "dungeon/DungeonTransitionSystem.h"

#include "CameraDescriptionComponent.h"
#include "DebugCameraStateComponent.h"
#include "SceneContext.h"
#include "TransformComponent.h"
#include "components/FloorTransitionStateComponent.h"
#include "components/PlayerComponent.h"
#include "ECSWorld.hpp"
#include "dungeon/DungeonManager.h"
#include "dungeon/DungeonVisibilityState.h"

namespace
{
    constexpr float STAIR_TRANSITION_DROP = 1.4f;
    constexpr float STAIR_TRANSITION_FORWARD = 0.35f;

    glm::vec3 gridToWorld(const glm::ivec2& gridPos)
    {
        return {gridPos.x + 0.5f, 0.5f, gridPos.y + 0.5f};
    }
}

void DungeonTransitionSystem::reset()
{
    stairLatchActive = false;
    latchedFloorIndex = -1;
    latchedStairPos = {-1, -1};
}

void DungeonTransitionSystem::update(SceneContext& ctx,
                                     ECSWorld& world,
                                     DungeonManager& dungeon,
                                     DungeonVisibilityState& visibilityState,
                                     Entity playerEntity,
                                     const RebuildFloorFn& rebuildFloor,
                                     const MovePlayerFn& movePlayerToTile)
{
    handleStairTransitions(ctx,
                           world,
                           dungeon,
                           visibilityState,
                           playerEntity,
                           rebuildFloor,
                           movePlayerToTile);
}

void DungeonTransitionSystem::handleStairTransitions(SceneContext& ctx,
                                                     ECSWorld& world,
                                                     DungeonManager& dungeon,
                                                     DungeonVisibilityState& visibilityState,
                                                     Entity playerEntity,
                                                     const RebuildFloorFn& rebuildFloor,
                                                     const MovePlayerFn& movePlayerToTile)
{
    auto& transition = world.getSingleton<FloorTransitionStateComponent>();
    if (transition.active)
    {
        updateFloorTransition(ctx,
                              world,
                              dungeon,
                              visibilityState,
                              playerEntity,
                              rebuildFloor,
                              movePlayerToTile);
        return;
    }

    if (world.hasAny<DebugCameraStateComponent>()
        && world.getSingleton<DebugCameraStateComponent>().active)
        return;

    auto& player = world.getComponent<PlayerComponent>(playerEntity);
    if (player.inTransition)
        return;

    const glm::ivec2 playerGridPos = {player.gridX, player.gridZ};
    const GeneratedFloor& floor = dungeon.getActiveFloor();

    updateStairLatch(dungeon, playerGridPos);
    if (stairLatchActive
        && latchedFloorIndex == dungeon.getCurrentFloorIndex()
        && latchedStairPos == playerGridPos)
        return;

    const TileType tileType = floor.get(player.gridX, player.gridZ);
    if (tileType == TileType::StairDown && floor.hasStairDown())
        beginFloorTransition(world, dungeon, playerEntity, true, playerGridPos);
    else if (tileType == TileType::StairUp && floor.hasStairUp())
        beginFloorTransition(world, dungeon, playerEntity, false, playerGridPos);
}

void DungeonTransitionSystem::beginFloorTransition(ECSWorld& world,
                                                   DungeonManager& dungeon,
                                                   Entity playerEntity,
                                                   bool movingDown,
                                                   const glm::ivec2& sourceGridPos)
{
    const int sourceFloor = dungeon.getCurrentFloorIndex();
    const int targetFloor = movingDown ? sourceFloor + 1 : sourceFloor - 1;
    const GeneratedFloor* destinationFloor = dungeon.getFloor(targetFloor);
    if (destinationFloor == nullptr)
        return;

    glm::ivec2 arrival = destinationFloor->playerStart;
    if (movingDown && destinationFloor->hasStairUp())
        arrival = destinationFloor->stairUpPos;
    else if (!movingDown && destinationFloor->hasStairDown())
        arrival = destinationFloor->stairDownPos;

    auto& player = world.getComponent<PlayerComponent>(playerEntity);
    auto& camera = world.getComponent<CameraDescriptionComponent>(playerEntity);
    const glm::vec3 currentPos = world.getComponent<TransformComponent>(playerEntity).position;

    glm::vec3 forward = camera.target - currentPos;
    if (glm::dot(forward, forward) < 0.0001f)
        forward = {1.0f, 0.0f, 0.0f};
    else
        forward = glm::normalize(forward);

    const glm::vec3 sourceWorld = gridToWorld(sourceGridPos);
    const glm::vec3 arrivalWorld = gridToWorld(arrival);
    const float dropOffset = movingDown ? -STAIR_TRANSITION_DROP : STAIR_TRANSITION_DROP;
    const glm::vec3 midOffset = forward * STAIR_TRANSITION_FORWARD + glm::vec3(0.0f, dropOffset, 0.0f);

    auto& transition = world.getSingleton<FloorTransitionStateComponent>();
    transition.active = true;
    transition.type = FloorTransitionType::Stairs;
    transition.phase = movingDown ? FloorTransitionPhase::Descending : FloorTransitionPhase::Ascending;
    transition.progress = 0.0f;
    transition.targetFloor = targetFloor;
    transition.arrivalGrid = arrival;
    transition.floorSwapApplied = false;
    transition.camStart = currentPos;
    transition.camMid = sourceWorld + midOffset;
    transition.camEnd = arrivalWorld;
    transition.lookAtStart = currentPos + forward;
    transition.lookAtMid = sourceWorld + forward * 0.75f + glm::vec3(0.0f, dropOffset, 0.0f);
    transition.lookAtEnd = arrivalWorld + forward;

    player.inTransition = false;
}

void DungeonTransitionSystem::updateFloorTransition(SceneContext& ctx,
                                                    ECSWorld& world,
                                                    DungeonManager& dungeon,
                                                    DungeonVisibilityState& visibilityState,
                                                    Entity playerEntity,
                                                    const RebuildFloorFn& rebuildFloor,
                                                    const MovePlayerFn& movePlayerToTile)
{
    auto& transition = world.getSingleton<FloorTransitionStateComponent>();
    if (!transition.active)
        return;

    if (world.hasAny<DebugCameraStateComponent>())
    {
        auto& debugState = world.getSingleton<DebugCameraStateComponent>();
        if (debugState.active)
        {
            debugState.active = false;

            if (world.hasComponent<CameraDescriptionComponent>(debugState.debugCameraEntity))
                world.getComponent<CameraDescriptionComponent>(debugState.debugCameraEntity).active = false;

            if (world.hasComponent<CameraDescriptionComponent>(playerEntity))
                world.getComponent<CameraDescriptionComponent>(playerEntity).active = true;
        }
    }

    transition.progress += ctx.time->unscaledDeltaTime / transition.getDuration();
    transition.progress = glm::clamp(transition.progress, 0.0f, 1.0f);

    const bool inFirstSegment = transition.progress < 0.5f;
    const float segmentT = inFirstSegment
        ? transition.progress * 2.0f
        : (transition.progress - 0.5f) * 2.0f;
    const float ease = segmentT * segmentT * (3.0f - 2.0f * segmentT);

    if (!transition.floorSwapApplied && transition.progress >= 0.5f)
    {
        if (dungeon.moveToFloor(transition.targetFloor))
        {
            rebuildFloor();
            movePlayerToTile(transition.arrivalGrid);
            visibilityState.revealAt(dungeon.getCurrentFloorIndex(),
                                     dungeon.getActiveFloor(),
                                     transition.arrivalGrid.x,
                                     transition.arrivalGrid.y);
            stairLatchActive = true;
            latchedFloorIndex = dungeon.getCurrentFloorIndex();
            latchedStairPos = transition.arrivalGrid;
        }

        transition.floorSwapApplied = true;
        transition.phase = FloorTransitionPhase::Completing;
    }

    const glm::vec3 camPos = (!transition.floorSwapApplied)
        ? glm::mix(transition.camStart, transition.camMid, ease)
        : glm::mix(transition.camMid, transition.camEnd, ease);
    const glm::vec3 lookAt = (!transition.floorSwapApplied)
        ? glm::mix(transition.lookAtStart, transition.lookAtMid, ease)
        : glm::mix(transition.lookAtMid, transition.lookAtEnd, ease);

    auto& cameraTransform = world.getComponent<TransformComponent>(playerEntity);
    auto& cameraDesc = world.getComponent<CameraDescriptionComponent>(playerEntity);
    cameraTransform.position = camPos;
    cameraDesc.aspectRatio = ctx.windowAspectRatio;
    cameraDesc.target = lookAt;

    if (transition.progress >= 1.0f)
    {
        auto& player = world.getComponent<PlayerComponent>(playerEntity);
        const float yawRad = glm::radians(player.toYaw);
        const glm::vec3 playerWorld = gridToWorld({player.gridX, player.gridZ});

        cameraTransform.position = playerWorld;
        cameraDesc.target = playerWorld + glm::vec3(glm::sin(yawRad), 0.0f, -glm::cos(yawRad));

        transition.active = false;
        transition.phase = FloorTransitionPhase::Idle;
        transition.progress = 0.0f;
        transition.floorSwapApplied = false;
    }
}

void DungeonTransitionSystem::updateStairLatch(const DungeonManager& dungeon,
                                               const glm::ivec2& playerGridPos)
{
    if (!stairLatchActive)
        return;

    if (latchedFloorIndex != dungeon.getCurrentFloorIndex() || latchedStairPos != playerGridPos)
    {
        stairLatchActive = false;
        latchedFloorIndex = -1;
        latchedStairPos = {-1, -1};
    }
}
