# Asset Pipeline Phase 5 Implementation Report

## Summary

Phase 5 introduces cooked texture assets (`.gtex`) for ordinary LDR textures.
Runtime texture loading now prefers engine-owned cooked texture data and only
decodes PNG/JPG in explicit development fallback mode.

Implemented flow:

```text
PNG / JPG / embedded glTF image
  -> ImageAssetImporter / ImportedTexture
  -> TextureCooker
  -> .gtex
  -> TextureAssetLoader
  -> TextureManager
  -> VulkanTexture / TextureResource
  -> Renderer
```

## Texture Inventory

- Ordinary LDR game textures: previously `TextureManager -> VulkanTexture ->
  stb_image`; now `TextureManager` prefers adjacent `.gtex` and logs fallback.
- OBJ/MTL textures: imported as `ImportedTexture` dependencies and cooked into
  `.gtex`; `.gmat` references the cooked texture.
- glTF external and embedded textures: imported as `ImportedTexture`; embedded
  GLB bytes cook directly to `.gtex` without temporary extracted PNG files.
- UI textures and pictures: DungeonCrawler build cooks these as sRGB single-mip
  `.gtex` assets.
- Font atlases: cooked as linear `font-atlas` single-mip `.gtex`; grayscale
  atlas alpha semantics are preserved during cooking.
- Particle textures: cooked as sRGB particle `.gtex` assets.
- Engine fallback/PBR validation textures: cooked into `engine/resources`.
- HDR environment textures: still use `EnvironmentResourceManager` and
  `stbi_loadf`; deferred to a future cooked environment format.
- Screenshot PNG writing: output-only path, not an imported runtime asset.
- Benchmark-generated texture paths: remain generated/runtime-specific and are
  not migrated to source-authored `.gtex` assets in this phase.

## Files Added

- `engine/modules/rendering/core/assets/ImageAssetImporter.h/.cpp`
- `engine/modules/rendering/core/assets/TextureCooker.h/.cpp`
- `engine/modules/rendering/core/assets/TextureAssetLoader.h`
- `engine/cmake/CookEngineTextures.cmake`
- `engine/docs/asset-pipeline-phase5-implementation-report.md`
- Engine cooked texture resources under `engine/resources/**/*.gtex`

## Files Changed

- Cooked asset contracts and serializers:
  - `AssetTypes.h`
  - `AssetSerializers.h/.cpp`
  - `AssetCooker.h/.cpp`
- Runtime loading/upload:
  - `RuntimeAssetPolicy.h`
  - `MaterialAssetLoader.h`
  - `TextureManager.hpp`
  - `VulkanTexture.hpp/.cpp`
- Tooling/build:
  - `tools/assetc/assetc.cpp`
  - `engine/modules/rendering/CMakeLists.txt`
  - `engine/modules/rendering/backend/vulkan/CMakeLists.txt`
  - `engine/applications/CMakeLists.txt`
  - `cmake/CookDungeonAssets.cmake`
- Tests:
  - `AssetSerializationTest.cpp`
  - `CookedAssetPipelineTest.cpp`
  - `RuntimeAssetPolicyTest.cpp`
- Documentation:
  - `engine/docs/cooked-asset-pipeline.md`

## Architecture Decisions

- `TextureAssetData`, `TextureMipData`, and `TextureSamplerDesc` are CPU-only
  and contain no Vulkan handles, descriptor sets, ECS entities, or runtime
  texture IDs.
- `.gtex` uses the same cooked file header strategy as `.gmesh`, `.gmat`, and
  `.gmodel`, with explicit fields and validated offsets.
- Phase 5 emits uncompressed `RGBA8_SRgb` and `RGBA8_UNorm` only. Compressed
  enum values exist for future serializers/loaders but are not accepted by the
  Vulkan upload path yet.
- Color space is role-driven. Base color, emissive, UI, and particle textures
  cook as sRGB. Metallic/roughness, normal, AO, font atlas, and data textures
  cook as linear.
- The same source texture used with conflicting roles emits
  `TEXTURE_COLOR_SPACE_CONFLICT`; the cooker writes separate role-specific
  cooked variants.
- Sampler metadata is stored in `.gtex` for this phase. Runtime `.gtex` cache
  keys are texture-path based, and cooked sampler metadata drives upload.
- Development fallback remains available for PNG/JPG/JPEG with explicit
  `TEXTURE_RUNTIME_SOURCE_FALLBACK` diagnostics. Strict/shipping policy rejects
  missing cooked textures.

## Deviations

- `TextureAssetData::dependencies` uses `AssetReference`, matching the existing
  cooked mesh/material/model dependency table. The architecture sketch named
  `AssetDependency`; this implementation keeps the cooked dependency contract
  consistent across asset types.
- Build integration is deterministic but simple. It recooks copied resources
  during the post-build script and does not implement stale-asset detection or
  an asset database.
- HDR environment cooking is not included. The current HDR path performs
  environment preprocessing and is intentionally left as a special runtime
  source path.
- Vulkan-device runtime tests for actual `.gtex` upload were not added. The
  upload path compiles, and CPU serializer/cooker/material integration tests
  cover the cooked boundary.

## Application Migration

- DungeonCrawler post-build asset cooking now emits `.gtex` for copied runtime
  resources. Validation produced 99 `.gtex` files under `build/src/resources`.
- Engine sample resources now have `.gtex` companions in `engine/resources`.
- Engine sample applications depend on `cook_engine_textures` when sample apps
  are built with `GTS_BUILD_TEST_SCENES=ON`.
- Existing application code can still name PNG/JPG paths. `TextureManager`
  prefers adjacent `.gtex`, so migration does not require broad source path
  rewrites.

## Remaining Runtime Source Image Paths

- PNG/JPG/JPEG fallback through `TextureManager` in development policy.
- HDR environment loading through `EnvironmentResourceManager`.
- Screenshot writing through `stbi_write_png`.
- Generated benchmark textures and other output/generated images.

## Validation

- Built `assetc` and `DungeonCrawler` in the game build tree.
- Ran `cmake -DASSETC=... -DRESOURCE_DIR=build/src/resources -P
  cmake/CookDungeonAssets.cmake`.
- Configured a fresh engine test build in `/tmp/gravitas_phase5_apps` and ran:

```text
ctest --test-dir /tmp/gravitas_phase5_apps -R 'asset_serialization|cooked_asset_pipeline|runtime_asset_policy' --output-on-failure
```

Result: all 3 tests passed.

## Known Limitations

- No BC/ASTC/ETC compression yet.
- No `.gtex` support for HDR/cubemap/environment products yet.
- No asset database, dependency graph, or stale-asset detection.
- Cooked sampler state is texture-owned rather than material-binding-owned.
- Runtime fallback diagnostics are logged to stderr rather than routed through a
  central structured diagnostics sink.

## Recommended Next Step

Phase 5B should add GPU texture compression and platform-specific texture
format selection. Uncompressed RGBA8 `.gtex` removes runtime source decoding,
but memory footprint and bandwidth are now the largest remaining texture-system
costs.
