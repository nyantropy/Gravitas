#include "assets/cooking/legacy/ImageAssetImporter.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <string>

#include "assets/importer/image/GtsImageDecode.h"

namespace gts::rendering
{
namespace
{
    bool supportedImageExtension(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
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

bool ImageAssetImporter::decodeFile(const std::filesystem::path& path, ImportedTexture& texture, std::string* error)
{
    GtsDecodedImage decoded;
    if (!decodeGtsImage(path, decoded, error)) return false;
    texture.sourcePath = path;
    texture.logicalPath = path.filename().generic_string();
    texture.source = ImportedTextureSource::ExternalFile;
    texture.width = decoded.width;
    texture.height = decoded.height;
    texture.sourceChannelCount = decoded.sourceChannelCount;
    texture.rgba8Pixels = std::move(decoded.rgba8Pixels);
    return true;
}
bool ImageAssetImporter::decodeBytes(const std::vector<uint8_t>& bytes, ImportedTexture& texture, std::string* error)
{
    GtsDecodedImage decoded;
    if (!decodeGtsImage(std::span<const uint8_t>(bytes), decoded, error)) return false;
    texture.width = decoded.width;
    texture.height = decoded.height;
    texture.sourceChannelCount = decoded.sourceChannelCount;
    texture.rgba8Pixels = std::move(decoded.rgba8Pixels);
    return true;
}
}
