#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include <unordered_map>

#include "TextureResource.h"
#include "VulkanTexture.hpp"
#include "dssheet.h"
#include "Types.h"

// manages texture loading, caching, and ID-based access, just like the mesh manager
class TextureManager
{
    private:
        std::unordered_map<std::string, texture_id_type> pathToID;
        std::unordered_map<texture_id_type, std::unique_ptr<TextureResource>> idToTexture;
        texture_id_type nextID = 1; // 0 = invalid

    public:
        TextureManager() = default;

        // Loads a texture if not already loaded; returns its unique ID.
        texture_id_type loadTexture(const std::string& path)
        {
            auto it = pathToID.find(path);
            if (it != pathToID.end())
                return it->second;

            auto resource = std::make_unique<TextureResource>();
            resource->texture = std::make_unique<VulkanTexture>(path);

            // Allocate descriptor sets via the descriptor set manager
            resource->descriptorSets = dssheet::getManager()
                .allocateForTexture(resource->texture->getTextureImageView(),
                                    resource->texture->getTextureSampler());

            texture_id_type id = nextID++;
            idToTexture[id] = std::move(resource);
            pathToID[path] = id;

            return id;
        }

        // Resolves an ID to an actual TextureResource (used by renderer).
        TextureResource* getTexture(texture_id_type id)
        {
            auto it = idToTexture.find(id);
            if (it != idToTexture.end())
                return it->second.get();
            return nullptr;
        }
};
