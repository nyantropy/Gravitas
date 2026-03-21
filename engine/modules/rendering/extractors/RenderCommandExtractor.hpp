#pragma once

#include <vector>
#include <algorithm>
#include <limits>

#include "ECSWorld.hpp"
#include "RenderGpuComponent.h"
#include "CameraGpuComponent.h"
#include "BoundsComponent.h"
#include "CullFlagsComponent.h"
#include "FrustumCuller.h"
#include "RenderCommand.h"

// Reads only RenderGpuComponent (per-object render state) and the active
// CameraGpuComponent.  No Vulkan types; no pointers into ECS memory.
//
// Camera selection is driven by CameraGpuComponent::active, which is written
// by both CameraGpuSystem (default cameras) and custom override systems such
// as TetrisCameraSystem.  No dependency on CameraDescriptionComponent.
//
// Frustum culling is applied when frustumCullingEnabled is true (set at
// construction time from EngineConfig).  Entities without a BoundsComponent
// are never culled — this is a hard guarantee regardless of config.
//
// Frustum freeze: when frustumFrozen is true, the frustum planes are not
// updated from the live camera — the planes captured at freeze time are reused.
// The render camera continues to move normally; only the cull volume is locked.
class RenderCommandExtractor
{
public:
    explicit RenderCommandExtractor(bool frustumCullingEnabled = true)
        : frustumCullingEnabled(frustumCullingEnabled)
    {}

    std::vector<RenderCommand> extractRenderList(ECSWorld& world)
    {
        int totalRenderables   = 0;
        int visibleRenderables = 0;

        // find the active camera's backend state
        view_id_type cameraViewID  = 0;
        glm::mat4    viewMatrix    = glm::mat4(1.0f);
        glm::mat4    projMatrix    = glm::mat4(1.0f);

        for (Entity e : world.getAllEntitiesWith<CameraGpuComponent>())
        {
            auto& gpu = world.getComponent<CameraGpuComponent>(e);

            if (!gpu.active)
                continue;

            cameraViewID = gpu.viewID;
            viewMatrix   = gpu.viewMatrix;
            projMatrix   = gpu.projMatrix;
            break;
        }

        // Extract frustum planes from the live camera each frame unless frozen.
        // When frozen, culler retains the planes captured at freeze time.
        if (frustumCullingEnabled && !frustumFrozen)
        {
            glm::mat4 viewProj = projMatrix * viewMatrix;
            culler.extractPlanes(viewProj);
        }

        // one command per renderable entity that has a bound SSBO slot
        std::vector<RenderCommand> cmds;

        for (Entity e : world.getAllEntitiesWith<RenderGpuComponent>())
        {
            auto& rc = world.getComponent<RenderGpuComponent>(e);

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                continue; // slot not yet assigned by RenderBindingSystem — skip this frame

            if (!rc.readyToRender)
                continue; // model matrix not yet computed by RenderGpuSystem — skip this frame

            ++totalRenderables;

            // --- Frustum cull test ---
            if (frustumCullingEnabled)
            {
                // Per-entity neverCull / cullEnabled overrides.
                bool skipCull = false;
                if (world.hasComponent<CullFlagsComponent>(e))
                {
                    const auto& flags = world.getComponent<CullFlagsComponent>(e);
                    if (flags.neverCull || !flags.cullEnabled)
                        skipCull = true;
                }

                if (!skipCull)
                {
                    if (world.hasComponent<BoundsComponent>(e))
                    {
                        const auto& bounds = world.getComponent<BoundsComponent>(e);

                        // Transform all 8 AABB corners to world space using the model
                        // matrix and recompute the world-space min/max from those corners.
                        // DO NOT simply multiply min and max directly — this gives wrong
                        // results for rotated or non-uniformly scaled entities.
                        const glm::mat4& model = rc.modelMatrix;

                        glm::vec3 corners[8] = {
                            { bounds.min.x, bounds.min.y, bounds.min.z },
                            { bounds.max.x, bounds.min.y, bounds.min.z },
                            { bounds.min.x, bounds.max.y, bounds.min.z },
                            { bounds.max.x, bounds.max.y, bounds.min.z },
                            { bounds.min.x, bounds.min.y, bounds.max.z },
                            { bounds.max.x, bounds.min.y, bounds.max.z },
                            { bounds.min.x, bounds.max.y, bounds.max.z },
                            { bounds.max.x, bounds.max.y, bounds.max.z }
                        };

                        glm::vec3 worldMin{ std::numeric_limits<float>::max() };
                        glm::vec3 worldMax{ std::numeric_limits<float>::lowest() };

                        for (const glm::vec3& corner : corners)
                        {
                            glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
                            worldMin = glm::min(worldMin, worldCorner);
                            worldMax = glm::max(worldMax, worldCorner);
                        }

                        if (!culler.isVisible(worldMin, worldMax))
                            continue;  // culled — skip this entity
                    }
                    // No BoundsComponent → never cull (safe default).
                }
            }
            // --- End cull test ---

            ++visibleRenderables;

            cmds.push_back({
                rc.meshID,
                rc.textureID,
                rc.objectSSBOSlot,
                cameraViewID,
                rc.modelMatrix,
                viewMatrix,
                projMatrix,
                rc.alpha
            });
        }

        // Opaque commands first so transparent geometry blends correctly against rendered geometry
        std::stable_partition(cmds.begin(), cmds.end(),
            [](const RenderCommand& cmd) { return cmd.alpha >= 1.0f; });

        lastTotalRenderables   = totalRenderables;
        lastVisibleRenderables = visibleRenderables;

        return cmds;
    }

    // Frustum culling global toggle
    void setFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
    bool isFrustumCullingEnabled() const { return frustumCullingEnabled; }

    // Freeze the frustum at its current position.  The cull volume stops updating
    // while the render camera continues to move normally.
    void toggleFrustumFreeze() { frustumFrozen = !frustumFrozen; }
    bool isFrustumFrozen()  const { return frustumFrozen; }

    int  getLastTotalRenderables()   const { return lastTotalRenderables; }
    int  getLastVisibleRenderables() const { return lastVisibleRenderables; }

private:
    bool          frustumCullingEnabled;
    bool          frustumFrozen        = false;
    FrustumCuller culler;               // persists between frames; updated unless frozen
    int           lastTotalRenderables   = 0;
    int           lastVisibleRenderables = 0;
};
