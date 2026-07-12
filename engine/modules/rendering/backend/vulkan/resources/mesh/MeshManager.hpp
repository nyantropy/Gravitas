#pragma once
#include <vulkan/vulkan.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstring>

#include "MeshResource.h"
#include "GtsModelLoader.hpp"
#include "IResourceProvider.hpp"
#include "VulkanBackendContext.h"
#include "BufferUtil.hpp"
#include "Types.h"

class MeshManager 
{
    private:
        VulkanBackendContext& backendContext;
        std::unordered_map<std::string, mesh_id_type> pathToID;
        std::unordered_map<mesh_id_type, std::unique_ptr<MeshResource>> idToMesh;
        std::unordered_set<mesh_id_type> proceduralMeshIDs;
        // Shared quad meshes: keyed by packed float bits (width << 32 | height).
        // These are never in proceduralMeshIDs, so destroyProceduralMesh won't free them.
        std::unordered_map<uint64_t, mesh_id_type> quadMeshCache;
        mesh_id_type nextID = 1; // start from 1, 0 = invalid
        ProceduralMeshUploadMetrics proceduralMetrics;
        uint32_t proceduralUpdateBatchDepth = 0;
        bool proceduralUpdateSafetyWaitPerformed = false;

        static VkDeviceSize grownCapacity(VkDeviceSize currentCapacity, VkDeviceSize requiredBytes)
        {
            if (requiredBytes == 0)
                return 0;
            if (currentCapacity >= requiredBytes)
                return currentCapacity;

            VkDeviceSize capacity = currentCapacity == 0 ? 256 : currentCapacity;
            while (capacity < requiredBytes)
                capacity *= 2;
            return capacity;
        }

        void ensureProceduralUpdateSafe(bool existingResource)
        {
            if (!existingResource)
                return;

            if (proceduralUpdateBatchDepth > 0)
            {
                if (!proceduralUpdateSafetyWaitPerformed)
                {
                    vkQueueWaitIdle(backendContext.graphicsQueue());
                    proceduralUpdateSafetyWaitPerformed = true;
                }
                return;
            }

            vkQueueWaitIdle(backendContext.graphicsQueue());
        }

