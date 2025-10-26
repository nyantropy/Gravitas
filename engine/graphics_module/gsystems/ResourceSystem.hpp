#pragma once
#include <string>

#include "MeshManager.hpp"
#include "TextureManager.hpp"
#include "UniformBufferManager.hpp"
#include "Types.h"

class ResourceSystem
{
    private:
        // all of our resource managers
        std::unique_ptr<DescriptorSetManager> descriptorSetManager;
        std::unique_ptr<MeshManager> meshManager;
        std::unique_ptr<UniformBufferManager> uniformBufferManager;
        std::unique_ptr<TextureManager> textureManager;

    public:
        ResourceSystem()
        {
            descriptorSetManager = std::make_unique<DescriptorSetManager>(GraphicsConstants::MAX_FRAMES_IN_FLIGHT, 1000);
            dssheet::SetManager(descriptorSetManager.get());

            meshManager = std::make_unique<MeshManager>();
            uniformBufferManager = std::make_unique<UniformBufferManager>();
            textureManager = std::make_unique<TextureManager>();
        }

        ~ResourceSystem()
        {
            if(descriptorSetManager) descriptorSetManager.reset();
            if(meshManager) meshManager.reset();
            if(uniformBufferManager) uniformBufferManager.reset();
            if(textureManager) textureManager.reset();
        }

        // Mesh Management
        mesh_id_type requestMesh(const std::string& path)
        {
            return meshManager->loadMesh(path); // lazy load
        }

        MeshResource* getMesh(mesh_id_type id)
        {
            return meshManager->getMesh(id);
        }

        // Texture Management
        texture_id_type requestTexture(const std::string& path)
        {
            return textureManager->loadTexture(path);
        }

        TextureResource* getTexture(texture_id_type id)
        {
            return textureManager->getTexture(id);
        }

        // UniformBuffer Management
        uniform_id_type requestUniformBuffer()
        {
            return uniformBufferManager->createUniformBuffer();
        }

        UniformBufferResource* getUniformBuffer(uniform_id_type id)
        {
            return uniformBufferManager->getUniformBuffer(id);
        }
};
