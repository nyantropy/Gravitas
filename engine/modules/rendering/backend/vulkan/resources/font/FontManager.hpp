#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "BitmapFont.h"
#include "BitmapFontLoader.h"
#include "FontAsset.h"
#include "FontAssetIO.h"
#include "TextureManager.hpp"
#include "Types.h"

struct FontResource
{
    FontAsset asset;
    BitmapFont font;
    std::string path;
};

class FontManager
{
private:
    TextureManager* textureManager = nullptr;
    std::unordered_map<std::string, font_id_type> pathToID;
    std::unordered_map<font_id_type, std::unique_ptr<FontResource>> idToFont;
    font_id_type nextID = 1;

public:
    explicit FontManager(TextureManager* inTextureManager)
        : textureManager(inTextureManager)
    {
    }

    font_id_type loadFont(const std::string& path)
    {
        const std::string key = normalizedPath(path);
        auto it = pathToID.find(key);
        if (it != pathToID.end())
            return it->second;

        FontAsset asset;
        if (textureManager == nullptr || !gts::fonts::loadFontAsset(key, asset))
            return 0;

        const std::string atlasPath = resolveAtlasPath(key, asset.atlasPath);
        const texture_id_type atlasTexture = textureManager->loadTexture(atlasPath, asset.pixelSampling);
        if (atlasTexture == 0)
            return 0;

        auto resource = std::make_unique<FontResource>();
        resource->asset = asset;
        resource->font = BitmapFontLoader::buildGridFont(atlasTexture, asset);
        resource->path = key;

        const font_id_type id = nextID++;
        idToFont[id] = std::move(resource);
        pathToID[key] = id;
        return id;
    }

    const BitmapFont* getFont(font_id_type id) const
    {
        auto it = idToFont.find(id);
        if (it != idToFont.end())
            return &it->second->font;
        return nullptr;
    }

private:
    static std::string normalizedPath(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }

    static std::string resolveAtlasPath(const std::string& fontPath,
                                        const std::string& atlasPath)
    {
        const std::filesystem::path atlas(atlasPath);
        if (atlas.is_absolute())
            return atlas.lexically_normal().string();

        const std::filesystem::path base = std::filesystem::path(fontPath).parent_path();
        return (base / atlas).lexically_normal().string();
    }
};
