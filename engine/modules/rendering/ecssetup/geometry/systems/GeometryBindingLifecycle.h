#pragma once

#include <unordered_map>
#include <unordered_set>

#include "ECSWorld.hpp"
#include "IResourceProvider.hpp"
#include "MaterialComponent.h"
#include "MaterialGpuComponent.h"
#include "MeshGpuComponent.h"
#include "ProceduralMeshComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "WorldTextComponent.h"

namespace gts::rendering
{
    struct GeometryBindingLifecycleState
    {
        std::unordered_set<entity_id_type> staticMeshRefreshEntities;
        std::unordered_set<entity_id_type> proceduralRefreshEntities;
        std::unordered_set<entity_id_type> cleanupEntities;
    };

    inline auto& geometryBindingLifecycleRegistry()
    {
        static std::unordered_map<ECSWorld*, GeometryBindingLifecycleState> registry;
        return registry;
    }

    inline GeometryBindingLifecycleState& geometryBindingLifecycleState(ECSWorld& world)
    {
        return geometryBindingLifecycleRegistry()[&world];
    }

    inline void resetGeometryBindingLifecycleState(ECSWorld& world)
    {
        geometryBindingLifecycleRegistry().erase(&world);
    }

    inline std::unordered_set<entity_id_type> takeStaticMeshRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.staticMeshRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeProceduralRefreshes(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.proceduralRefreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeRenderableCleanupEntities(ECSWorld& world)
    {
        auto& state = geometryBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.cleanupEntities);
        return pending;
    }

