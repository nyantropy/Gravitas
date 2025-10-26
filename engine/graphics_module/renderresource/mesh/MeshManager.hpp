#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

#include "MeshResource.h"
#include "GtsModelLoader.hpp"

#include "vcsheet.h"

#include "BufferUtil.hpp"

// manages meshes, ensuring they are only loaded once
class MeshManager 
{
    public:
        MeshResource& loadMesh(const std::string& path) 
        {
            if (meshes.find(path) != meshes.end()) 
            {
                return *meshes[path];
            }

            auto mesh = std::make_unique<MeshResource>();

            GtsModelLoader::loadModel(path, mesh->vertices, mesh->indices);

            BufferUtil::createVertexBuffer(vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
            vcsheet::getCommandPool(), vcsheet::getGraphicsQueue(),
            mesh->vertices, mesh->vertexBuffer, mesh->vertexMemory);

            BufferUtil::createIndexBuffer(vcsheet::getDevice(), vcsheet::getCommandPool(),
            vcsheet::getGraphicsQueue(), vcsheet::getPhysicalDevice(),
            mesh->indices, mesh->indexBuffer, mesh->indexMemory);

            MeshResource& ref = *mesh;
            meshes[path] = std::move(mesh);
            return ref;
        }

        MeshManager()
        {           
        }

        ~MeshManager()
        {
            for (auto& pair : meshes)
            {
                MeshResource* mesh = pair.second.get();

                if (mesh->vertexBuffer != VK_NULL_HANDLE)
                {
                    vkDestroyBuffer(vcsheet::getDevice(), mesh->vertexBuffer, nullptr);
                }

                if (mesh->vertexMemory != VK_NULL_HANDLE)
                {
                    vkFreeMemory(vcsheet::getDevice(), mesh->vertexMemory, nullptr);
                }

                if (mesh->indexBuffer != VK_NULL_HANDLE)
                {
                    vkDestroyBuffer(vcsheet::getDevice(), mesh->indexBuffer, nullptr);
                }

                if (mesh->indexMemory != VK_NULL_HANDLE)
                {
                    vkFreeMemory(vcsheet::getDevice(), mesh->indexMemory, nullptr);
                }
            }

            meshes.clear();
        }       

    private:
        std::unordered_map<std::string, std::unique_ptr<MeshResource>> meshes;
};