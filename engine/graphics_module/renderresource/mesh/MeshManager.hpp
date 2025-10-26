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
    private:
        std::unordered_map<std::string, std::unique_ptr<MeshResource>> meshes;

    public:
        // the mesh manager does not need any external constructs to work
        MeshManager(){}

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

        // while we dont need any resources at instantiation, if this is called before the context is setup, the program will die
        // we could make this more robust, but i really dont mind it being like this right now
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
};