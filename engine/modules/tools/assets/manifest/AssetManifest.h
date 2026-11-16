#pragma once

#include <string>

#include "GlmConfig.h"

namespace gts::tools
{
    enum class AssetMaterialMode
    {
        UnlitTextureOverride,
        CookedMeshMaterials
    };

    struct AssetPreviewSettings
    {
        float cameraDistance = 4.0f;
    };

    struct AssetManifestBounds
    {
        glm::vec3 min{-0.5f};
        glm::vec3 max{0.5f};
    };

    struct AssetManifest
    {
        int version = 1;
        std::string id;
        std::string displayName;
        std::string sourceModelPath;
        std::string modelPath;
        std::string fallbackTexturePath;
        AssetMaterialMode materialMode = AssetMaterialMode::UnlitTextureOverride;
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
        AssetManifestBounds bounds;
        bool baseAtGround = true;
        AssetPreviewSettings preview;
    };

    const char* assetMaterialModeName(AssetMaterialMode mode);

    bool loadAssetManifest(const std::string& path,
                           AssetManifest& manifest,
                           std::string* error = nullptr);
    AssetManifest loadAssetManifestOrThrow(const std::string& path);
}
