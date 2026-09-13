#pragma once
#include "ECSWorld.hpp"
#include "SkinnedModelComponent.h"
#include "WorldTransformComponent.h"

// Snapshot palette ownership and resolved placement together. Animated bounds/culling
// are deferred; static bind bounds must not discard an animated limb or occurrence.
inline SkinnedFrameData extractSkinnedFrame(ECSWorld& world, view_id_type camera)
{
    SkinnedFrameData result;
    result.cameraViewID = camera;
    world.forEach<SkinnedModelComponent, WorldTransformComponent>(
        [&](Entity, const SkinnedModelComponent& mesh, const WorldTransformComponent& transform)
        {
            result.draws.push_back({mesh.instance, mesh.palettes, transform.matrix * mesh.actorFromReference});
        });
    return result;
}
