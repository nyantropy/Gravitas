# Asset Pipeline Phase 4 Implementation Report

## Summary

Implemented glTF/GLB as a second source importer feeding the existing
engine-owned import and cooked asset pipeline. The runtime cooked mesh/material
loaders remain source-format independent: GLB-derived assets cook to `.gmesh`,
`.gmat`, and `.gmodel`, then load through the same runtime asset contracts as
OBJ-derived assets.

## Files Added

- `engine/engine/modules/rendering/core/assets/GltfAssetImporter.h`
- `engine/engine/modules/rendering/core/assets/GltfAssetImporter.cpp`
- `engine/engine/modules/rendering/core/assets/ModelAssetLoader.h`
- `engine/tests/assets/GltfAssetImporterTest.cpp`
- `engine/docs/asset-pipeline-phase4-implementation-report.md`

## Files Changed

- `engine/engine/modules/rendering/core/assets/AssetTypes.h`
- `engine/engine/modules/rendering/core/assets/AssetSerializers.h`
- `engine/engine/modules/rendering/core/assets/AssetSerializers.cpp`
- `engine/engine/modules/rendering/core/assets/AssetCooker.h`
- `engine/engine/modules/rendering/core/assets/AssetCooker.cpp`
- `engine/engine/modules/rendering/core/assets/ObjAssetImporter.cpp`
- `engine/tools/assetc/assetc.cpp`
- `engine/engine/modules/rendering/ecssetup/extraction/RenderCommand.h`
- `engine/engine/modules/rendering/ecssetup/extraction/RenderCommandExtractor.hpp`
- `engine/engine/modules/rendering/backend/vulkan/rendering/framegraph/SceneRenderStage.h`
- `engine/tests/CMakeLists.txt`
- `engine/tests/assets/AssetSerializationTest.cpp`
- `engine/tests/assets/CookedAssetPipelineTest.cpp`
- `engine/docs/cooked-asset-pipeline.md`

## Architecture Decisions

- `GltfAssetImporter` implements `IAssetImporter` and outputs only
  `AssetImportResult` CPU data. It does not include Vulkan, ECS, runtime
  handles, cooked serialization, or renderer state.
- The importer is self-contained because the repository did not already contain
  a glTF or JSON parser dependency. It supports the static subset required for
  this phase instead of adding a broad dependency.
- Imported texture data now distinguishes external image references from
  embedded image bytes. The cooker writes embedded images as ordinary texture
  dependency files until `.gtex` exists.
- `ModelAssetData` and `ModelNodeAssetData` represent imported model packages.
  `.gmodel` serialization is versioned with the same header contract as
  `.gmesh` and `.gmat`.
- `AssetCooker` selects the importer by source format first, then requests only
  the capabilities that importer supports. This keeps OBJ working even though
  OBJ does not provide node hierarchy.
- OBJ and glTF use the same cooker path after import. There is no
  source-format branch in cooked mesh/material/model generation.
- `SceneRenderStage` expands meshes with submesh metadata into one prepared draw
  batch per submesh range and draws with `firstIndex`/`indexCount`.

## glTF Support Level

Supported:

- `.glb`
- `.gltf` JSON with data URI or external buffers
- multiple meshes
- multiple triangle primitives
- positions
- normals
- tangents
- vertex colors
- UV0
- indices
- material assignments
- node names
- node parent relationships
- local transforms from matrix or TRS
- external texture URIs
- embedded images from data URI or bufferView
- base color, metallic, roughness, normal, AO, and emissive material inputs
- alpha mode, alpha cutoff, double-sided state
- `KHR_materials_emissive_strength`

Unsupported features emit structured diagnostics:

- required unsupported extensions: fatal `GLTF_UNSUPPORTED_EXTENSION`
- optional unsupported extensions: warning `GLTF_UNSUPPORTED_EXTENSION`
- skins and joints/weights: `GLTF_SKINNING_NOT_IMPLEMENTED`
- animations: `GLTF_ANIMATION_SKIPPED`
- morph targets: `GLTF_MORPH_TARGETS_NOT_IMPLEMENTED`
- cameras: `GLTF_CAMERA_SKIPPED`
- lights: `GLTF_LIGHT_SKIPPED`
- additional UV sets: `GLTF_MULTIPLE_UV_SETS_IGNORED`

## Import Convention

