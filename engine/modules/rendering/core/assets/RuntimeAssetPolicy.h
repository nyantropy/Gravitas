#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace gts::rendering
{
    enum class RuntimeSourceAssetPolicy
    {
        DevelopmentFallback,
        CookedOnly
    };

    inline bool isCookedMeshAssetPath(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".gmesh";
    }

    inline bool isCookedTextureAssetPath(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".gtex";
    }

    inline bool isRuntimeSourceMeshAssetPath(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".obj" || extension == ".gltf" || extension == ".glb";
    }

    inline bool isRuntimeSourceTextureAssetPath(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
    }

    inline bool runtimeSourceMeshFallbackSupported(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".obj";
    }

    inline bool runtimeSourceTextureFallbackSupported(const std::filesystem::path& path)
    {
        return isRuntimeSourceTextureAssetPath(path);
    }

    inline std::filesystem::path expectedCookedMeshAssetPath(const std::filesystem::path& sourcePath)
    {
        if (isCookedMeshAssetPath(sourcePath))
            return sourcePath;

        std::filesystem::path cookedPath = sourcePath;
        cookedPath.replace_extension(".gmesh");
        return cookedPath;
    }

    inline std::filesystem::path expectedCookedTextureAssetPath(const std::filesystem::path& sourcePath)
    {
        if (isCookedTextureAssetPath(sourcePath))
            return sourcePath;

        std::filesystem::path cookedPath = sourcePath;
        cookedPath.replace_extension(".gtex");
        return cookedPath;
    }

    inline RuntimeSourceAssetPolicy runtimeSourceAssetPolicy()
    {
#if defined(GTS_SHIPPING_BUILD) || defined(GTS_DISABLE_RUNTIME_SOURCE_ASSET_FALLBACK)
        return RuntimeSourceAssetPolicy::CookedOnly;
#else
        const char* rawPolicy = std::getenv("GTS_RUNTIME_ASSET_POLICY");
        if (rawPolicy == nullptr)
            return RuntimeSourceAssetPolicy::DevelopmentFallback;

        std::string policy(rawPolicy);
        std::transform(policy.begin(), policy.end(), policy.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });

        if (policy == "strict" || policy == "shipping" || policy == "cooked-only" || policy == "cooked_only")
            return RuntimeSourceAssetPolicy::CookedOnly;

        return RuntimeSourceAssetPolicy::DevelopmentFallback;
#endif
    }

    inline bool runtimeSourceAssetFallbackAllowed()
    {
        return runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::DevelopmentFallback;
    }
}
