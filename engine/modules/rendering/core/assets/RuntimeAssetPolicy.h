#pragma once

#include "assets/runtime/RuntimeAssetPolicy.h"

// Existing mesh/texture callers retain their API; the policy is CPU asset-owned.
namespace gts::rendering
{
    using gts::assets::RuntimeSourceAssetPolicy;
    using gts::assets::isCookedMeshAssetPath;
    using gts::assets::isCookedTextureAssetPath;
    using gts::assets::isRuntimeSourceMeshAssetPath;
    using gts::assets::isRuntimeSourceTextureAssetPath;
    using gts::assets::runtimeSourceMeshFallbackSupported;
    using gts::assets::runtimeSourceTextureFallbackSupported;
    using gts::assets::expectedCookedMeshAssetPath;
    using gts::assets::expectedCookedTextureAssetPath;
    using gts::assets::runtimeSourceAssetPolicy;
    using gts::assets::runtimeSourceAssetFallbackAllowed;
}
