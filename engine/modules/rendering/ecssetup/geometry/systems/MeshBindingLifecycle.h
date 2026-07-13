#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>

#include "AssetTypes.h"
#include "BoundsComponent.h"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MaterialReferenceHelpers.h"
#include "MeshGpuComponent.h"
#include "QuadMeshComponent.h"
#include "StaticMeshComponent.h"

namespace gts::rendering
{
    struct DynamicMeshSyncResult
    {
        bool visited = false;
        bool skippedUnchanged = false;
        bool skippedFailedVersion = false;
        bool processed = false;
        bool invalid = false;
        bool uploaded = false;
        bool boundsRecomputed = false;
        bool allocated = false;
        bool reallocated = false;
        bool inPlaceUpdate = false;
        bool renderableInvalidated = false;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexBytes = 0;
        uint32_t indexBytes = 0;
        float versionCheckCpuMs = 0.0f;
        float validationCpuMs = 0.0f;
        float boundsCpuMs = 0.0f;
        float uploadCpuMs = 0.0f;
        float publicationCpuMs = 0.0f;
        float invalidationCpuMs = 0.0f;
        float cleanupCpuMs = 0.0f;
    };

    inline uint32_t growDynamicMeshCapacity(uint32_t currentCapacity, uint32_t requiredBytes)
    {
        if (requiredBytes == 0)
            return 0;

        if (currentCapacity >= requiredBytes)
            return currentCapacity;

        uint64_t capacity = currentCapacity == 0 ? 256u : currentCapacity;
        while (capacity < requiredBytes)
            capacity *= 2u;

        return capacity > std::numeric_limits<uint32_t>::max()
            ? requiredBytes
            : static_cast<uint32_t>(capacity);
    }

    inline bool validDynamicMeshGeometry(const DynamicMeshComponent& mesh)
    {
        if (mesh.vertices.empty() || mesh.indices.empty())
            return false;

        for (const Vertex& vertex : mesh.vertices)
        {
            if (!std::isfinite(vertex.pos.x) ||
                !std::isfinite(vertex.pos.y) ||
                !std::isfinite(vertex.pos.z))
            {
                return false;
            }
        }

        for (uint32_t index : mesh.indices)
        {
            if (index >= mesh.vertices.size())
                return false;
        }

        return true;
    }

    inline bool recomputeDynamicMeshBounds(ECSWorld& world,
                                           Entity entity,
                                           const DynamicMeshComponent& mesh)
    {
        if (!world.hasComponent<BoundsComponent>(entity) || mesh.vertices.empty())
            return false;

        glm::vec3 min = mesh.vertices.front().pos;
        glm::vec3 max = mesh.vertices.front().pos;
        for (const Vertex& vertex : mesh.vertices)
        {
            min = glm::min(min, vertex.pos);
            max = glm::max(max, vertex.pos);
        }

        BoundsComponent& bounds = world.getComponent<BoundsComponent>(entity);
        bounds.min = min;
        bounds.max = max;
        return true;
    }

    inline std::string resolveSubmeshMaterialPath(const std::string& meshPath,
                                                  const AssetReference& material)
    {
        if (material.logicalPath.empty())
            return {};

        std::filesystem::path materialPath(material.logicalPath);
        if (materialPath.is_absolute())
            return materialPath.string();

        const std::filesystem::path meshParent = std::filesystem::path(meshPath).parent_path();
        if (meshParent.empty())
            return materialPath.string();

        return (meshParent / materialPath).lexically_normal().string();
    }

    inline void resolveStaticMeshSubmeshMaterials(ECSWorld& world,
                                                  IResourceProvider* resources,
                                                  const std::string& meshPath,
                                                  MeshGpuComponent& meshGpu)
    {
        meshGpu.submeshMaterials.clear();
        if (resources == nullptr || meshGpu.meshID == 0)
            return;

        const std::vector<SubmeshAssetData>* submeshes = resources->getMeshSubmeshes(meshGpu.meshID);
        if (submeshes == nullptr || submeshes->empty())
            return;

        meshGpu.submeshMaterials.reserve(submeshes->size());
        for (const SubmeshAssetData& submesh : *submeshes)
        {
            const std::string materialPath = resolveSubmeshMaterialPath(meshPath, submesh.material);
            if (materialPath.empty())
            {
                meshGpu.submeshMaterials.push_back({});
                continue;
            }

            try
            {
                MaterialInstanceHandle handle =
                    getOrCreateSharedCookedMaterial(world, materialPath);
                materialRuntime(world).synchronizeGpuState(handle, resources);
                meshGpu.submeshMaterials.push_back(handle);
            }
            catch (const std::exception& error)
            {
                std::cerr
                    << "[assets] Could not resolve cooked submesh material; "
                    << "falling back to the entity material.\n"
                    << "  Mesh asset: " << meshPath << "\n"
                    << "  Material asset: " << materialPath << "\n"
                    << "  Error: " << error.what() << "\n";
                meshGpu.submeshMaterials.push_back({});
            }
        }
    }

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

