#pragma once

#include "GlmConfig.h"

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"
#include "TransformComponent.h"

// Controller system — pure CPU math, no GPU resources.
// Runs every frame regardless of pause state so camera matrices stay current.
// Mirrors RenderGpuSystem for the default camera pipeline.
//
// Each frame, for every entity with CameraDescriptionComponent + CameraGpuComponent:
//   - Skips the entity if CameraOverrideComponent is present (owned by a custom
//     camera system that already wrote its matrices this frame)
//   - Reads TransformComponent.position (falls back to origin if absent)
//   - Computes view matrix via glm::lookAt
//   - Computes projection matrix via glm::perspective with Vulkan Y-flip
//   - Syncs the active flag from CameraDescriptionComponent
//   - Writes matrices into CameraGpuComponent and sets dirty = true
//
// Does not allocate GPU resources or descriptor sets — that is
// CameraBindingSystem's exclusive responsibility.
class CameraGpuSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        ctx.world.forEach<CameraDescriptionComponent, CameraGpuComponent>(
            [&](Entity e, CameraDescriptionComponent& desc, CameraGpuComponent& gpu)
        {
            // custom camera systems own this entity — do not overwrite their matrices
            if (ctx.world.hasComponent<CameraOverrideComponent>(e))
                return;

            glm::vec3 position = ctx.world.hasComponent<TransformComponent>(e)
                ? ctx.world.getComponent<TransformComponent>(e).position
                : glm::vec3(0.0f);

            gpu.viewMatrix = glm::lookAt(position, desc.target, desc.up);

            gpu.projMatrix = glm::perspective(desc.fov, desc.aspectRatio, desc.nearClip, desc.farClip);
            gpu.projMatrix[1][1] *= -1; // Vulkan Y-flip

            gpu.active = desc.active;
            gpu.dirty  = true;
        });
    }
};
