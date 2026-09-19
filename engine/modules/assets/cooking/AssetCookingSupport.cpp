#include "assets/cooking/AssetCookingSupport.h"

namespace gts::rendering::cooking_detail
{
    void addCookDiagnostic(AssetCookResult&             result,
                           AssetDiagnosticSeverity      severity,
                           std::string                  code,
                           std::string                  message,
                           const std::filesystem::path& sourcePath)
    {
        result.diagnostics.push_back({severity, std::move(code), std::move(message), sourcePath, 0});
    }

    std::filesystem::path outputDirectoryFor(const std::filesystem::path& sourcePath, const AssetCookerOptions& options)
    {
        if (!options.outputDirectory.empty())
            return options.outputDirectory;
        if (!sourcePath.parent_path().empty())
            return sourcePath.parent_path();
        return ".";
    }

    std::string sanitizedName(std::string name, std::string fallback)
    {
        if (name.empty())
            name = std::move(fallback);

        for (char& ch : name)
        {
            const unsigned char value = static_cast<unsigned char>(ch);
            if (std::isalnum(value) == 0 && ch != '_' && ch != '-' && ch != '.')
                ch = '_';
        }

        if (name.empty())
            return "asset";
        return name;
    }

    std::string logicalPathForCookedOutput(const std::filesystem::path& path)
    {
        return path.filename().generic_string();
    }

    AssetReference referenceForCookedOutput(const std::filesystem::path& path)
    {
        return AssetReference::fromLogicalPath(logicalPathForCookedOutput(path));
    }

    bool sameReference(const AssetReference& lhs, const AssetReference& rhs)
    {
        return lhs.id == rhs.id && lhs.logicalPath == rhs.logicalPath;
    }

    void addUniqueReference(std::vector<AssetReference>& references, const AssetReference& reference)
    {
        if (reference.empty())
            return;

        const auto found = std::find_if(references.begin(),
                                        references.end(),
                                        [&reference](const AssetReference& existing)
                                        {
                                            return sameReference(existing, reference);
                                        });
        if (found == references.end())
            references.push_back(reference);
    }

    TextureCookRole cookRoleForMaterialRole(MaterialTextureRole role)
    {
        switch (role)
        {
        case MaterialTextureRole::BaseColor:
            return TextureCookRole::BaseColor;
        case MaterialTextureRole::MetallicRoughness:
            return TextureCookRole::MetallicRoughness;
        case MaterialTextureRole::Normal:
            return TextureCookRole::Normal;
        case MaterialTextureRole::AmbientOcclusion:
            return TextureCookRole::AmbientOcclusion;
        case MaterialTextureRole::Emissive:
            return TextureCookRole::Emissive;
        }
        return TextureCookRole::BaseColor;
    }

    std::string textureIdentityFor(const TextureCookInput& texture)
    {
        if (texture.source == TextureCookSource::ExternalFile && !texture.sourcePath.empty())
            return "file:" + texture.sourcePath.lexically_normal().generic_string();
        if (!texture.logicalPath.empty())
            return "logical:" + texture.logicalPath;
        if (!texture.embeddedBytes.empty())
        {
            uint64_t hash = 1469598103934665603ull;
            for (uint8_t byte : texture.embeddedBytes)
            {
                hash ^= byte;
                hash *= 1099511628211ull;
            }
            return "embedded:" + std::to_string(texture.embeddedBytes.size()) + ":" + std::to_string(hash);
        }
        return "debug:" + texture.debugName;
    }

    std::string textureOutputStemFor(const TextureCookInput& texture, const std::string& fallback)
    {
        std::string stem;
        if (texture.source == TextureCookSource::ExternalFile && !texture.sourcePath.empty())
            stem = texture.sourcePath.stem().string();
        if (stem.empty() && !texture.logicalPath.empty())
            stem = std::filesystem::path(texture.logicalPath).stem().string();
        if (stem.empty())
            stem = texture.debugName;
        return sanitizedName(stem, fallback);
    }

    TextureCookInput textureInputForPath(const std::filesystem::path& path)
    {
        TextureCookInput texture;
        texture.debugName   = path.stem().string();
        texture.sourcePath  = path;
        texture.logicalPath = path.filename().generic_string();
        texture.source      = TextureCookSource::ExternalFile;
        return texture;
    }