        if (mesh.meshPath == meshGpu.boundMeshPath &&
            mesh.useMeshMaterials == meshGpu.boundUseMeshMaterials &&
            meshGpu.meshID != 0 &&
            !meshGpu.ownsProceduralMeshResource)
        {
            return;
        }

        releaseOwnedProceduralMesh(resources, meshGpu);
        meshGpu.meshID = resources->requestMesh(mesh.meshPath);
        if (mesh.useMeshMaterials)
            resolveStaticMeshSubmeshMaterials(world, resources, mesh.meshPath, meshGpu);
        else
            meshGpu.submeshMaterials.clear();
        meshGpu.ownsProceduralMeshResource = false;
        meshGpu.boundMeshPath = mesh.meshPath;
        meshGpu.boundUseMeshMaterials = mesh.useMeshMaterials;
        meshGpu.boundQuadWidth = 0.0f;
        meshGpu.boundQuadHeight = 0.0f;
        meshGpu.boundDynamicGeometryVersion = 0;
        meshGpu.attemptedDynamicGeometryVersion = 0;
        meshGpu.dynamicVertexCapacityBytes = 0;
        meshGpu.dynamicIndexCapacityBytes = 0;
        meshGpu.dynamicVertexUsedBytes = 0;
        meshGpu.dynamicIndexUsedBytes = 0;

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
        meshGpu.boundUseMeshMaterials = false;
        meshGpu.submeshMaterials.clear();
        meshGpu.boundQuadWidth = mesh.width;
        meshGpu.boundQuadHeight = mesh.height;
        meshGpu.boundDynamicGeometryVersion = 0;
        meshGpu.attemptedDynamicGeometryVersion = 0;
        meshGpu.dynamicVertexCapacityBytes = 0;
        meshGpu.dynamicIndexCapacityBytes = 0;
        meshGpu.dynamicVertexUsedBytes = 0;
        meshGpu.dynamicIndexUsedBytes = 0;