The import convention is documented in `engine/docs/cooked-asset-pipeline.md`:
right-handed coordinates, +Y up, -Z forward, unit scale 1.0, preserved triangle
winding, V-flipped UV0 by default, and negated tangent handedness when V is
flipped. glTF already matches the engine axis convention; OBJ is assumed to be
authored in that convention.

## Data Flow

```text
OBJ
  -> ObjAssetImporter
  -> AssetImportResult
  -> AssetCooker
  -> .gmesh / .gmat
  -> MeshAssetLoader / MaterialAssetLoader
  -> MeshResource / MaterialInstance
  -> Renderer
```

```text
GLB / glTF
  -> GltfAssetImporter
  -> AssetImportResult
  -> AssetCooker
  -> .gmesh / .gmat / .gmodel
  -> MeshAssetLoader / MaterialAssetLoader / ModelAssetLoader
  -> MeshResource / MaterialInstance / ModelAssetData
  -> Renderer
```

Example GLB output:

```text
robot.glb
  -> robot_body.gmesh
  -> robot_head.gmesh
  -> robot_red.gmat
  -> robot_blue.gmat
  -> robot.gmodel
```

## Deviations And Weaknesses

- Runtime `.gmodel` loading exists, but there is not yet an engine-level model
  instantiation service that creates ECS entities from `.gmodel` nodes.
- Cooked submesh material references are preserved in `.gmesh` and
  `MeshResource`, but the current ECS renderer binding has one material handle
  per entity. `SceneRenderStage` emits one draw range per submesh using the
  entity material. Automatic per-submesh material resolution remains future
  work.
- Embedded glTF textures are written back out as ordinary image dependencies.
  They are not cooked, color-space validated, mipmapped, or compressed because
  `.gtex` is Phase 5.
- The glTF importer intentionally covers static assets only. It does not import
  scenes as game scenes, animation, skinning, morph targets, cameras, lights, or
  extra UV sets.
- The importer is a targeted parser rather than a full conformance-layer glTF
  implementation.

## Tests Added Or Extended

- `GltfAssetImporterTest`
  - static GLB import
  - multiple meshes
  - multiple primitives
  - normals, tangents, vertex colors, UV0, indices
  - external textures
  - embedded textures
  - multiple materials
  - alpha modes
  - missing texture diagnostics
  - unsupported feature diagnostics
  - unsupported required extension failure
  - importer registry selection
- `AssetSerializationTest`
  - `.gmodel` round trip
  - `.gmodel` deterministic output
  - invalid model counts and parent indices
- `CookedAssetPipelineTest`
  - GLB -> cook -> `.gmesh`/`.gmat`/`.gmodel`
  - cooked load after removing source GLB
  - submesh material reference preservation
  - deterministic GLB cooking
  - `MeshResource` population without reparsing source

## Verification Run

- `cmake --build /tmp/dc-engine-asset-build --target GltfAssetImporterTest`
- `/tmp/dc-engine-asset-build/tests/GltfAssetImporterTest`
- `cmake --build /tmp/dc-engine-asset-build --target AssetSerializationTest`
- `/tmp/dc-engine-asset-build/tests/AssetSerializationTest`
- `cmake --build /tmp/dc-engine-asset-build --target CookedAssetPipelineTest`
- `/tmp/dc-engine-asset-build/tests/CookedAssetPipelineTest`
- `cmake --build /tmp/dc-engine-asset-build --target ObjAssetImporterTest`
- `/tmp/dc-engine-asset-build/tests/ObjAssetImporterTest`
- `ctest --test-dir /tmp/dc-engine-asset-build -R 'gltf_asset_importer|asset_serialization|cooked_asset_pipeline|obj_asset_importer' --output-on-failure`
- `cmake --build /tmp/dc-engine-asset-build --target assetc gravitas_vulkan_backend`
- `cmake --build build`

## Remaining Direct Source Loads

- `MeshManager::loadMesh` still falls back to OBJ source parsing for
  compatibility when no adjacent `.gmesh` exists.
- Runtime texture loading still decodes PNG/JPG/HDR through `TextureManager`.
- Bitmap font atlas images still load through the texture manager.
- Particle, UI, and shader assets still use their existing source/runtime paths.
- There is no runtime glTF parser path.

## Next Recommended Step

Implement Phase 5 cooked texture assets (`.gtex`) and then add model/package
instantiation that resolves `.gmodel` nodes, mesh references, and material
references into runtime entities without source-format knowledge.
