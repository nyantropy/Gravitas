#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include <unordered_map>

#include "MeshResource.h"
#include "GtsModelLoader.hpp"
#include "vcsheet.h"
#include "BufferUtil.hpp"
#include "Types.h"

class MeshManager 
{
    private:
        std::unordered_map<std::string, mesh_id_type> pathToID;
        std::unordered_map<mesh_id_type, std::unique_ptr<MeshResource>> idToMesh;
        mesh_id_type nextID = 1; // start from 1, 0 = invalid

    public:
        MeshManager() = default;

        ~MeshManager()
        {
            for (auto& pair : idToMesh)
            {
                MeshResource* mesh = pair.second.get();

                if (mesh->vertexBuffer != VK_NULL_HANDLE)
                    vkDestroyBuffer(vcsheet::getDevice(), mesh->vertexBuffer, nullptr);

                if (mesh->vertexMemory != VK_NULL_HANDLE)
                    vkFreeMemory(vcsheet::getDevice(), mesh->vertexMemory, nullptr);

                if (mesh->indexBuffer != VK_NULL_HANDLE)
                    vkDestroyBuffer(vcsheet::getDevice(), mesh->indexBuffer, nullptr);

                if (mesh->indexMemory != VK_NULL_HANDLE)
                    vkFreeMemory(vcsheet::getDevice(), mesh->indexMemory, nullptr);
            }

            idToMesh.clear();
            pathToID.clear();
        }

        // load mesh if not loaded, return mesh ID
        mesh_id_type loadMesh(const std::string& path)
        {
            auto it = pathToID.find(path);
            if (it != pathToID.end())
                return it->second;

            auto mesh = std::make_unique<MeshResource>();

            GtsModelLoader::loadModel(path, mesh->vertices, mesh->indices);

            BufferUtil::createVertexBuffer(
                vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
                vcsheet::getCommandPool(), vcsheet::getGraphicsQueue(),
                mesh->vertices, mesh->vertexBuffer, mesh->vertexMemory
            );

            BufferUtil::createIndexBuffer(
                vcsheet::getDevice(), vcsheet::getCommandPool(),
                vcsheet::getGraphicsQueue(), vcsheet::getPhysicalDevice(),
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

        // Create or update a GPU mesh from CPU-generated vertex/index data.
        //
        // If existingId == 0: allocates a new MeshResource, uploads the geometry
        // via staging buffers, and returns the new ID.
        //
        // If existingId != 0: creates replacement GPU buffers first (the staging
        // copy internally calls vkQueueWaitIdle, which guarantees the graphics
        // queue — and therefore any in-flight frame referencing the old buffers —
        // has finished before we free them), then destroys the old buffers and
        // stores the new ones under the same ID.
        //
        // Callers must not pass an empty vertex array (Vulkan cannot create a
        // zero-byte buffer); guard against this before calling.
        mesh_id_type uploadProceduralMesh(mesh_id_type                 existingId,
                                          const std::vector<Vertex>&   vertices,
                                          const std::vector<uint32_t>& indices)
        {
            // BufferUtil takes non-const refs; copy into locals.
            std::vector<Vertex>   verts = vertices;
            std::vector<uint32_t> idxs  = indices;

            if (existingId != 0)
            {
                auto it = idToMesh.find(existingId);
                if (it == idToMesh.end())
                    existingId = 0; // fall through to create-new path
                else
                {
                    MeshResource& mesh = *it->second;

                    // Create new buffers first; vkQueueWaitIdle inside each
                    // createXxxBuffer call ensures the old buffers are no longer
                    // referenced by any in-flight command buffer before we free them.
                    VkBuffer     newVB; VkDeviceMemory newVM;
                    VkBuffer     newIB; VkDeviceMemory newIM;

                    BufferUtil::createVertexBuffer(
                        vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
                        vcsheet::getCommandPool(), vcsheet::getGraphicsQueue(),
                        verts, newVB, newVM);

                    BufferUtil::createIndexBuffer(
                        vcsheet::getDevice(), vcsheet::getCommandPool(),
                        vcsheet::getGraphicsQueue(), vcsheet::getPhysicalDevice(),
                        idxs, newIB, newIM);

                    // Queue is now idle — safe to destroy old buffers.
                    vkDestroyBuffer(vcsheet::getDevice(), mesh.vertexBuffer, nullptr);
                    vkFreeMemory   (vcsheet::getDevice(), mesh.vertexMemory, nullptr);
                    vkDestroyBuffer(vcsheet::getDevice(), mesh.indexBuffer,  nullptr);
                    vkFreeMemory   (vcsheet::getDevice(), mesh.indexMemory,  nullptr);

                    mesh.vertices     = vertices;
                    mesh.indices      = indices;
                    mesh.vertexBuffer = newVB;
                    mesh.vertexMemory = newVM;
                    mesh.indexBuffer  = newIB;
                    mesh.indexMemory  = newIM;

                    return existingId;
                }
            }

            // Create a brand-new mesh entry.
            auto mesh = std::make_unique<MeshResource>();
            mesh->vertices = vertices;
            mesh->indices  = indices;

            BufferUtil::createVertexBuffer(
                vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
                vcsheet::getCommandPool(), vcsheet::getGraphicsQueue(),
                verts, mesh->vertexBuffer, mesh->vertexMemory);

            BufferUtil::createIndexBuffer(
                vcsheet::getDevice(), vcsheet::getCommandPool(),
                vcsheet::getGraphicsQueue(), vcsheet::getPhysicalDevice(),
                idxs, mesh->indexBuffer, mesh->indexMemory);

            mesh_id_type id = nextID++;
            idToMesh[id] = std::move(mesh);
            return id;
        }
};
