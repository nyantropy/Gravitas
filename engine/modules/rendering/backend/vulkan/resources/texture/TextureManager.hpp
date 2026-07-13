#pragma once

#include <filesystem>
#include <iostream>
#include <memory>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "DescriptorSetManager.hpp"
#include "MaterialTypes.h"
#include "RuntimeAssetPolicy.h"
#include "TextureResource.h"
#include "TextureAssetLoader.h"
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
        std::unordered_map<std::string, std::vector<VkDescriptorSet>> materialTextureSets;
        std::unordered_set<std::string> sourceFallbackWarnings;
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
            const std::filesystem::path loadPath = preferredTextureLoadPath(path);
            const bool cookedTexture = gts::rendering::isCookedTextureAssetPath(loadPath);
            const std::string key = cookedTexture
                ? cookedTextureKey(loadPath)
                : sourceTextureKey(loadPath, nearestFilter, clampToEdge, colorSpace);

            auto it = pathToID.find(key);
            if (it != pathToID.end())
                return it->second;

            auto resource = std::make_unique<TextureResource>();
            if (cookedTexture)
            {
                gts::rendering::TextureAssetData textureAsset;
                std::string error;
                if (!gts::rendering::TextureAssetLoader::load(loadPath, textureAsset, &error))
                    throw std::runtime_error("failed to load cooked texture asset: " + loadPath.string() + ": " + error);

                resource->texture = std::make_unique<VulkanTexture>(backendContext, textureAsset);
                resource->width = resource->texture->getWidth();
                resource->height = resource->texture->getHeight();
                resource->colorSpace = textureAsset.colorSpace;
            }
            else
            {
                resource->texture = std::make_unique<VulkanTexture>(
                    backendContext,
                    loadPath.generic_string(),
                    nearestFilter,
                    clampToEdge,
                    colorSpace);
                resource->width = resource->texture->getWidth();
                resource->height = resource->texture->getHeight();
                resource->colorSpace = colorSpace;
            }

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

        const std::vector<VkDescriptorSet>* getMaterialTextureDescriptorSets(
            const MaterialTextureIds& textures)
        {
            const std::string key = materialTextureSetKey(textures);
            auto existing = materialTextureSets.find(key);
            if (existing != materialTextureSets.end())
                return &existing->second;

            std::array<TextureResource*, DescriptorSetManager::MaterialTextureBindingCount> resources = {
                getTexture(textures.baseColor),
                getTexture(textures.metallicRoughness),
                getTexture(textures.normal),
                getTexture(textures.ambientOcclusion),
                getTexture(textures.emissive)
            };

            for (TextureResource* texture : resources)
            {
                if (texture == nullptr || texture->texture == nullptr)
                    return nullptr;
            }

            std::array<VkDescriptorImageInfo, DescriptorSetManager::MaterialTextureBindingCount> images{};
            for (size_t i = 0; i < resources.size(); ++i)
            {
                images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                images[i].imageView = resources[i]->texture->getTextureImageView();
                images[i].sampler = resources[i]->texture->getTextureSampler();
            }

            auto [it, inserted] = materialTextureSets.emplace(
                key,
                descriptorSetManager.allocateForMaterialTextures(images));
            return &it->second;
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

    private:
        static std::string materialTextureSetKey(const MaterialTextureIds& textures)
        {
            return std::to_string(textures.baseColor)
                + ":" + std::to_string(textures.metallicRoughness)
                + ":" + std::to_string(textures.normal)
                + ":" + std::to_string(textures.ambientOcclusion)
                + ":" + std::to_string(textures.emissive);
        }

        static std::string sourceTextureKey(const std::filesystem::path& path,
                                            bool nearestFilter,
                                            bool clampToEdge,
                                            TextureColorSpace colorSpace)
        {
            const bool effectiveClampToEdge = nearestFilter || clampToEdge;
            return path.generic_string()
                + (nearestFilter ? ":nearest" : ":linear")
                + (effectiveClampToEdge ? ":clamp" : ":repeat")
                + (colorSpace == TextureColorSpace::SRgb ? ":srgb" : ":linear-data");
        }

        static std::string cookedTextureKey(const std::filesystem::path& path)
        {
            return path.generic_string() + ":gtex";
        }

        std::filesystem::path preferredTextureLoadPath(const std::filesystem::path& requestedPath)
        {
            if (requestedPath.empty() || gts::rendering::isCookedTextureAssetPath(requestedPath))
                return requestedPath;

            if (!gts::rendering::isRuntimeSourceTextureAssetPath(requestedPath))
                return requestedPath;

            const std::filesystem::path expectedCooked =
                gts::rendering::expectedCookedTextureAssetPath(requestedPath);
            if (std::filesystem::exists(expectedCooked))
                return expectedCooked;

            if (!gts::rendering::runtimeSourceAssetFallbackAllowed())
            {
                throw std::runtime_error(
                    "Cooked texture missing; runtime source image fallback is disabled. Source asset: "
                    + requestedPath.string()
                    + " Expected cooked asset: "
                    + expectedCooked.string()
                    + " Active policy: cooked-only");
            }

            if (!gts::rendering::runtimeSourceTextureFallbackSupported(requestedPath))
            {
                throw std::runtime_error(
                    "Cooked texture missing and source image fallback is unsupported: "
                    + requestedPath.string());
            }

            const std::string warningKey = requestedPath.lexically_normal().generic_string();
            if (sourceFallbackWarnings.insert(warningKey).second)
            {
                std::cerr
                    << "warning [TEXTURE_RUNTIME_SOURCE_FALLBACK] Cooked texture missing; "
                    << "falling back to runtime source image decode. Source asset: "
                    << requestedPath.string()
                    << " Expected cooked asset: "
                    << expectedCooked.string()
                    << " Active policy: development-fallback. Run assetc import "
                    << requestedPath.string()
                    << " --output "
                    << (expectedCooked.parent_path().empty()
                        ? std::filesystem::path(".")
                        : expectedCooked.parent_path()).string()
                    << '\n';
            }

            return requestedPath;
        }
};
