#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "ECSSimulationSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraOverrideComponent.h"
#include "TransformComponent.h"

// Simulation system — pure CPU math, no GPU resources, no SceneContext.
// Mirrors RenderGpuSystem for the default camera pipeline.
//
// Each frame, for every entity with CameraDescriptionComponent:
//   - Skips the entity if CameraOverrideComponent is present (owned by a custom
//     camera system that already wrote its matrices this frame)
//   - Ensures CameraGpuComponent exists (creates with viewID=0 if absent)
//   - Reads TransformComponent.position (falls back to origin if absent)
//   - Computes view matrix via glm::lookAt
//   - Computes projection matrix via glm::perspective with Vulkan Y-flip
//   - Syncs the active flag from CameraDescriptionComponent
//   - Writes matrices into CameraGpuComponent and sets dirty = true
//
// Does not allocate GPU resources or descriptor sets — that is
// CameraBindingSystem's exclusive responsibility.
class CameraGpuSystem : public ECSSimulationSystem
{
public:
    void update(ECSWorld& world, float) override
    {
        world.forEach<CameraDescriptionComponent>([&](Entity e, CameraDescriptionComponent& desc)
        {
            // custom camera systems own this entity — do not overwrite their matrices
            if (world.hasComponent<CameraOverrideComponent>(e))
                return;

            if (!world.hasComponent<CameraGpuComponent>(e))
                world.addComponent(e, CameraGpuComponent{});

            auto& gpu = world.getComponent<CameraGpuComponent>(e);

            glm::vec3 position = world.hasComponent<TransformComponent>(e)
                ? world.getComponent<TransformComponent>(e).position
                : glm::vec3(0.0f);

            gpu.viewMatrix = glm::lookAt(position, desc.target, desc.up);

            gpu.projMatrix = glm::perspective(desc.fov, desc.aspectRatio, desc.nearClip, desc.farClip);
            gpu.projMatrix[1][1] *= -1; // Vulkan Y-flip

            gpu.active = desc.active;
            gpu.dirty  = true;
        });
    }
};
