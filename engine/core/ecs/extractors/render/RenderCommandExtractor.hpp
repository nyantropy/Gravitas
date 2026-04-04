#pragma once

#include <chrono>
#include <vector>
#include <algorithm>
#include <limits>
#include <unordered_map>

#include "ECSWorld.hpp"
#include "IGtsExtractor.h"
#include "RenderGpuComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "CameraGpuComponent.h"
#include "BoundsComponent.h"
#include "CullFlagsComponent.h"
#include "FrustumCuller.h"
#include "RenderCommand.h"

// Reads RenderGpuComponent (transform / slot), MeshGpuComponent (mesh ID),
// and MaterialGpuComponent (texture, alpha, doubleSided) per entity, plus the
// active CameraGpuComponent.  No Vulkan types; no pointers into ECS memory.
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
class RenderCommandExtractor : public IGtsExtractor<std::vector<RenderCommand>>
{
public:
    struct Metrics
    {
        float    extractCpuMs       = 0.0f;
        float    sortCpuMs          = 0.0f;
        uint32_t visitedRenderables = 0;
        uint32_t totalRenderables   = 0;
        uint32_t visibleRenderables = 0;
        uint32_t updatedCommands    = 0;
        uint32_t cachedCommands     = 0;
        bool     sortedThisFrame    = false;
    };

    explicit RenderCommandExtractor(bool frustumCullingEnabled = true)
        : frustumCullingEnabled(frustumCullingEnabled)
    {}

    Metrics getLastMetrics() const
    {
        return lastMetrics;
    }

    const std::vector<RenderCommand>& extract(const GtsExtractorContext& ctx) override
    {
        const auto start = std::chrono::steady_clock::now();
        ECSWorld& world = ctx.world;

        frameStamp += 1;
        int totalRenderables    = 0;
        int visibleRenderables  = 0;
        int visitedRenderables  = 0;
        int updatedCommands     = 0;

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

        const bool cameraChanged = !cacheInitialised
            || cameraViewID != lastCameraViewID
            || viewMatrix != lastViewMatrix
            || projMatrix != lastProjMatrix;

        // Extract frustum planes from the live camera each frame unless frozen.
        // When frozen, culler retains the planes captured at freeze time.
        if (frustumCullingEnabled && !frustumFrozen && cameraChanged)
        {
            glm::mat4 viewProj = projMatrix * viewMatrix;
            culler.extractPlanes(viewProj);
        }

        opaqueEntityOrder.clear();
        transparentEntityOrder.clear();
        opaqueEntityOrder.reserve(commandCache.size());
        transparentEntityOrder.reserve(commandCache.size());

        world.forEach<RenderGpuComponent, MeshGpuComponent, MaterialGpuComponent>(
            [&](Entity e, RenderGpuComponent& rc, MeshGpuComponent& meshGpu, MaterialGpuComponent& matGpu)
        {
            visitedRenderables += 1;

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                markInvisible(e.id);
                return;
            }

            if (!rc.readyToRender)
            {
                markInvisible(e.id);
                return;
            }

            ++totalRenderables;

            CachedCommandState& cached = commandCache[e.id];
            cached.lastSeenFrame = frameStamp;

            const bool opaque = matGpu.alpha >= 1.0f;
            const bool needsUpdate = !cacheInitialised
                || rc.commandDirty
                || cameraChanged
                || !cached.initialised
                || cached.command.meshID != meshGpu.meshID
                || cached.command.textureID != matGpu.textureID
                || cached.command.objectSSBOSlot != rc.objectSSBOSlot
                || cached.command.alpha != matGpu.alpha
                || cached.command.doubleSided != matGpu.doubleSided;

            if (needsUpdate)
            {
                cached.command.meshID         = meshGpu.meshID;
                cached.command.textureID      = matGpu.textureID;
                cached.command.objectSSBOSlot = rc.objectSSBOSlot;
                cached.command.cameraViewID   = cameraViewID;
                cached.command.modelMatrix    = rc.modelMatrix;
                cached.command.viewMatrix     = viewMatrix;
                cached.command.projMatrix     = projMatrix;
                cached.command.alpha          = matGpu.alpha;
                cached.command.doubleSided    = matGpu.doubleSided;
                cached.visible                = isEntityVisible(world, e, rc);
                cached.initialised            = true;
                rc.commandDirty               = false;
                updatedCommands += 1;
            }

            if (!cached.visible)
                return;

            ++visibleRenderables;
            if (opaque)
                opaqueEntityOrder.push_back(e.id);
            else
                transparentEntityOrder.push_back(e.id);
        });

        pruneStaleCache();

