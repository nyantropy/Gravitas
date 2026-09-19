#include "AssetCookingSupport.h"
namespace gts::rendering
{
    using namespace cooking_detail;
    AssetCookResult AssetCooker::cookSourceAsset(const std::filesystem::path& sourcePath,
                                                 const AssetCookerOptions&    options)
    {
        std::string extension = sourcePath.extension().string();
        std::transform(extension.begin(),
                       extension.end(),
                       extension.begin(),
                       [](unsigned char ch)
                       {
                           return static_cast<char>(std::tolower(ch));
                       });
        AssetCookResult result;
        if (options.explicitImporter == "image" ||
            (options.explicitImporter.empty() && (extension == ".png" || extension == ".jpg" || extension == ".jpeg")))
        {
            const auto       outputDirectory = outputDirectoryFor(sourcePath, options);
            const auto       stem            = sanitizedName(sourcePath.stem().string(), "texture");
            TextureCookCache cache(outputDirectory, stem, options, result, sourcePath);
            auto             texture = textureInputForPath(sourcePath);
            cache.cookTexture(std::move(texture), options.textureRole, "", stem, sourcePath);
            if (result.textures.empty() && !result.hasErrors())
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Error,
                                  "TEXTURE_DECODE_FAILED",
                                  "Image cooking requires decoded pixels",
                                  sourcePath);
            if (!result.hasErrors())
            {
                std::vector<std::vector<uint8_t>> bytes(result.textures.size());
                for (size_t i = 0; i < result.textures.size(); ++i)
                {
                    std::string error;
                    if (!TextureAssetSerializer::serialize(result.textures[i], bytes[i], &error))
                        addCookDiagnostic(
                            result, AssetDiagnosticSeverity::Error, "ASSET_COOK_SERIALIZATION", error, sourcePath);
                }
                if (!result.hasErrors())
                    publishCookedFiles(result, sourcePath, bytes);
            }
            if (result.hasErrors())
                result.outputs.clear();
            return result;
        }
        addCookDiagnostic(result,
                          AssetDiagnosticSeverity::Error,
                          "ASSET_IMPORTER_NOT_FOUND",
                          "No image decoder for source: " + sourcePath.string(),
                          sourcePath);
        return result;
    }
} // namespace gts::rendering
