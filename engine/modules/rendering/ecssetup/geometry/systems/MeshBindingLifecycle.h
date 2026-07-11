#pragma once

#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MeshGpuComponent.h"
#include "QuadMeshComponent.h"
#include "StaticMeshComponent.h"

namespace gts::rendering
{
    inline void releaseOwnedProceduralMesh(IResourceProvider* resources, MeshGpuComponent& meshGpu)
    {
        if (resources == nullptr || !meshGpu.ownsProceduralMeshResource || meshGpu.meshID == 0)
            return;

        resources->releaseProceduralMesh(meshGpu.meshID);
        meshGpu.meshID = 0;
        meshGpu.ownsProceduralMeshResource = false;
    }

    inline void writeMeshGpu(ECSWorld& world,
                             ECSWorld::EntityCommandBuffer& commands,
                             Entity entity,
                             const MeshGpuComponent& meshGpu)
    {
        if (world.hasComponent<MeshGpuComponent>(entity))
            world.getComponent<MeshGpuComponent>(entity) = meshGpu;
        else
            commands.addComponent<MeshGpuComponent>(entity, meshGpu);
    }

    inline void syncStaticMeshBinding(ECSWorld& world,
                                      Entity entity,
                                      IResourceProvider* resources,
                                      ECSWorld::EntityCommandBuffer& commands)
    {
        if (resources == nullptr || !world.hasComponent<StaticMeshComponent>(entity))
            return;

        const StaticMeshComponent& mesh = world.getComponent<StaticMeshComponent>(entity);
        MeshGpuComponent meshGpu = world.hasComponent<MeshGpuComponent>(entity)
            ? world.getComponent<MeshGpuComponent>(entity)
            : MeshGpuComponent{};

        if (mesh.meshPath == meshGpu.boundMeshPath && meshGpu.meshID != 0 &&
            !meshGpu.ownsProceduralMeshResource)
        {
            return;
        }

        releaseOwnedProceduralMesh(resources, meshGpu);
        meshGpu.meshID = resources->requestMesh(mesh.meshPath);
        meshGpu.ownsProceduralMeshResource = false;
        meshGpu.boundMeshPath = mesh.meshPath;
        meshGpu.boundQuadWidth = 0.0f;
        meshGpu.boundQuadHeight = 0.0f;
        meshGpu.boundDynamicGeometryVersion = 0;

        writeMeshGpu(world, commands, entity, meshGpu);
        markMeshRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }

    inline void syncQuadMeshBinding(ECSWorld& world,
                                    Entity entity,
                                    IResourceProvider* resources,
                                    ECSWorld::EntityCommandBuffer& commands)
    {
        if (resources == nullptr || !world.hasComponent<QuadMeshComponent>(entity))
            return;

        const QuadMeshComponent& mesh = world.getComponent<QuadMeshComponent>(entity);
        MeshGpuComponent meshGpu = world.hasComponent<MeshGpuComponent>(entity)
            ? world.getComponent<MeshGpuComponent>(entity)
            : MeshGpuComponent{};

        const bool quadGeometryChanged =
            meshGpu.ownsProceduralMeshResource
            || mesh.width != meshGpu.boundQuadWidth
            || mesh.height != meshGpu.boundQuadHeight
            || meshGpu.meshID == 0;
        if (!quadGeometryChanged)
            return;

        releaseOwnedProceduralMesh(resources, meshGpu);
        meshGpu.meshID = resources->getSharedQuadMesh(mesh.width, mesh.height);
        meshGpu.ownsProceduralMeshResource = false;
        meshGpu.boundMeshPath = {};
        meshGpu.boundQuadWidth = mesh.width;
        meshGpu.boundQuadHeight = mesh.height;
        meshGpu.boundDynamicGeometryVersion = 0;

        writeMeshGpu(world, commands, entity, meshGpu);
        markMeshRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }

    inline void syncDynamicMeshBinding(ECSWorld& world,
                                       Entity entity,
                                       IResourceProvider* resources,
                                       ECSWorld::EntityCommandBuffer& commands)
    {
        if (resources == nullptr || !world.hasComponent<DynamicMeshComponent>(entity))
            return;

        const DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        if (mesh.vertices.empty() || mesh.indices.empty())
        {
            scheduleMeshGpuCleanup(world, commands, entity);
            queueRenderObjectRefresh(world, entity);
            return;
        }

        MeshGpuComponent meshGpu = world.hasComponent<MeshGpuComponent>(entity)
            ? world.getComponent<MeshGpuComponent>(entity)
            : MeshGpuComponent{};
        const bool dynamicGeometryChanged =
            !meshGpu.ownsProceduralMeshResource
            || mesh.geometryVersion != meshGpu.boundDynamicGeometryVersion
            || meshGpu.meshID == 0;
        if (!dynamicGeometryChanged)
            return;

        const mesh_id_type existingId = meshGpu.ownsProceduralMeshResource ? meshGpu.meshID : 0;
        meshGpu.meshID = resources->uploadProceduralMesh(
            existingId,
            mesh.vertices,
            mesh.indices,
            mesh.sourceAttributes);
        meshGpu.ownsProceduralMeshResource = true;
        meshGpu.boundMeshPath = {};
        meshGpu.boundQuadWidth = 0.0f;
        meshGpu.boundQuadHeight = 0.0f;
        meshGpu.boundDynamicGeometryVersion = mesh.geometryVersion;

        writeMeshGpu(world, commands, entity, meshGpu);
        markMeshRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }
}
