#include <cassert>
#include <cstdlib>
#include <filesystem>

#include "RuntimeAssetPolicy.h"

namespace
{
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

    assert(isCookedMeshAssetPath("robot.gmesh"));
    assert(isCookedMeshAssetPath("ROBOT.GMESH"));
    assert(!isCookedMeshAssetPath("robot.obj"));
    assert(isCookedTextureAssetPath("brick.gtex"));
    assert(isCookedTextureAssetPath("BRICK.GTEX"));
    assert(!isCookedTextureAssetPath("brick.png"));

    assert(isRuntimeSourceMeshAssetPath("robot.obj"));
    assert(isRuntimeSourceMeshAssetPath("robot.gltf"));
    assert(isRuntimeSourceMeshAssetPath("robot.glb"));
    assert(!isRuntimeSourceMeshAssetPath("robot.gmesh"));
    assert(isRuntimeSourceTextureAssetPath("brick.png"));
    assert(isRuntimeSourceTextureAssetPath("brick.jpg"));
    assert(isRuntimeSourceTextureAssetPath("brick.jpeg"));
    assert(!isRuntimeSourceTextureAssetPath("brick.gtex"));
    assert(runtimeSourceMeshFallbackSupported("robot.obj"));
    assert(!runtimeSourceMeshFallbackSupported("robot.gltf"));
    assert(!runtimeSourceMeshFallbackSupported("robot.glb"));
    assert(runtimeSourceTextureFallbackSupported("brick.png"));

    assert(expectedCookedMeshAssetPath("assets/robot.obj") ==
           std::filesystem::path("assets/robot.gmesh"));
    assert(expectedCookedMeshAssetPath("assets/robot.gmesh") ==
           std::filesystem::path("assets/robot.gmesh"));
    assert(expectedCookedTextureAssetPath("assets/brick.png") ==
           std::filesystem::path("assets/brick.gtex"));
    assert(expectedCookedTextureAssetPath("assets/brick.gtex") ==
           std::filesystem::path("assets/brick.gtex"));

    clearPolicyEnv();
    assert(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::DevelopmentFallback);
    assert(runtimeSourceAssetFallbackAllowed());

    setPolicyEnv("strict");
    assert(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::CookedOnly);
    assert(!runtimeSourceAssetFallbackAllowed());

    setPolicyEnv("cooked-only");
    assert(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::CookedOnly);

    setPolicyEnv("development");
    assert(runtimeSourceAssetPolicy() == RuntimeSourceAssetPolicy::DevelopmentFallback);
    assert(runtimeSourceAssetFallbackAllowed());

    clearPolicyEnv();
    return 0;
}