    inline void queueStaticMeshRefresh(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).staticMeshRefreshEntities.insert(entity.id);
    }

    inline void queueProceduralRefresh(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).proceduralRefreshEntities.insert(entity.id);
    }

    inline void queueRenderableCleanup(ECSWorld& world, Entity entity)
    {
        geometryBindingLifecycleState(world).cleanupEntities.insert(entity.id);
    }

    inline void markRenderableDirty(RenderDirtyComponent& dirty, RenderGpuComponent& renderGpu)
    {
        renderGpu.dirty         = true;
        renderGpu.readyToRender = false;
        renderGpu.commandDirty  = true;
        dirty.transformDirty    = true;
    }

    inline void scheduleRenderableCleanup(ECSWorld& world,
                                          ECSWorld::EntityCommandBuffer& commands,
                                          Entity entity)
    {
        if (world.hasComponent<MeshGpuComponent>(entity))
            commands.removeComponent<MeshGpuComponent>(entity);
        if (world.hasComponent<MaterialGpuComponent>(entity))
            commands.removeComponent<MaterialGpuComponent>(entity);
        if (world.hasComponent<RenderDirtyComponent>(entity))
            commands.removeComponent<RenderDirtyComponent>(entity);
        if (world.hasComponent<RenderGpuComponent>(entity))
            commands.removeComponent<RenderGpuComponent>(entity);
    }

    inline bool hasRenderableDescriptor(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<StaticMeshComponent>(entity)
            && world.hasComponent<MaterialComponent>(entity))
        {
            return true;
        }

        if (world.hasComponent<ProceduralMeshComponent>(entity)
            && world.hasComponent<MaterialComponent>(entity))
        {
            const auto& material = world.getComponent<MaterialComponent>(entity);
            if (!material.texturePath.empty())
                return true;
        }

        if (world.hasComponent<WorldTextComponent>(entity)
            && world.hasComponent<TransformComponent>(entity))
        {
            const auto& text = world.getComponent<WorldTextComponent>(entity);
            if (text.font != nullptr && !text.text.empty())
                return true;
        }

        return false;
    }

    inline void syncStaticMeshBinding(ECSWorld& world,
                                      Entity entity,
                                      IResourceProvider* resources,
                                      ECSWorld::EntityCommandBuffer& commands)
    {
        if (!world.hasComponent<StaticMeshComponent>(entity) || !world.hasComponent<MaterialComponent>(entity))
            return;

        const auto& mesh = world.getComponent<StaticMeshComponent>(entity);
        const auto& mat  = world.getComponent<MaterialComponent>(entity);
        const bool hasMeshGpu = world.hasComponent<MeshGpuComponent>(entity);
        const bool hasMatGpu = world.hasComponent<MaterialGpuComponent>(entity);
        const bool hasRenderGpu = world.hasComponent<RenderGpuComponent>(entity);
        const bool hasDirty = world.hasComponent<RenderDirtyComponent>(entity);

        MeshGpuComponent meshGpu = hasMeshGpu ? world.getComponent<MeshGpuComponent>(entity) : MeshGpuComponent{};
        MaterialGpuComponent matGpu = hasMatGpu ? world.getComponent<MaterialGpuComponent>(entity) : MaterialGpuComponent{};
        RenderGpuComponent renderGpu = hasRenderGpu ? world.getComponent<RenderGpuComponent>(entity) : RenderGpuComponent{};
        RenderDirtyComponent dirty = hasDirty ? world.getComponent<RenderDirtyComponent>(entity) : RenderDirtyComponent{};

        if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
        {
            renderGpu.objectSSBOSlot = resources->requestObjectSlot();
            renderGpu.commandDirty   = true;
        }

        if (mesh.meshPath != meshGpu.boundMeshPath || meshGpu.meshID == 0 || meshGpu.ownsProceduralMeshResource)
        {
            if (meshGpu.ownsProceduralMeshResource && meshGpu.meshID != 0)
                resources->releaseProceduralMesh(meshGpu.meshID);

            meshGpu.meshID                     = resources->requestMesh(mesh.meshPath);
            meshGpu.ownsProceduralMeshResource = false;
            meshGpu.boundMeshPath              = mesh.meshPath;
            meshGpu.boundWidth                 = 0.0f;
            meshGpu.boundHeight                = 0.0f;
            meshGpu.boundGeometryVersion       = 0;
            dirty.meshDirty                    = true;
            markRenderableDirty(dirty, renderGpu);
        }

        if (mat.texturePath != matGpu.boundTexturePath || matGpu.textureID == 0)
        {
            matGpu.textureID        = resources->requestTexture(mat.texturePath);
            matGpu.boundTexturePath = mat.texturePath;
            dirty.materialDirty     = true;
            markRenderableDirty(dirty, renderGpu);
        }

        if (matGpu.tint != mat.tint
            || matGpu.alpha != mat.alpha
            || matGpu.doubleSided != mat.doubleSided)
        {
            dirty.materialDirty   = true;
            renderGpu.commandDirty = true;
        }

        matGpu.tint        = mat.tint;
        matGpu.alpha       = mat.alpha;
        matGpu.doubleSided = mat.doubleSided;

        if (hasMeshGpu)
            world.getComponent<MeshGpuComponent>(entity) = meshGpu;
        else
            commands.addComponent<MeshGpuComponent>(entity, meshGpu);

        if (hasMatGpu)
            world.getComponent<MaterialGpuComponent>(entity) = matGpu;
        else
            commands.addComponent<MaterialGpuComponent>(entity, matGpu);

        if (hasRenderGpu)
            world.getComponent<RenderGpuComponent>(entity) = renderGpu;
        else
            commands.addComponent<RenderGpuComponent>(entity, renderGpu);

        if (hasDirty)
            world.getComponent<RenderDirtyComponent>(entity) = dirty;
        else
            commands.addComponent<RenderDirtyComponent>(entity, dirty);

        if (world.hasComponent<TransformComponent>(entity))
            gts::transform::markDirty(world, entity);
    }

    inline void syncProceduralMeshBinding(ECSWorld& world,
                                          Entity entity,
                                          IResourceProvider* resources,
                                          ECSWorld::EntityCommandBuffer& commands)
    {
        if (!world.hasComponent<ProceduralMeshComponent>(entity) || !world.hasComponent<MaterialComponent>(entity))
            return;

        if (world.getComponent<MaterialComponent>(entity).texturePath.empty())
        {
            scheduleRenderableCleanup(world, commands, entity);
            return;
        }

        const auto& mesh = world.getComponent<ProceduralMeshComponent>(entity);
        const auto& mat  = world.getComponent<MaterialComponent>(entity);
        const bool hasMeshGpu = world.hasComponent<MeshGpuComponent>(entity);
        const bool hasMatGpu = world.hasComponent<MaterialGpuComponent>(entity);
        const bool hasRenderGpu = world.hasComponent<RenderGpuComponent>(entity);
        const bool hasDirty = world.hasComponent<RenderDirtyComponent>(entity);

        MeshGpuComponent meshGpu = hasMeshGpu ? world.getComponent<MeshGpuComponent>(entity) : MeshGpuComponent{};
        MaterialGpuComponent matGpu = hasMatGpu ? world.getComponent<MaterialGpuComponent>(entity) : MaterialGpuComponent{};
        RenderGpuComponent renderGpu = hasRenderGpu ? world.getComponent<RenderGpuComponent>(entity) : RenderGpuComponent{};
        RenderDirtyComponent dirty = hasDirty ? world.getComponent<RenderDirtyComponent>(entity) : RenderDirtyComponent{};

        if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
        {
            renderGpu.objectSSBOSlot = resources->requestObjectSlot();
            renderGpu.commandDirty   = true;
        }

        if (mat.texturePath != matGpu.boundTexturePath || matGpu.textureID == 0)
        {
            matGpu.textureID        = resources->requestTexture(mat.texturePath);
            matGpu.boundTexturePath = mat.texturePath;
            dirty.materialDirty     = true;
            markRenderableDirty(dirty, renderGpu);
        }

        if (matGpu.tint != mat.tint || matGpu.alpha != mat.alpha || !matGpu.doubleSided)
        {
            dirty.materialDirty   = true;
            renderGpu.commandDirty = true;
        }

        matGpu.tint        = mat.tint;
        matGpu.alpha       = mat.alpha;
        matGpu.doubleSided = true;

        const bool customGeometryChanged =
            mesh.useCustomGeometry
            && (!meshGpu.ownsProceduralMeshResource
                || mesh.geometryVersion != meshGpu.boundGeometryVersion
                || meshGpu.meshID == 0);

        const bool quadGeometryChanged =
            !mesh.useCustomGeometry
            && (meshGpu.ownsProceduralMeshResource
                || mesh.width != meshGpu.boundWidth
                || mesh.height != meshGpu.boundHeight
                || meshGpu.meshID == 0);

        if (customGeometryChanged || quadGeometryChanged)
        {
            if (mesh.useCustomGeometry)
            {
                const mesh_id_type existingId = meshGpu.ownsProceduralMeshResource ? meshGpu.meshID : 0;
                meshGpu.meshID = resources->uploadProceduralMesh(existingId, mesh.vertices, mesh.indices);
                meshGpu.ownsProceduralMeshResource = true;
            }
            else
            {
                if (meshGpu.ownsProceduralMeshResource && meshGpu.meshID != 0)
                    resources->releaseProceduralMesh(meshGpu.meshID);

                meshGpu.meshID = resources->getSharedQuadMesh(mesh.width, mesh.height);
                meshGpu.ownsProceduralMeshResource = false;
            }

            meshGpu.boundMeshPath        = {};
            meshGpu.boundWidth           = mesh.width;
            meshGpu.boundHeight          = mesh.height;
            meshGpu.boundGeometryVersion = mesh.geometryVersion;
            dirty.meshDirty              = true;
            markRenderableDirty(dirty, renderGpu);
        }

        if (hasMeshGpu)
            world.getComponent<MeshGpuComponent>(entity) = meshGpu;
        else
            commands.addComponent<MeshGpuComponent>(entity, meshGpu);

        if (hasMatGpu)
            world.getComponent<MaterialGpuComponent>(entity) = matGpu;
        else
            commands.addComponent<MaterialGpuComponent>(entity, matGpu);

        if (hasRenderGpu)
            world.getComponent<RenderGpuComponent>(entity) = renderGpu;
        else
            commands.addComponent<RenderGpuComponent>(entity, renderGpu);

        if (hasDirty)
            world.getComponent<RenderDirtyComponent>(entity) = dirty;
        else
            commands.addComponent<RenderDirtyComponent>(entity, dirty);

        if (world.hasComponent<TransformComponent>(entity))
            gts::transform::markDirty(world, entity);
    }
}
