#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

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
                                    bool clampToEdge = false,
                                    TextureColorSpace colorSpace = TextureColorSpace::SRgb)
        {
            const bool effectiveClampToEdge = nearestFilter || clampToEdge;
            const std::string key = path
                + (nearestFilter ? ":nearest" : ":linear")
                + (effectiveClampToEdge ? ":clamp" : ":repeat")
                + (colorSpace == TextureColorSpace::SRgb ? ":srgb" : ":linear-data");

            auto it = pathToID.find(key);
            if (it != pathToID.end())
                return it->second;

            auto resource = std::make_unique<TextureResource>();
            resource->texture =
                std::make_unique<VulkanTexture>(backendContext, path, nearestFilter, clampToEdge, colorSpace);
            resource->width = resource->texture->getWidth();
            resource->height = resource->texture->getHeight();
            resource->colorSpace = colorSpace;

            resource->descriptorSets = descriptorSetManager.allocateForTexture(
                resource->texture->getTextureImageView(),
                resource->texture->getTextureSampler());

            texture_id_type id = nextID++;
            resource->id = id;
            resource->cacheKey = key;
            idToTexture[id] = std::move(resource);
            pathToID[key] = id;

            return id;
        }

        texture_id_type registerSampledImage(const std::string& key,
                                             VkImageView imageView,
                                             VkSampler sampler,
                                             VkImageLayout imageLayout,
                                             int width,
                                             int height)
        {
            auto existing = pathToID.find(key);
            if (existing != pathToID.end())
                return existing->second;

            auto resource = std::make_unique<TextureResource>();
            resource->descriptorSets = descriptorSetManager.allocateForSampledImage(imageView, sampler, imageLayout);
            resource->width = width;
            resource->height = height;
            resource->cacheKey = key;

            texture_id_type id = nextID++;
            resource->id = id;
            idToTexture[id] = std::move(resource);
            pathToID[key] = id;
            return id;
        }

        void unregisterTexture(texture_id_type id)
        {
            auto it = idToTexture.find(id);
            if (it == idToTexture.end())
                return;

            if (!it->second->descriptorSets.empty())
                descriptorSetManager.freeSampledImageSets(it->second->descriptorSets);

            if (!it->second->cacheKey.empty())
                pathToID.erase(it->second->cacheKey);

            idToTexture.erase(it);
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
            if (it == idToTexture.end() || !it->second)
                return 0;
            return it->second->width;
        }

        int getTextureHeight(texture_id_type id) const
        {
            auto it = idToTexture.find(id);
            if (it == idToTexture.end() || !it->second)
                return 0;
            return it->second->height;
        }
};