        void destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory)
        {
            if (buffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(backendContext.device(), buffer, nullptr);
                buffer = VK_NULL_HANDLE;
            }
            if (memory != VK_NULL_HANDLE)
            {
                vkFreeMemory(backendContext.device(), memory, nullptr);
                memory = VK_NULL_HANDLE;
            }
        }

        void allocateHostVisibleBuffer(VkDeviceSize size,
                                       VkBufferUsageFlags usage,
                                       VkBuffer& buffer,
                                       VkDeviceMemory& memory)
        {
            BufferUtil::createBuffer(
                backendContext.device(),
                backendContext.physicalDevice(),
                size,
                usage,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                buffer,
                memory);
        }

        void writeHostVisibleBuffer(VkDeviceMemory memory, const void* source, VkDeviceSize bytes)
        {
            if (bytes == 0 || source == nullptr || memory == VK_NULL_HANDLE)
                return;

            void* data = nullptr;
            vkMapMemory(backendContext.device(), memory, 0, bytes, 0, &data);
            std::memcpy(data, source, static_cast<size_t>(bytes));
            vkUnmapMemory(backendContext.device(), memory);
        }

        void ensureProceduralVertexCapacity(MeshResource& mesh, VkDeviceSize requiredBytes)
        {
            if (requiredBytes <= mesh.vertexCapacityBytes && mesh.vertexBuffer != VK_NULL_HANDLE)
                return;

            const auto start = std::chrono::steady_clock::now();
            destroyBuffer(mesh.vertexBuffer, mesh.vertexMemory);
            const VkDeviceSize capacity = grownCapacity(mesh.vertexCapacityBytes, requiredBytes);
            allocateHostVisibleBuffer(
                capacity,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                mesh.vertexBuffer,
                mesh.vertexMemory);
            mesh.vertexCapacityBytes = capacity;
            proceduralMetrics.vertexBytesAllocated += static_cast<uint64_t>(capacity);
            proceduralMetrics.resourceAllocationCpuMs +=
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
        }

        void ensureProceduralIndexCapacity(MeshResource& mesh, VkDeviceSize requiredBytes)
        {
            if (requiredBytes <= mesh.indexCapacityBytes && mesh.indexBuffer != VK_NULL_HANDLE)
                return;

            const auto start = std::chrono::steady_clock::now();
            destroyBuffer(mesh.indexBuffer, mesh.indexMemory);
            const VkDeviceSize capacity = grownCapacity(mesh.indexCapacityBytes, requiredBytes);
            allocateHostVisibleBuffer(
                capacity,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                mesh.indexBuffer,
                mesh.indexMemory);
            mesh.indexCapacityBytes = capacity;
            proceduralMetrics.indexBytesAllocated += static_cast<uint64_t>(capacity);
            proceduralMetrics.resourceAllocationCpuMs +=
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
        }

    public:
        explicit MeshManager(VulkanBackendContext& backendContext) : backendContext(backendContext) {}

        ~MeshManager()
        {
            for (auto& pair : idToMesh)
            {
                MeshResource* mesh = pair.second.get();

                if (mesh->vertexBuffer != VK_NULL_HANDLE)
                    vkDestroyBuffer(backendContext.device(), mesh->vertexBuffer, nullptr);

                if (mesh->vertexMemory != VK_NULL_HANDLE)
                    vkFreeMemory(backendContext.device(), mesh->vertexMemory, nullptr);

                if (mesh->indexBuffer != VK_NULL_HANDLE)
                    vkDestroyBuffer(backendContext.device(), mesh->indexBuffer, nullptr);

                if (mesh->indexMemory != VK_NULL_HANDLE)
                    vkFreeMemory(backendContext.device(), mesh->indexMemory, nullptr);
            }

            idToMesh.clear();
            pathToID.clear();
            proceduralMeshIDs.clear();
        }

        // load mesh if not loaded, return mesh ID
        mesh_id_type loadMesh(const std::string& path)
        {
            auto it = pathToID.find(path);
            if (it != pathToID.end())
                return it->second;

            auto mesh = std::make_unique<MeshResource>();

            mesh->metadata = GtsModelLoader::loadModel(path, mesh->vertices, mesh->indices);

            BufferUtil::createVertexBuffer(
                backendContext.device(), backendContext.physicalDevice(),
                backendContext.commandPool(), backendContext.graphicsQueue(),
                mesh->vertices, mesh->vertexBuffer, mesh->vertexMemory
            );

            BufferUtil::createIndexBuffer(
                backendContext.device(), backendContext.commandPool(),
                backendContext.graphicsQueue(), backendContext.physicalDevice(),
                mesh->indices, mesh->indexBuffer, mesh->indexMemory
            );

            mesh_id_type id = nextID++;
            idToMesh[id] = std::move(mesh);
            pathToID[path] = id;

            return id;
        }

        // resolve ID to MeshResource
        MeshResource* getMesh(mesh_id_type id)
        {
            auto it = idToMesh.find(id);
            if (it != idToMesh.end())
                return it->second.get();
            return nullptr; // invalid ID
        }

        void beginProceduralMeshUpdateBatch(uint32_t)
        {
            proceduralUpdateBatchDepth += 1;
            proceduralUpdateSafetyWaitPerformed = false;
        }

        void endProceduralMeshUpdateBatch()
        {
            if (proceduralUpdateBatchDepth > 0)
                proceduralUpdateBatchDepth -= 1;
            if (proceduralUpdateBatchDepth == 0)
                proceduralUpdateSafetyWaitPerformed = false;
        }

        ProceduralMeshUploadMetrics takeProceduralMeshUploadMetrics()
        {
            ProceduralMeshUploadMetrics result = proceduralMetrics;
            proceduralMetrics = {};
            return result;
        }

        // Returns a shared mesh ID for an axis-aligned quad of the given dimensions.
        // Same (width, height) → same mesh ID, so all identically-sized quads share
        // one vertex/index buffer and can be batched via instanced drawing.
        // The returned ID is NOT in proceduralMeshIDs; it lives until the MeshManager
        // is destroyed and must NOT be passed to destroyProceduralMesh.
        mesh_id_type getOrCreateQuadMesh(float w, float h)
        {
            uint32_t wBits, hBits;
            std::memcpy(&wBits, &w, sizeof(float));
            std::memcpy(&hBits, &h, sizeof(float));
            const uint64_t key = (static_cast<uint64_t>(wBits) << 32) | hBits;

            auto it = quadMeshCache.find(key);
            if (it != quadMeshCache.end())
                return it->second;

            const float hw = w * 0.5f;
            const float hh = h * 0.5f;
            const glm::vec3 normal = {0.0f, 0.0f, 1.0f};
            const glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
            const glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
            std::vector<Vertex> verts = {
                { { -hw,  hh, 0.0f }, normal, tangent, color, { 0.0f, 0.0f } },
                { {  hw,  hh, 0.0f }, normal, tangent, color, { 1.0f, 0.0f } },
                { {  hw, -hh, 0.0f }, normal, tangent, color, { 1.0f, 1.0f } },
                { { -hw, -hh, 0.0f }, normal, tangent, color, { 0.0f, 1.0f } },
            };
            std::vector<uint32_t> idxs  = { 0, 1, 2, 0, 2, 3 };
            std::vector<Vertex>   vertsC = verts;
            std::vector<uint32_t> idxsC  = idxs;

            auto mesh = std::make_unique<MeshResource>();
            mesh->vertices = verts;
            mesh->indices  = idxs;
            mesh->metadata = gts::rendering::standardGeneratedMeshMetadata(verts.size(), idxs.size());

            BufferUtil::createVertexBuffer(
                backendContext.device(), backendContext.physicalDevice(),
                backendContext.commandPool(), backendContext.graphicsQueue(),
                vertsC, mesh->vertexBuffer, mesh->vertexMemory);

            BufferUtil::createIndexBuffer(
                backendContext.device(), backendContext.commandPool(),
                backendContext.graphicsQueue(), backendContext.physicalDevice(),
                idxsC, mesh->indexBuffer, mesh->indexMemory);

            mesh_id_type id = nextID++;
            idToMesh[id] = std::move(mesh);
            quadMeshCache[key] = id;
            return id;
        }

        // Create or update a GPU mesh from CPU-generated vertex/index data.
        // Procedural meshes keep stable mesh IDs and host-visible GPU buffers.
        // Existing buffers are reused when the new logical data fits capacity;
        // capacity grows independently for vertices and indices.
        mesh_id_type uploadProceduralMesh(mesh_id_type                 existingId,
                                          const std::vector<Vertex>&   vertices,
                                          const std::vector<uint32_t>& indices,
                                          VertexAttributeFlags sourceAttributes =
                                              UnlitVertexAttributes)
        {
            proceduralMetrics.uploadCalls += 1;

            MeshResource* mesh = nullptr;
            bool createNew = existingId == 0;
            if (existingId != 0)
            {
                auto it = idToMesh.find(existingId);
                if (it == idToMesh.end())
                    createNew = true;
                else
                    mesh = it->second.get();
            }

            mesh_id_type id = existingId;
            if (createNew)
            {
                auto ownedMesh = std::make_unique<MeshResource>();
                mesh = ownedMesh.get();
                id = nextID++;
                idToMesh[id] = std::move(ownedMesh);
                proceduralMeshIDs.insert(id);
                proceduralMetrics.gpuAllocations += 1;
            }
            else
            {
                ensureProceduralUpdateSafe(true);
            }

            const auto copyStart = std::chrono::steady_clock::now();
            mesh->vertices = vertices;
            mesh->indices = indices;
            proceduralMetrics.temporaryCopyCpuMs +=
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - copyStart).count();
            proceduralMetrics.cpuBytesCopied +=
                static_cast<uint64_t>(vertices.size() * sizeof(Vertex))
                + static_cast<uint64_t>(indices.size() * sizeof(uint32_t));

            const auto prepareStart = std::chrono::steady_clock::now();
            mesh->metadata = gts::rendering::prepareMeshGeometry(
                mesh->vertices,
                mesh->indices,
                sourceAttributes);
            proceduralMetrics.geometryPreparationCpuMs +=
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - prepareStart).count();
            proceduralMetrics.generatedNormals += mesh->metadata.generatedNormals ? 1u : 0u;
            proceduralMetrics.generatedTangents += mesh->metadata.generatedTangents ? 1u : 0u;
            proceduralMetrics.verticesProcessed += mesh->vertices.size();
            proceduralMetrics.indicesProcessed += mesh->indices.size();

            const VkDeviceSize vertexBytes =
                static_cast<VkDeviceSize>(mesh->vertices.size() * sizeof(Vertex));
            const VkDeviceSize indexBytes =
                static_cast<VkDeviceSize>(mesh->indices.size() * sizeof(uint32_t));
            const bool vertexFits = mesh->vertexBuffer != VK_NULL_HANDLE &&
                vertexBytes <= mesh->vertexCapacityBytes;
            const bool indexFits = mesh->indexBuffer != VK_NULL_HANDLE &&
                indexBytes <= mesh->indexCapacityBytes;
            const bool reallocated = !createNew && (!vertexFits || !indexFits);

            ensureProceduralVertexCapacity(*mesh, vertexBytes);
            ensureProceduralIndexCapacity(*mesh, indexBytes);
            mesh->hostVisibleProcedural = true;

            const auto vertexUploadStart = std::chrono::steady_clock::now();
            writeHostVisibleBuffer(mesh->vertexMemory, mesh->vertices.data(), vertexBytes);
            proceduralMetrics.vertexUploadCpuMs +=
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - vertexUploadStart).count();

            const auto indexUploadStart = std::chrono::steady_clock::now();
            writeHostVisibleBuffer(mesh->indexMemory, mesh->indices.data(), indexBytes);
            proceduralMetrics.indexUploadCpuMs +=
                std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - indexUploadStart).count();

            proceduralMetrics.vertexBytesUploaded += static_cast<uint64_t>(vertexBytes);
            proceduralMetrics.indexBytesUploaded += static_cast<uint64_t>(indexBytes);
            if (reallocated)
                proceduralMetrics.gpuReallocations += 1;
            else if (!createNew)
                proceduralMetrics.inPlaceUpdates += 1;
            return id;
        }

        void destroyProceduralMesh(mesh_id_type id)
        {
            if (id == 0 || !proceduralMeshIDs.contains(id))
                return;

            auto it = idToMesh.find(id);
            if (it == idToMesh.end())
            {
                proceduralMeshIDs.erase(id);
                return;
            }

            MeshResource* mesh = it->second.get();

            // Dungeon floor rebuilds can remove many procedural renderables
            // while the previous frame is still submitted. Match the update
            // path's lifetime guarantee before freeing buffers referenced by
            // recorded draw commands.
            vkQueueWaitIdle(backendContext.graphicsQueue());

            if (mesh->vertexBuffer != VK_NULL_HANDLE)
                vkDestroyBuffer(backendContext.device(), mesh->vertexBuffer, nullptr);
            if (mesh->vertexMemory != VK_NULL_HANDLE)
                vkFreeMemory(backendContext.device(), mesh->vertexMemory, nullptr);
            if (mesh->indexBuffer != VK_NULL_HANDLE)
                vkDestroyBuffer(backendContext.device(), mesh->indexBuffer, nullptr);
            if (mesh->indexMemory != VK_NULL_HANDLE)
                vkFreeMemory(backendContext.device(), mesh->indexMemory, nullptr);

            idToMesh.erase(it);
            proceduralMeshIDs.erase(id);
        }
};
