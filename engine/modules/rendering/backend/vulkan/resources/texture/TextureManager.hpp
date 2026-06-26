#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include <unordered_map>

#include "DescriptorSetManager.hpp"
#include "TextureResource.h"
#include "VulkanTexture.hpp"
#include "Types.h"
#include "VulkanBackendContext.h"

// manages texture loading, caching, and ID-based access, just like the mesh manager
class TextureManager
{
    private:
        VulkanBackendContext& backendContext;
        DescriptorSetManager& descriptorSetManager;
        std::unordered_map<std::string, texture_id_type> pathToID;
        std::unordered_map<texture_id_type, std::unique_ptr<TextureResource>> idToTexture;
        texture_id_type nextID = 1; // 0 = invalid

    public:
        TextureManager(VulkanBackendContext& backendContext, DescriptorSetManager& descriptorSetManager)
            : backendContext(backendContext)
            , descriptorSetManager(descriptorSetManager)
        {
        }

        // Loads a texture if not already loaded; returns its unique ID.
        // nearestFilter = true requests NEAREST sampling (no anisotropy, no mip blending).
        // clampToEdge = true preserves filtering but clamps UVs for UI/atlas edges.
        // Sampler variants of the same path are cached independently.
        texture_id_type loadTexture(const std::string& path,
                                    bool nearestFilter = false,
                                    bool clampToEdge = false)
        {
            const bool effectiveClampToEdge = nearestFilter || clampToEdge;
            const std::string key = path
                + (nearestFilter ? ":nearest" : ":linear")
                + (effectiveClampToEdge ? ":clamp" : ":repeat");

            auto it = pathToID.find(key);
            if (it != pathToID.end())
                return it->second;

            auto resource = std::make_unique<TextureResource>();
            resource->texture = std::make_unique<VulkanTexture>(backendContext, path, nearestFilter, clampToEdge);

            resource->descriptorSets = descriptorSetManager.allocateForTexture(
                resource->texture->getTextureImageView(),
                resource->texture->getTextureSampler());

            texture_id_type id = nextID++;
            idToTexture[id] = std::move(resource);
            pathToID[key] = id;

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

        int getTextureWidth(texture_id_type id) const
        {
            auto it = idToTexture.find(id);
            if (it == idToTexture.end() || !it->second || !it->second->texture)
                return 0;
            return it->second->texture->getWidth();
        }

        int getTextureHeight(texture_id_type id) const
        {
            auto it = idToTexture.find(id);
            if (it == idToTexture.end() || !it->second || !it->second->texture)
                return 0;
            return it->second->texture->getHeight();
        }
};
