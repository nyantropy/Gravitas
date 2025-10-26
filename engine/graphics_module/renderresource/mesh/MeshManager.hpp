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
};
