#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include <unordered_map>

#include "TextureResource.h"
#include "dssheet.h"

// same principle as the mesh manager, just a simple resource management class
class TextureManager
{
    public:
        TextureResource& loadTexture(const std::string& path)
        {
            auto it = textures.find(path);
            if (it != textures.end())
                return *it->second;

            auto resource = std::make_unique<TextureResource>();
            resource->texture = std::make_unique<VulkanTexture>(path);

            // let the descriptor set manager allocate descriptor sets directly in the resource
            resource->descriptorSets = dssheet::getManager()
            .allocateForTexture(resource->texture->getTextureImageView(), resource->texture->getTextureSampler());

            TextureResource& ref = *resource;
            textures[path] = std::move(resource);
            return ref;
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<TextureResource>> textures;
};
