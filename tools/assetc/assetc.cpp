#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "AssetCooker.h"

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
            case gts::rendering::CookedAssetOutputType::TextureDependency:
                return "texture";
        }
        return "unknown";
    }

    void printUsage()
    {
        std::cerr
            << "usage: assetc import <source> [--output <directory>] "
            << "[--importer <name>] [--base-color-texture <path>] [--vertex-color-only]\n";
    }

    bool parseArguments(int argc,
                        char** argv,
                        std::filesystem::path& sourcePath,
                        gts::rendering::AssetCookerOptions& options)
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
            else
            {
                return false;
            }
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    std::filesystem::path sourcePath;
    gts::rendering::AssetCookerOptions options;
    if (!parseArguments(argc, argv, sourcePath, options))
    {
        printUsage();
        return 1;
    }

    const gts::rendering::AssetCookResult result =
        gts::rendering::AssetCooker::cookSourceAsset(sourcePath, options);

    for (const gts::rendering::AssetDiagnostic& diagnostic : result.diagnostics)
    {
        std::cerr
            << severityName(diagnostic.severity)
            << " [" << diagnostic.code << "] "
            << diagnostic.message;
        if (!diagnostic.sourcePath.empty())
            std::cerr << " (" << diagnostic.sourcePath.string() << ")";
        std::cerr << '\n';
    }

    for (const gts::rendering::CookedAssetOutput& output : result.outputs)
    {
        std::cout
            << outputTypeName(output.type)
            << " " << output.path.string()
            << " " << output.reference.logicalPath
            << '\n';
    }

    return result.succeeded() ? 0 : 1;
}
