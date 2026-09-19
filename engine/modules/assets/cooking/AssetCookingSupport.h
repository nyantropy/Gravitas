#pragma once
#include "assets/cooking/AssetCooker.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

#include "assets/serialization/AssetSerializers.h"
#include "assets/importer/image/GtsImageDecode.h"
#include "assets/cooking/TextureCooker.h"

namespace gts::rendering::cooking_detail
{
    void addCookDiagnostic(AssetCookResult&             result,
                           AssetDiagnosticSeverity      severity,
                           std::string                  code,
                           std::string                  message,
                           const std::filesystem::path& sourcePath);

    std::filesystem::path outputDirectoryFor(const std::filesystem::path& sourcePath,
                                             const AssetCookerOptions&    options);

    std::string sanitizedName(std::string name, std::string fallback);

    std::string logicalPathForCookedOutput(const std::filesystem::path& path);

    AssetReference referenceForCookedOutput(const std::filesystem::path& path);

    bool sameReference(const AssetReference& lhs, const AssetReference& rhs);

    void addUniqueReference(std::vector<AssetReference>& references, const AssetReference& reference);

    TextureCookRole cookRoleForMaterialRole(MaterialTextureRole role);

    const char* materialRoleSuffix(MaterialTextureRole role);

    std::string textureIdentityFor(const TextureCookInput& texture);

    std::string textureOutputStemFor(const TextureCookInput& texture, const std::string& fallback);

    TextureCookInput textureInputForPath(const std::filesystem::path& path);

    void addMaterialTextureDependencies(MaterialAssetData& asset);

    bool ensureOutputDirectory(const std::filesystem::path& outputDirectory,
                               AssetCookResult&             result,
                               const std::filesystem::path& sourcePath);

    bool decodeTextureInput(TextureCookInput& input, std::string* error);

    struct TextureCookCache
    {
        const std::filesystem::path& outputDirectory;
        const std::string&           sourceStem;
        const AssetCookerOptions&    options;
        AssetCookResult&             result;
        const std::filesystem::path& sourcePath;

        std::map<std::string, AssetReference>  referencesByKey;
        std::map<std::string, std::string>     outputOwners;
        std::map<std::string, TextureCookRole> firstRoleByIdentity;
        std::set<std::string>                  reportedRoleConflicts;

        TextureCookCache(const std::filesystem::path& outputDirectory,
                         const std::string&           sourceStem,
                         const AssetCookerOptions&    options,
                         AssetCookResult&             result,
                         const std::filesystem::path& sourcePath)
            : outputDirectory(outputDirectory), sourceStem(sourceStem), options(options), result(result),
              sourcePath(sourcePath)
        {
        }

        AssetReference cookTexture(TextureCookInput             texture,
                                   TextureCookRole              role,
                                   const std::string&           outputSuffix,
                                   const std::string&           fallbackName,
                                   const std::filesystem::path& diagnosticSource,
                                   const std::string&           identityOverride = {});

        private:
        bool decodeTexture(TextureCookInput& texture, const std::filesystem::path& diagnosticSource);
    };

    void publishCookedFiles(AssetCookResult&                         result,
                            const std::filesystem::path&             sourcePath,
                            const std::vector<std::vector<uint8_t>>& bytes);

} // namespace gts::rendering::cooking_detail