        const bool needsRebuild = !cacheInitialised || cameraChanged || updatedCommands > 0 || staleEntriesPruned;
        lastMetrics.sortCpuMs = 0.0f;
        lastMetrics.sortedThisFrame = false;

        if (needsRebuild)
        {
            const auto sortStart = std::chrono::steady_clock::now();
            std::sort(opaqueEntityOrder.begin(), opaqueEntityOrder.end(),
                [&](entity_id_type lhs, entity_id_type rhs)
                {
                    const auto& a = commandCache[lhs].command;
                    const auto& b = commandCache[rhs].command;
                    if (a.doubleSided != b.doubleSided) return !a.doubleSided && b.doubleSided;
                    if (a.meshID      != b.meshID)      return a.meshID < b.meshID;
                    return a.textureID < b.textureID;
                });

            cachedVisibleCommands.clear();
            cachedVisibleCommands.reserve(opaqueEntityOrder.size() + transparentEntityOrder.size());
            for (entity_id_type id : opaqueEntityOrder)
                cachedVisibleCommands.push_back(commandCache[id].command);
            for (entity_id_type id : transparentEntityOrder)
                cachedVisibleCommands.push_back(commandCache[id].command);

            const auto sortEnd = std::chrono::steady_clock::now();
            lastMetrics.sortCpuMs =
                std::chrono::duration<float, std::milli>(sortEnd - sortStart).count();
            lastMetrics.sortedThisFrame = !opaqueEntityOrder.empty();
        }

        lastTotalRenderables   = totalRenderables;
        lastVisibleRenderables = visibleRenderables;
        const auto end = std::chrono::steady_clock::now();
        lastCameraViewID = cameraViewID;
        lastViewMatrix   = viewMatrix;
        lastProjMatrix   = projMatrix;
        cacheInitialised = true;
        staleEntriesPruned = false;

        lastMetrics.extractCpuMs = std::chrono::duration<float, std::milli>(end - start).count();
        lastMetrics.visitedRenderables = static_cast<uint32_t>(visitedRenderables);
        lastMetrics.totalRenderables = static_cast<uint32_t>(totalRenderables);
        lastMetrics.visibleRenderables = static_cast<uint32_t>(visibleRenderables);
        lastMetrics.updatedCommands = static_cast<uint32_t>(updatedCommands);
        lastMetrics.cachedCommands  = static_cast<uint32_t>(cachedVisibleCommands.size());

        return cachedVisibleCommands;
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
    struct CachedCommandState
    {
        RenderCommand command;
        uint64_t      lastSeenFrame = 0;
        bool          visible       = false;
        bool          initialised   = false;
    };

    bool          frustumCullingEnabled;
    bool          frustumFrozen        = false;
    FrustumCuller culler;               // persists between frames; updated unless frozen
    int           lastTotalRenderables   = 0;
    int           lastVisibleRenderables = 0;
    Metrics       lastMetrics;
    std::unordered_map<entity_id_type, CachedCommandState> commandCache;
    std::vector<entity_id_type> opaqueEntityOrder;
    std::vector<entity_id_type> transparentEntityOrder;
    std::vector<RenderCommand>  cachedVisibleCommands;
    glm::mat4    lastViewMatrix = glm::mat4(1.0f);
    glm::mat4    lastProjMatrix = glm::mat4(1.0f);
    view_id_type lastCameraViewID = 0;
    uint64_t     frameStamp = 0;
    bool         cacheInitialised = false;
    bool         staleEntriesPruned = false;

    void markInvisible(entity_id_type id)
    {
        auto it = commandCache.find(id);
        if (it != commandCache.end())
        {
            it->second.lastSeenFrame = frameStamp;
            it->second.visible = false;
        }
    }

    void pruneStaleCache()
    {
        for (auto it = commandCache.begin(); it != commandCache.end(); )
        {
            if (it->second.lastSeenFrame != frameStamp)
            {
                staleEntriesPruned = true;
                it = commandCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    bool isEntityVisible(ECSWorld& world, Entity e, const RenderGpuComponent& rc)
    {
        if (!frustumCullingEnabled)
            return true;

        bool skipCull = false;
        if (world.hasComponent<CullFlagsComponent>(e))
        {
            const auto& flags = world.getComponent<CullFlagsComponent>(e);
            if (flags.neverCull || !flags.cullEnabled)
                skipCull = true;
        }

        if (skipCull || !world.hasComponent<BoundsComponent>(e))
            return true;

        const auto& bounds = world.getComponent<BoundsComponent>(e);
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

        return culler.isVisible(worldMin, worldMax);
    }
};
