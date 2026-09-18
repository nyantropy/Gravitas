#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "assets/loading/mesh/RuntimeMeshLoading.h"
#include "assets/serialization/AssetSerializers.h"

#include "assets/loading/RuntimeAssetPolicy.h"

namespace
{
    void require(bool condition)
    {
        if (!condition) throw std::runtime_error("Runtime asset policy regression");
    }

    void setPolicyEnv(const char* value)
    {
#if defined(_WIN32)
        _putenv_s("GTS_RUNTIME_ASSET_POLICY", value);
#else
        setenv("GTS_RUNTIME_ASSET_POLICY", value, 1);
#endif
    }

    void clearPolicyEnv()
    {
#if defined(_WIN32)
        _putenv_s("GTS_RUNTIME_ASSET_POLICY", "");
#else
        unsetenv("GTS_RUNTIME_ASSET_POLICY");
#endif
    }
}

int main()
{
    using namespace gts::rendering;
    using namespace gts::assets;

    require(isCookedMeshAssetPath("robot.gmesh"));
    require(isCookedMeshAssetPath("ROBOT.GMESH"));
    require(!isCookedMeshAssetPath("robot.obj"));
    require(isCookedTextureAssetPath("brick.gtex"));
    require(isCookedTextureAssetPath("BRICK.GTEX"));
    require(!isCookedTextureAssetPath("brick.png"));

    require(isRuntimeSourceMeshAssetPath("robot.obj"));
    require(isRuntimeSourceMeshAssetPath("robot.gltf"));
    require(isRuntimeSourceMeshAssetPath("robot.glb"));
    require(!isRuntimeSourceMeshAssetPath("robot.gmesh"));
    require(isRuntimeSourceTextureAssetPath("brick.png"));
    require(isRuntimeSourceTextureAssetPath("brick.jpg"));
    require(isRuntimeSourceTextureAssetPath("brick.jpeg"));
    require(!isRuntimeSourceTextureAssetPath("brick.gtex"));
    require(runtimeSourceMeshFallbackSupported("robot.obj"));
    require(!runtimeSourceMeshFallbackSupported("robot.gltf"));
    require(!runtimeSourceMeshFallbackSupported("robot.glb"));
    require(runtimeSourceTextureFallbackSupported("brick.png"));

    require(expectedCookedMeshAssetPath("assets/robot.obj") ==
           std::filesystem::path("assets/robot.gmesh"));
    require(expectedCookedMeshAssetPath("assets/robot.gmesh") ==
           std::filesystem::path("assets/robot.gmesh"));
    require(expectedCookedTextureAssetPath("assets/brick.png") ==
           std::filesystem::path("assets/brick.gtex"));
    require(expectedCookedTextureAssetPath("assets/brick.gtex") ==
           std::filesystem::path("assets/brick.gtex"));

    clearPolicyEnv();
    require(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::DevelopmentFallback);
    require(runtimeSourceAssetFallbackAllowed());

    setPolicyEnv("strict");
    require(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::CookedOnly);
    require(!runtimeSourceAssetFallbackAllowed());

    setPolicyEnv("cooked-only");
    require(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::CookedOnly);

    setPolicyEnv("development");
    require(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::DevelopmentFallback);
    require(runtimeSourceAssetFallbackAllowed());

    clearPolicyEnv();
    const auto root = std::filesystem::temp_directory_path() / "gravitas-mesh-policy-regression";
    std::filesystem::create_directories(root);
    const auto source = root / "triangle.obj";
    const auto cooked = expectedCookedMeshAssetPath(source);
    std::ofstream(source) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    require(resolveRuntimeMeshPath(source) == source);
    std::vector<GtsModelDiagnostic> diagnostics;
    const auto sourceMesh = loadRuntimeMeshAsset(source, diagnostics);
    require(sourceMesh.vertices.size() == 3 && sourceMesh.indices.size() == 3);
    std::string error;
    require(MeshAssetSerializer::writeFile(sourceMesh, cooked, &error));
    require(resolveRuntimeMeshPath(source) == cooked);
    setPolicyEnv("strict");
    const auto loaded = loadRuntimeMeshAsset(source, diagnostics);
    require(loaded.vertices == sourceMesh.vertices && loaded.indices == sourceMesh.indices);
    std::ofstream(cooked) << "corrupt";
    for (const auto* policy : {"strict", "development"})
    {
        setPolicyEnv(policy);
        bool failed = false;
        try { loadRuntimeMeshAsset(source, diagnostics); }
        catch (const std::runtime_error&) { failed = true; }
        require(failed);
    }
    std::filesystem::remove(cooked);
    setPolicyEnv("strict");
    bool failed = false;
    try { loadRuntimeMeshAsset(source, diagnostics); }
    catch (const std::runtime_error&) { failed = true; }
    require(failed);
    clearPolicyEnv();
    std::filesystem::remove_all(root);
    return 0;
}
