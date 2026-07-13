#include "ImageAssetImporter.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include <stb_image.h>

namespace gts::rendering
{
namespace
{
    void setError(std::string* error, const std::string& message)
    {
        if (error != nullptr)
            *error = message;
    }

    bool supportedImageExtension(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
    }

    bool assignDecoded(stbi_uc* pixels,
                       int width,
                       int height,
                       int sourceChannels,
                       ImportedTexture& texture,
                       std::string* error)
    {
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            setError(error, "Image decode failed");
            return false;
        }

        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixelCount == 0 || pixelCount > (static_cast<size_t>(1u) << 31u))
        {
            setError(error, "Image dimensions are invalid or too large");
            return false;
        }

        texture.width = static_cast<uint32_t>(width);
        texture.height = static_cast<uint32_t>(height);
        texture.sourceChannelCount = sourceChannels > 0 ? static_cast<uint32_t>(sourceChannels) : 4u;
        texture.rgba8Pixels.assign(pixels, pixels + pixelCount * 4u);
        return true;
    }
}

std::string_view ImageAssetImporter::name() const
{
    return "image";
}

uint32_t ImageAssetImporter::version() const
{
    return 1;
}

AssetImportCapability ImageAssetImporter::capabilities() const
{
    return AssetImportCapability::Textures;
}

bool ImageAssetImporter::supports(const std::filesystem::path& sourcePath) const
{
    return supportedImageExtension(sourcePath);
}

AssetImportResult ImageAssetImporter::importAsset(const AssetImportRequest& request) const
{
    AssetImportResult result;
    ImportedTexture texture;
    texture.debugName = request.sourcePath.stem().string();
    texture.sourcePath = request.sourcePath;
    texture.logicalPath = request.sourcePath.filename().generic_string();
    texture.source = ImportedTextureSource::ExternalFile;

    std::string error;
    if (!decodeFile(request.sourcePath, texture, &error))
    {
        result.diagnostics.push_back({
            AssetDiagnosticSeverity::Error,
            "TEXTURE_DECODE_FAILED",
            error,
            request.sourcePath,
            0
        });
        return result;
    }

    result.dependencies.push_back({request.sourcePath, AssetDependencyType::SourceFile, true});
    result.textures.push_back(std::move(texture));
    return result;
}

bool ImageAssetImporter::decodeFile(const std::filesystem::path& path,
                                    ImportedTexture& texture,
                                    std::string* error)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load(path.string().c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha),
        stbi_image_free);

    if (!pixels)
    {
        setError(error, "Could not decode image source: " + path.string());
        return false;
    }

    texture.sourcePath = path;
    texture.logicalPath = path.filename().generic_string();
    texture.source = ImportedTextureSource::ExternalFile;
    return assignDecoded(pixels.get(), width, height, sourceChannels, texture, error);
}

bool ImageAssetImporter::decodeBytes(const std::vector<uint8_t>& bytes,
                                     ImportedTexture& texture,
                                     std::string* error)
{
    if (bytes.empty())
    {
        setError(error, "Embedded image payload is empty");
        return false;
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(
            bytes.data(),
            static_cast<int>(bytes.size()),
            &width,
            &height,
            &sourceChannels,
            STBI_rgb_alpha),
        stbi_image_free);

    if (!pixels)
    {
        setError(error, "Could not decode embedded image payload");
        return false;
    }

    return assignDecoded(pixels.get(), width, height, sourceChannels, texture, error);
}
}