    void addMaterialTextureDependencies(MaterialAssetData& asset)
    {
        asset.dependencies.clear();
        addUniqueReference(asset.dependencies, asset.baseColorTexture);
        addUniqueReference(asset.dependencies, asset.metallicRoughnessTexture);
        addUniqueReference(asset.dependencies, asset.normalTexture);
        addUniqueReference(asset.dependencies, asset.ambientOcclusionTexture);
        addUniqueReference(asset.dependencies, asset.emissiveTexture);
    }

    bool ensureOutputDirectory(const std::filesystem::path& outputDirectory,
                               AssetCookResult&             result,
                               const std::filesystem::path& sourcePath)
    {
        std::error_code errorCode;
        std::filesystem::create_directories(outputDirectory, errorCode);
        if (errorCode)
        {
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_OUTPUT_DIRECTORY_FAILED",
                              "Could not create asset output directory: " + outputDirectory.string(),
                              sourcePath);
            return false;
        }
        return true;
    }

    bool decodeTextureInput(TextureCookInput& input, std::string* error)
    {
        if (input.decoded())
            return true;
        GtsDecodedImage decoded;
        const bool      ok = input.source == TextureCookSource::ExternalFile
                                 ? decodeGtsImage(input.sourcePath, decoded, error)
                                 : decodeGtsImage(std::span<const uint8_t>(input.embeddedBytes), decoded, error);
        if (!ok)
            return false;
        input.width              = decoded.width;
        input.height             = decoded.height;
        input.sourceChannelCount = decoded.sourceChannelCount;
        input.rgba8Pixels        = std::move(decoded.rgba8Pixels);
        return true;
    }

    void publishCookedFiles(AssetCookResult&                         result,
                            const std::filesystem::path&             sourcePath,
                            const std::vector<std::vector<uint8_t>>& bytes)
    {
        if (result.outputs.empty())
            return;
        const auto directory = result.outputs.front().path.parent_path();
        if (!ensureOutputDirectory(directory, result, sourcePath))
        {
            result.outputs.clear();
            return;
        }
        static std::atomic<uint64_t> sequence{0};
        auto                         staging =
            directory / (".assetc-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                         "-" + std::to_string(sequence++));
        std::error_code error;
        if (!std::filesystem::create_directory(staging, error))
        {
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_WRITE_FAILED",
                              "Cannot stage package: " + error.message(),
                              sourcePath);
            result.outputs.clear();
            return;
        }
        std::vector<size_t> installed, backedUp;
        auto                fail = [&](const std::string& message)
        {
            addCookDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_COOK_WRITE_FAILED", message, sourcePath);
            for (auto i : installed)
            {
                std::error_code ignored;
                std::filesystem::remove(result.outputs[i].path, ignored);
            }
            for (auto i : backedUp)
            {
                std::error_code restoreError;
                std::filesystem::rename(staging / (std::to_string(i) + ".old"), result.outputs[i].path, restoreError);
                if (restoreError)
                    addCookDiagnostic(result,
                                      AssetDiagnosticSeverity::Error,
                                      "ASSET_COOK_RESTORE_FAILED",
                                      "Previous output retained in " + staging.string() + ": " + restoreError.message(),
                                      sourcePath);
            }
            result.outputs.clear();
        };
        // Stage all writes before replacing any published component.
        for (size_t i = 0; i < bytes.size(); ++i)
        {
            std::ofstream file(staging / std::to_string(i), std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(bytes[i].data()), static_cast<std::streamsize>(bytes[i].size()));
            file.close();
            if (!file)
            {
                fail("Cannot stage " + result.outputs[i].path.string());
                break;
            }
        }
        for (size_t i = 0; i < result.outputs.size(); ++i)
        {
            const auto& path = result.outputs[i].path;
            if (std::filesystem::exists(path, error))
            {
                if (!std::filesystem::is_regular_file(path, error))
                {
                    fail("Output is not a regular file: " + path.string());
                    break;
                }
                std::filesystem::rename(path, staging / (std::to_string(i) + ".old"), error);
                if (error)
                {
                    fail("Cannot replace " + path.string() + ": " + error.message());
                    break;
                }
                backedUp.push_back(i);
            }
            std::filesystem::rename(staging / std::to_string(i), path, error);
            if (error)
            {
                fail("Cannot publish " + path.string() + ": " + error.message());
                break;
            }
            installed.push_back(i);
        }
        const bool restoreFailed = std::any_of(result.diagnostics.begin(),
                                               result.diagnostics.end(),
                                               [](const auto& d)
                                               {
                                                   return d.code == "ASSET_COOK_RESTORE_FAILED";
                                               });
        if (!restoreFailed)
            std::filesystem::remove_all(staging, error);
    }

    const char* materialRoleSuffix(MaterialTextureRole role)
    {
        switch (role)
        {
        case MaterialTextureRole::BaseColor:
            return "";
        case MaterialTextureRole::MetallicRoughness:
            return "_metallic_roughness";
        case MaterialTextureRole::Normal:
            return "_normal";
        case MaterialTextureRole::AmbientOcclusion:
            return "_ao";
        case MaterialTextureRole::Emissive:
            return "_emissive";
        }
        return "";
    }

    AssetReference TextureCookCache::cookTexture(TextureCookInput             texture,
                                                 TextureCookRole              role,
                                                 const std::string&           outputSuffix,
                                                 const std::string&           fallbackName,
                                                 const std::filesystem::path& diagnosticSource,
                                                 const std::string&           identityOverride)
    {
        const std::string identity  = identityOverride.empty() ? textureIdentityFor(texture) : identityOverride;
        const auto        roleFound = firstRoleByIdentity.find(identity);
        if (roleFound == firstRoleByIdentity.end())
        {
            firstRoleByIdentity.emplace(identity, role);
        }
        else if (roleFound->second != role && reportedRoleConflicts.insert(identity).second)
        {
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Warning,
                              "TEXTURE_COLOR_SPACE_CONFLICT",
                              "The same source texture is used with multiple texture roles; cooker emitted "
                              "separate cooked variants",
                              diagnosticSource);
        }

        const std::string key            = identity + "|role=" + std::to_string(static_cast<uint16_t>(role));
        const auto        referenceFound = referencesByKey.find(key);
        if (referenceFound != referencesByKey.end())
            return referenceFound->second;

        if (!decodeTexture(texture, diagnosticSource))
            return {};

        const std::string baseStem       = textureOutputStemFor(texture, sourceStem + "_" + fallbackName);
        std::string       fileStem       = baseStem + outputSuffix;
        std::string       fileName       = fileStem + ".gtex";
        uint32_t          collisionIndex = 1;
        while (true)
        {
            const auto ownerFound = outputOwners.find(fileName);
            if (ownerFound == outputOwners.end() || ownerFound->second == key)
                break;
            fileName = fileStem + "_" + std::to_string(collisionIndex++) + ".gtex";
        }
        outputOwners[fileName] = key;

        const std::filesystem::path outputPath = outputDirectory / fileName;
        const AssetReference        reference  = referenceForCookedOutput(outputPath);

        TextureCookerOptions textureOptions;
        textureOptions.role            = role;
        textureOptions.generateMipmaps = options.generateTextureMipmaps;
        textureOptions.sampler         = defaultSamplerForTextureCookRole(role, options.generateTextureMipmaps);
        textureOptions.debugName       = baseStem;

        TextureCookResult cookResult =
            TextureCooker::cookTexture(texture, reference.id, textureOptions, diagnosticSource);
        result.diagnostics.insert(
            result.diagnostics.end(), cookResult.diagnostics.begin(), cookResult.diagnostics.end());
        if (cookResult.hasErrors())
            return {};

        result.textures.push_back(std::move(cookResult.texture));

        result.outputs.push_back({CookedAssetOutputType::Texture, outputPath, reference});
        referencesByKey.emplace(key, reference);
        return reference;
    }

    bool TextureCookCache::decodeTexture(TextureCookInput& texture, const std::filesystem::path& diagnosticSource)
    {
        std::string error;
        if (decodeTextureInput(texture, &error))
            return true;
        addCookDiagnostic(result, AssetDiagnosticSeverity::Error, "TEXTURE_DECODE_FAILED", error, diagnosticSource);
        return false;
    }
} // namespace gts::rendering::cooking_detail
