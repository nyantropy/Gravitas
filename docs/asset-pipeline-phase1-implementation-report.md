# Asset Pipeline Phase 1 Implementation Report

## Summary

Implemented the first engine-owned, CPU-only asset boundary for OBJ mesh import.
OBJ parsing now lives behind `ObjAssetImporter` and produces import data before
the existing runtime mesh upload path creates `MeshResource` GPU buffers.

## Files Added

- `engine/engine/modules/rendering/core/assets/AssetTypes.h`
- `engine/engine/modules/rendering/core/assets/AssetImporter.h`
- `engine/engine/modules/rendering/core/assets/AssetCooker.h`
- `engine/engine/modules/rendering/core/assets/ObjAssetImporter.h`
- `engine/engine/modules/rendering/core/assets/ObjAssetImporter.cpp`
- `engine/tests/assets/ObjAssetImporterTest.cpp`

## Files Changed

- `engine/engine/modules/rendering/backend/vulkan/resources/mesh/GtsModelLoader.hpp`
- `engine/engine/modules/rendering/backend/vulkan/resources/mesh/GtsModelLoader.cpp`
- `engine/engine/modules/rendering/CMakeLists.txt`
- `engine/engine/modules/rendering/backend/vulkan/CMakeLists.txt`
- `engine/tests/CMakeLists.txt`

## Architecture Decisions

- Import data lives under `rendering/core/assets` because the first slice uses
  existing rendering CPU value types: `Vertex`, `VertexAttributeFlags`,
  `MaterialShaderFamily`, `MaterialRenderState`, and `TextureColorSpace`.
- Import types contain no Vulkan handles, descriptor sets, ECS entities,
  runtime manager handles, parser-library types, or raw owning pointers.
- `ObjAssetImporter` is the only tinyobj parser owner.
- `GtsModelLoader` remains as a compatibility bridge for `MeshManager`, but it
  now calls `ObjAssetImporter` and `cookImportedMesh`.
- Tinyobj implementation ownership moved from `gravitas_vulkan_backend` to
  `gravitas_rendering` so importer tests can link without a Vulkan backend.
- The mesh cooker adapter is CPU-only and in-memory only. It does not serialize
  `.gmesh` or `.gmat`.

## Deviations

- The architecture document is located at
  `engine/docs/engine-asset-pipeline-architecture.md`, not under root `docs/`.
- `.gmesh` and `.gmat` serialization were not implemented because this task
  explicitly excluded those formats.
- Final OBJ/MTL material mapping was not implemented. Imported material names,
  basic values, texture paths, dependencies, and primitive assignments are
  preserved for the next step.
- Tests use generated temporary OBJ/MTL fixtures, matching existing engine test
  style for OBJ coverage.

## Known Limitations

- OBJ source files are still parsed at runtime through the transitional bridge.
- `MeshResource` still does not store imported submesh/material ranges.
- `MaterialAssetData` exists as CPU asset data, but no runtime material loader
  consumes it yet.
- Explicit PNG/material override manifests are not implemented yet.
- Texture import/cooking remains unchanged.
- glTF, skeletal animation, additional UV sets, and cooked package assets are
  not implemented.

## New OBJ Data Flow

```text
OBJ / optional MTL
    |
    v
ObjAssetImporter
    |
    v
AssetImportResult
  - ImportedMesh
  - ImportedMaterial
  - ImportedTexture
  - AssetDependency
  - AssetDiagnostic
    |
    v
cookImportedMesh
    |
    v
MeshAssetData
    |
    v
GtsModelLoader compatibility bridge
    |
    v
MeshManager::loadMesh
    |
    v
MeshResource
    |
    v
existing Vulkan buffer upload
```

## Remaining Direct Source-To-Runtime Paths

- Ordinary PNG/JPG texture path:
  `TextureManager::loadTexture -> VulkanTexture -> stbi_load -> TextureResource`.
- HDR environment path:
  `EnvironmentResourceManager -> stbi_is_hdr/stbi_loadf -> Vulkan environment resources`.
- Bitmap font path:
  `FontManager -> FontAssetIO -> BitmapFont`, with atlas images still loaded
  through `TextureManager`.
- Particle effect path:
  `ParticleEffectAssetIO -> ParticleEffectAsset -> ParticleEmitterComponent`.
- UI JSON paths:
  `loadUiSerializedAssetFromFile`, `loadUiWidgetAssetFromFile`, and
  `UiAssetRuntime` source reload.
- Shader bytecode path:
  `VulkanShader::readFile -> VkShaderModule`.
- Transitional OBJ runtime path:
  `MeshManager::loadMesh -> GtsModelLoader -> ObjAssetImporter -> MeshAssetData`.
  This now has the CPU import boundary, but it is still a runtime source parse
  until cooked mesh loading is added.

## Tests Run

- `cmake --build build`
- `cmake -S engine -B /tmp/dc-engine-asset-build -G Ninja -DGTS_BUILD_TEST_SCENES=OFF`
- `cmake --build /tmp/dc-engine-asset-build --target ObjAssetImporterTest`
- `/tmp/dc-engine-asset-build/tests/ObjAssetImporterTest`
- `cmake --build /tmp/dc-engine-asset-build --target GeometryPbrPreparationTest`
- `/tmp/dc-engine-asset-build/tests/GeometryPbrPreparationTest`
- `ctest --test-dir /tmp/dc-engine-asset-build -R 'obj_asset_importer|geometry_pbr_preparation' --output-on-failure`

## Next Recommended Step

Implement the real cooker/runtime loader step for mesh and material assets:
`MeshAssetData -> .gmesh -> MeshAssetLoader -> MeshManager` and
`MaterialAssetData -> .gmat -> MaterialRuntime`. That removes runtime OBJ
parsing for cooked assets while keeping the importer boundary introduced here.
