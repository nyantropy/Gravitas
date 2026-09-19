#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "GtsModelCooker.h"
#include "TextureCooker.h"

namespace
{
    const char* severityName(gts::rendering::AssetDiagnosticSeverity severity)
    {
        switch (severity)
        {
        case gts::rendering::AssetDiagnosticSeverity::Info:
            return "info";
        case gts::rendering::AssetDiagnosticSeverity::Warning:
            return "warning";
        case gts::rendering::AssetDiagnosticSeverity::Error:
            return "error";
        }
        return "unknown";
    }

    const char* outputTypeName(gts::rendering::CookedAssetOutputType type)
    {
        switch (type)
        {
        case gts::rendering::CookedAssetOutputType::Mesh:
            return "mesh";
        case gts::rendering::CookedAssetOutputType::Material:
            return "material";
        case gts::rendering::CookedAssetOutputType::Model:
            return "model";
        case gts::rendering::CookedAssetOutputType::Texture:
            return "texture";
        case gts::rendering::CookedAssetOutputType::TextureDependency:
            return "texture-dependency";
        }
        return "unknown";
    }

    void printUsage()
    {
        std::cerr << "usage: assetc import <source> [--output <directory>] "
                  << "[--importer <name>] [--base-color-texture <path>] [--vertex-color-only] "
                  << "[--texture-role <role>] [--single-mip]\n";
    }

    bool parseArguments(int                                    argc,
                        char**                                 argv,
                        std::filesystem::path&                 sourcePath,
                        gts::rendering::GtsModelCookerOptions& options)
    {
        if (argc < 3 || std::string(argv[1]) != "import")
            return false;

        sourcePath = argv[2];
        for (int i = 3; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--output" && i + 1 < argc)
            {
                options.outputDirectory = argv[++i];
            }
            else if (arg == "--importer" && i + 1 < argc)
            {
                options.explicitImporter = argv[++i];
            }
            else if (arg == "--base-color-texture" && i + 1 < argc)
            {
                options.baseColorTextureOverride = argv[++i];
            }
            else if (arg == "--vertex-color-only")
            {
                options.vertexColorOnly = true;
            }
            else if (arg == "--texture-role" && i + 1 < argc)
            {
                gts::rendering::TextureCookRole role{};
                if (!gts::rendering::parseTextureCookRole(argv[++i], role))
                    return false;
                options.textureRole = role;
            }
            else if (arg == "--single-mip")
            {
                options.generateTextureMipmaps = false;
            }
            else
            {
                return false;
            }
        }
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path                 sourcePath;
    gts::rendering::GtsModelCookerOptions options;
    if (!parseArguments(argc, argv, sourcePath, options))
    {
        printUsage();
        return 1;
    }

    auto extension = sourcePath.extension().string();
    std::transform(extension.begin(),
                   extension.end(),
                   extension.begin(),
                   [](unsigned char c)
                   {
                       return static_cast<char>(std::tolower(c));
                   });
    const bool model =
        options.explicitImporter == "obj" || options.explicitImporter == "gltf" ||
        (options.explicitImporter.empty() && (extension == ".obj" || extension == ".gltf" || extension == ".glb"));
    const gts::rendering::AssetCookResult result =
        model ? static_cast<gts::rendering::AssetCookResult>(
                    gts::rendering::GtsModelCooker::cookSourceAsset(sourcePath, options))
              : gts::rendering::AssetCooker::cookSourceAsset(sourcePath, options);

    for (const gts::rendering::AssetDiagnostic& diagnostic : result.diagnostics)
    {
        std::cerr << severityName(diagnostic.severity) << " [" << diagnostic.code << "] " << diagnostic.message;
        if (!diagnostic.sourcePath.empty())
            std::cerr << " (" << diagnostic.sourcePath.string() << ")";
        std::cerr << '\n';
    }

    for (const gts::rendering::CookedAssetOutput& output : result.outputs)
    {
        std::cout << outputTypeName(output.type) << " " << output.path.string() << " " << output.reference.logicalPath
                  << '\n';
    }

    return result.succeeded() ? 0 : 1;
}
