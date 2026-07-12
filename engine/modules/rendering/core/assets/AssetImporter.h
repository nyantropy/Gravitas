#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "AssetTypes.h"

namespace gts::rendering
{
    enum class AssetImportCapability : uint32_t
    {
        None = 0,
        Meshes = 1u << 0u,
        Materials = 1u << 1u,
        Textures = 1u << 2u,
        Nodes = 1u << 3u,
        All = 0xFu
    };

    inline constexpr AssetImportCapability operator|(AssetImportCapability lhs,
                                                     AssetImportCapability rhs)
    {
        return static_cast<AssetImportCapability>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    inline constexpr AssetImportCapability operator&(AssetImportCapability lhs,
                                                     AssetImportCapability rhs)
    {
        return static_cast<AssetImportCapability>(
            static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    inline constexpr bool hasImportCapability(AssetImportCapability capabilities,
                                              AssetImportCapability capability)
    {
        return (static_cast<uint32_t>(capabilities) & static_cast<uint32_t>(capability)) != 0u;
    }

    inline constexpr bool supportsRequestedCapabilities(AssetImportCapability available,
                                                        AssetImportCapability requested)
    {
        const uint32_t requestedBits = static_cast<uint32_t>(requested);
        return requestedBits == 0u ||
            (static_cast<uint32_t>(available) & requestedBits) == requestedBits;
    }

    struct AssetImportOptions
    {
        bool flipTexCoordV = true;
        bool includeSourceMaterials = true;
        std::vector<std::filesystem::path> searchPaths;
    };

    struct AssetImportRequest
    {
        std::filesystem::path sourcePath;
        AssetImportOptions options{};
        AssetImportCapability requestedCapabilities =
            AssetImportCapability::Meshes
            | AssetImportCapability::Materials
            | AssetImportCapability::Textures;
        std::string explicitImporter;
    };

    class IAssetImporter
    {
    public:
        virtual ~IAssetImporter() = default;

        virtual std::string_view name() const = 0;
        virtual uint32_t version() const = 0;
        virtual AssetImportCapability capabilities() const = 0;
        virtual bool supports(const std::filesystem::path& sourcePath) const = 0;
        virtual AssetImportResult importAsset(const AssetImportRequest& request) const = 0;
    };

    class AssetImporterRegistry
    {
    public:
        void registerImporter(std::unique_ptr<IAssetImporter> importer)
        {
            if (importer != nullptr)
                importers.push_back(std::move(importer));
        }

        IAssetImporter* findImporter(
            const std::filesystem::path& sourcePath,
            std::string_view explicitImporter = {},
            AssetImportCapability requestedCapabilities = AssetImportCapability::None) const
        {
            for (const std::unique_ptr<IAssetImporter>& importer : importers)
            {
                if (importer == nullptr)
                    continue;
                if (!explicitImporter.empty() && importer->name() != explicitImporter)
                    continue;
                if (!supportsRequestedCapabilities(importer->capabilities(), requestedCapabilities))
                    continue;
                if (explicitImporter.empty() && !importer->supports(sourcePath))
                    continue;
                return importer.get();
            }
            return nullptr;
        }

        AssetImportResult importAsset(const AssetImportRequest& request) const
        {
            IAssetImporter* importer = findImporter(
                request.sourcePath,
                request.explicitImporter,
                request.requestedCapabilities);

            if (importer != nullptr)
                return importer->importAsset(request);

            AssetImportResult result;
            result.diagnostics.push_back({
                AssetDiagnosticSeverity::Error,
                "ASSET_IMPORTER_NOT_FOUND",
                "No importer is registered for source asset: " + request.sourcePath.string(),
                request.sourcePath,
                0
            });
            return result;
        }

    private:
        std::vector<std::unique_ptr<IAssetImporter>> importers;
    };
}
