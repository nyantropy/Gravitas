#pragma once

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

namespace gts::rendering
{
    inline void markRenderableDirty(RenderDirtyComponent& dirty, RenderGpuComponent& renderGpu)
    {
        renderGpu.dirty         = true;
        renderGpu.readyToRender = false;
        renderGpu.commandDirty  = true;
        dirty.transformDirty    = true;
    }

    inline void ensureRenderableGpuState(ECSWorld& world,
                                         Entity entity,
                                         IResourceProvider* resources)
    {
        if (!world.hasComponent<MeshGpuComponent>(entity))
            world.addComponent(entity, MeshGpuComponent{});
        if (!world.hasComponent<MaterialGpuComponent>(entity))
            world.addComponent(entity, MaterialGpuComponent{});
        if (!world.hasComponent<RenderGpuComponent>(entity))
            world.addComponent(entity, RenderGpuComponent{});
        if (!world.hasComponent<RenderDirtyComponent>(entity))
            world.addComponent(entity, RenderDirtyComponent{});

        auto& renderGpu = world.getComponent<RenderGpuComponent>(entity);
        if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
        {
            renderGpu.objectSSBOSlot = resources->requestObjectSlot();
            renderGpu.commandDirty   = true;
        }

        if (world.hasComponent<TransformComponent>(entity))
            gts::transform::markDirty(world, entity);
    }

    inline void cleanupRenderableBinding(ECSWorld& world,
                                         Entity entity,
                                         IResourceProvider*)
    {
        if (world.hasComponent<MeshGpuComponent>(entity))
            world.removeComponent<MeshGpuComponent>(entity);

        if (world.hasComponent<MaterialGpuComponent>(entity))
            world.removeComponent<MaterialGpuComponent>(entity);
        if (world.hasComponent<RenderDirtyComponent>(entity))
            world.removeComponent<RenderDirtyComponent>(entity);
        if (world.hasComponent<RenderGpuComponent>(entity))
            world.removeComponent<RenderGpuComponent>(entity);
    }

    inline void syncStaticMeshBinding(ECSWorld& world,
                                      Entity entity,
                                      IResourceProvider* resources)
    {
        if (!world.hasComponent<StaticMeshComponent>(entity) || !world.hasComponent<MaterialComponent>(entity))
            return;

        ensureRenderableGpuState(world, entity, resources);

        const auto& mesh = world.getComponent<StaticMeshComponent>(entity);
        const auto& mat  = world.getComponent<MaterialComponent>(entity);
        auto& meshGpu    = world.getComponent<MeshGpuComponent>(entity);
        auto& matGpu     = world.getComponent<MaterialGpuComponent>(entity);
        auto& dirty      = world.getComponent<RenderDirtyComponent>(entity);
        auto& renderGpu  = world.getComponent<RenderGpuComponent>(entity);

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
            dirty.materialDirty = true;
            renderGpu.commandDirty = true;
        }

        matGpu.tint        = mat.tint;
        matGpu.alpha       = mat.alpha;
        matGpu.doubleSided = mat.doubleSided;
    }

    inline void syncProceduralMeshBinding(ECSWorld& world,
                                          Entity entity,
                                          IResourceProvider* resources)
    {
        if (!world.hasComponent<ProceduralMeshComponent>(entity) || !world.hasComponent<MaterialComponent>(entity))
            return;

        const auto& mat = world.getComponent<MaterialComponent>(entity);
        if (mat.texturePath.empty())
        {
            cleanupRenderableBinding(world, entity, resources);
            return;
        }

        ensureRenderableGpuState(world, entity, resources);

        const auto& mesh = world.getComponent<ProceduralMeshComponent>(entity);
        auto& meshGpu    = world.getComponent<MeshGpuComponent>(entity);
        auto& matGpu     = world.getComponent<MaterialGpuComponent>(entity);
        auto& dirty      = world.getComponent<RenderDirtyComponent>(entity);
        auto& renderGpu  = world.getComponent<RenderGpuComponent>(entity);

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
    }
}
