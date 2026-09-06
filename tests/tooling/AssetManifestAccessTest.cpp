#include "AssetManifest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("gravitas-manifest-access-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    const auto path = directory / "manifest.json";
    try
    {
        const auto write = [&](const std::string& fields)
        {
            std::ofstream file(path);
            file << "{" << fields << R"(,"id":"asset","displayName":"Asset","model":"model.gmesh",
                "materialMode":"cooked_mesh_materials",
                "scale":[1,1,1],"bounds":{"min":[0,0,0],"max":[1,1,1]},"baseAtGround":false})";
            require(file.good(), "Could not write manifest");
        };
        gts::tools::AssetManifest manifest;
        std::string               error;
        write(R"("version":1,"preview":null,"fallbackTexture":null)");
        if (!gts::tools::loadAssetManifest(path.string(), manifest, &error))
            throw std::runtime_error(error);
        require(!manifest.baseAtGround && manifest.id == "asset", "False boolean");
        for (const auto token : {"1.5", "2147483648", "-2147483649", "1e100"})
        {
            write(std::string("\"version\":") + token);
            require(!gts::tools::loadAssetManifest(path.string(), manifest, &error), "Invalid integer accepted");
            require(error.find("integer") != std::string::npos && error.find("version") != std::string::npos,
                    "Integer diagnostic lost");
            require(manifest.id == "asset" && manifest.version == 1, "Failed load changed output");
        }
        write(R"("preview":null)");
        require(!gts::tools::loadAssetManifest(path.string(), manifest, &error) &&
                    error.find("missing required") != std::string::npos,
                "Missing field diagnostic");
        write(R"("version":"1")");
        require(!gts::tools::loadAssetManifest(path.string(), manifest, &error) &&
                    error.find("must be a number") != std::string::npos,
                "Wrong type diagnostic");
        write(R"("version":1,"preview":{"cameraDistance":1e100})");
        require(!gts::tools::loadAssetManifest(path.string(), manifest, &error) &&
                    error.find("float") != std::string::npos,
                "Float overflow");
        write(R"("version":1,"preview":{"cameraDistance":-1})");
        require(!gts::tools::loadAssetManifest(path.string(), manifest, &error) &&
                    error.find("positive") != std::string::npos,
                "Feature range validation");
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::filesystem::remove_all(directory);
    return 0;
}