        writeMeshGpu(world, commands, entity, meshGpu);
        markMeshRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }

    inline DynamicMeshSyncResult syncDynamicMeshBinding(ECSWorld& world,
                                                        Entity entity,
                                                        IResourceProvider* resources,
                                                        ECSWorld::EntityCommandBuffer& commands)
    {
        DynamicMeshSyncResult result;
        if (resources == nullptr || !world.hasComponent<DynamicMeshComponent>(entity))
            return result;

        const DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        MeshGpuComponent meshGpu = world.hasComponent<MeshGpuComponent>(entity)
            ? world.getComponent<MeshGpuComponent>(entity)
            : MeshGpuComponent{};

        result.visited = true;
        result.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        result.indexCount = static_cast<uint32_t>(mesh.indices.size());
        result.vertexBytes = static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex));
        result.indexBytes = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t));

        const auto versionStart = std::chrono::steady_clock::now();
        const bool dynamicGeometryChanged =
            !meshGpu.ownsProceduralMeshResource
            || mesh.geometryVersion != meshGpu.boundDynamicGeometryVersion
            || meshGpu.meshID == 0;
        const bool failedVersionAlreadyAttempted =
            meshGpu.meshID == 0
            && meshGpu.attemptedDynamicGeometryVersion == mesh.geometryVersion
            && meshGpu.boundDynamicGeometryVersion != mesh.geometryVersion;
        result.versionCheckCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - versionStart).count();

        if (!dynamicGeometryChanged)
        {
            result.skippedUnchanged = true;
            return result;
        }

        if (failedVersionAlreadyAttempted)
        {
            result.skippedFailedVersion = true;
            return result;
        }

        const auto validationStart = std::chrono::steady_clock::now();
        const bool valid = validDynamicMeshGeometry(mesh);
        result.validationCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - validationStart).count();

        meshGpu.attemptedDynamicGeometryVersion = mesh.geometryVersion;
        if (!valid)
        {
            const auto cleanupStart = std::chrono::steady_clock::now();
            releaseOwnedProceduralMesh(resources, meshGpu);
            meshGpu.boundMeshPath = {};
            meshGpu.boundUseMeshMaterials = false;
            meshGpu.submeshMaterials.clear();
            meshGpu.boundQuadWidth = 0.0f;
            meshGpu.boundQuadHeight = 0.0f;
            meshGpu.boundDynamicGeometryVersion = 0;
            meshGpu.dynamicVertexCapacityBytes = 0;
            meshGpu.dynamicIndexCapacityBytes = 0;
            meshGpu.dynamicVertexUsedBytes = 0;
            meshGpu.dynamicIndexUsedBytes = 0;
            writeMeshGpu(world, commands, entity, meshGpu);
            result.cleanupCpuMs =
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - cleanupStart).count();

            const auto invalidationStart = std::chrono::steady_clock::now();
            markMeshRepresentationDirty(world, entity);
            queueRenderObjectRefresh(world, entity);
            result.invalidationCpuMs =
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - invalidationStart).count();
            result.invalid = true;
            result.renderableInvalidated = true;
            return result;
        }

        result.processed = true;
        const auto boundsStart = std::chrono::steady_clock::now();
        result.boundsRecomputed = recomputeDynamicMeshBounds(world, entity, mesh);
        result.boundsCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - boundsStart).count();

        result.allocated = meshGpu.meshID == 0 || !meshGpu.ownsProceduralMeshResource;
        result.reallocated = !result.allocated &&
            (result.vertexBytes > meshGpu.dynamicVertexCapacityBytes ||
             result.indexBytes > meshGpu.dynamicIndexCapacityBytes);
        result.inPlaceUpdate = !result.allocated && !result.reallocated;

        const mesh_id_type existingId = meshGpu.ownsProceduralMeshResource ? meshGpu.meshID : 0;
        const auto uploadStart = std::chrono::steady_clock::now();
        const mesh_id_type uploadedMeshId = resources->uploadProceduralMesh(
            existingId,
            mesh.vertices,
            mesh.indices,
            mesh.sourceAttributes);
        result.uploadCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - uploadStart).count();

        if (uploadedMeshId == 0)
        {
            result.allocated = false;
            result.reallocated = false;
            result.inPlaceUpdate = false;

            const auto cleanupStart = std::chrono::steady_clock::now();
            releaseOwnedProceduralMesh(resources, meshGpu);
            meshGpu.boundMeshPath = {};
            meshGpu.boundUseMeshMaterials = false;
            meshGpu.submeshMaterials.clear();
            meshGpu.boundQuadWidth = 0.0f;
            meshGpu.boundQuadHeight = 0.0f;
            meshGpu.boundDynamicGeometryVersion = 0;
            meshGpu.dynamicVertexCapacityBytes = 0;
            meshGpu.dynamicIndexCapacityBytes = 0;
            meshGpu.dynamicVertexUsedBytes = 0;
            meshGpu.dynamicIndexUsedBytes = 0;
            writeMeshGpu(world, commands, entity, meshGpu);
            result.cleanupCpuMs =
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - cleanupStart).count();

            const auto invalidationStart = std::chrono::steady_clock::now();
            markMeshRepresentationDirty(world, entity);
            queueRenderObjectRefresh(world, entity);
            result.invalidationCpuMs =
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - invalidationStart).count();
            result.renderableInvalidated = true;
            return result;
        }

        meshGpu.meshID = uploadedMeshId;
        meshGpu.ownsProceduralMeshResource = true;
        meshGpu.boundMeshPath = {};
        meshGpu.boundUseMeshMaterials = false;
        meshGpu.submeshMaterials.clear();
        meshGpu.boundQuadWidth = 0.0f;
        meshGpu.boundQuadHeight = 0.0f;
        meshGpu.boundDynamicGeometryVersion = mesh.geometryVersion;
        meshGpu.dynamicVertexUsedBytes = result.vertexBytes;
        meshGpu.dynamicIndexUsedBytes = result.indexBytes;
        meshGpu.dynamicVertexCapacityBytes =
            growDynamicMeshCapacity(meshGpu.dynamicVertexCapacityBytes, result.vertexBytes);
        meshGpu.dynamicIndexCapacityBytes =
            growDynamicMeshCapacity(meshGpu.dynamicIndexCapacityBytes, result.indexBytes);

        const auto publicationStart = std::chrono::steady_clock::now();
        writeMeshGpu(world, commands, entity, meshGpu);
        result.publicationCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - publicationStart).count();

        const auto invalidationStart = std::chrono::steady_clock::now();
        markMeshRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
        result.invalidationCpuMs =
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - invalidationStart).count();
        result.uploaded = meshGpu.meshID != 0;
        result.renderableInvalidated = true;
        return result;
    }
}
