# Asset Pipeline Phase 4.5 Migration Report

## Inventory

Required root-level documents named in the task were not present in this
workspace. The current asset documents are under `engine/docs/` and were used as
the authoritative source.

Maintained runtime applications and scenes inspected:

- `DungeonCrawler`
- `EditorVisualEvaluationScene`
- `CombatTestScene`
- `DungeonTestScene`
- `RoomVisualTestScene`
- `YuneSpatialTestScene`
- engine rendering benchmarks and particle preview helpers
- engine asset/import/cooked pipeline tests

Runtime source mesh usage before migration:

- Game cube mesh references used `GamePaths::model("cube.obj")`.
- Dungeon decoration catalog entries used source OBJ paths under
  `resources/assets/**`.
- Particle descriptors could carry `meshPath` values such as
  `resources/models/cube.obj`.
- Engine benchmark and particle preview helpers still used the engine cube OBJ.
- `MeshManager::loadMesh` silently preferred adjacent `.gmesh` when present and
  silently fell back to source OBJ otherwise.

Runtime source mesh usage after migration:

- Maintained game code now names `cube.gmesh` directly for cube renderables.
- Dungeon decoration catalog model paths now name `.gmesh` files directly.
- `GamePaths::model()` and `GamePaths::asset()` still rewrite `.obj`, `.gltf`,
  and `.glb` inputs to `.gmesh` for compatibility with older game code.
- Engine sample scenes, PBR validation, rendering benchmarks, and particle
  authoring tests now use `engine/resources/models/cube.gmesh`.
- The remaining source-format references are importer tests, cooked-pipeline
  tests, runtime-policy tests, and the isolated legacy `GtsModelLoader` fallback.

Runtime glTF usage before migration:

- No maintained runtime application or scene loaded glTF/GLB directly.
- glTF/GLB usage was limited to importer and cooked-pipeline tests.

Direct PNG/JPG usage:

- Game UI, sprites, dungeon textures, item icons, particle textures, and
  authored material texture paths still use PNG/JPG.
- This remains expected until Phase 5 because `.gtex` does not exist yet.

Manual runtime material construction:

- Game scenes and spawners commonly use `UnlitMaterialDescriptor` plus
  `sharedUnlitMaterialReference`.
- World text, debug draw, selection tools, particle previews, and tests still
  construct runtime materials directly.
- `MaterialReferenceHelpers` already supports `.gmat` paths, but application
  authored content was not using that path by default.

Manual mesh/resource creation:

- Static authored meshes flow through `StaticMeshComponent` and `MeshManager`.
- Procedural/dynamic meshes still upload CPU-generated geometry directly. These
  are runtime geometry paths, not source asset import paths.

One-material-per-entity assumptions:

- `MaterialReferenceComponent` stores one entity material.
- Phase 4 preserved submesh material references and draw ranges, but runtime
  extraction and rendering still used the entity material for every submesh.

Cooked asset usage before migration:

- Cooked `.gmesh`, `.gmat`, and `.gmodel` were exercised by asset tests.
- The maintained game did not automatically cook or reference cooked assets.

Cooked asset usage after migration:

- The game target builds `assetc` and runs a post-build cook step for copied
  `resources/models` and `resources/assets` source meshes.
- Runtime resources under `build/src/resources` contain `.gmesh` and `.gmat`
  outputs next to the copied source files.
- `engine/resources/models/cube.gmesh` and `cube_default.gmat` are committed so
  engine samples can use cooked assets without a runtime OBJ parse.

## Implementation

- Added `RuntimeAssetPolicy.h` with development fallback and cooked-only modes.
  Development mode logs missing cooked meshes before falling back to source
  assets. Cooked-only mode rejects missing cooked meshes with a clear error.
- Updated `MeshManager` to prefer `.gmesh`, emit fallback diagnostics, and obey
  `GTS_RUNTIME_ASSET_POLICY=strict` or compile-time shipping/disable macros.
- Added `cmake/CookDungeonAssets.cmake` and wired `DungeonCrawler` post-build
  cooking through `assetc`.
- Added `StaticMeshComponent::useMeshMaterials`. Direct mesh usage defaults to
  entity-level material overrides for visual parity. When enabled, cooked
  submesh material references resolve to runtime `MaterialInstanceHandle`s.
- Extended `IResourceProvider`, `RenderResourceManager`, `MeshGpuComponent`,
  extraction snapshots, render commands, and `SceneRenderStage` so submesh draw
  ranges can use distinct runtime materials.
- Updated game scenes, dungeon decoration catalog data, engine sample
  applications, engine rendering benchmarks, and particle authoring tests to
  reference cooked mesh paths.

## Visual Parity Notes

No automated screenshot comparison exists for the maintained game scenes in this
phase. Validation is limited to build/test coverage and draw-path regression
tests.

Expected visual behavior is unchanged for existing direct mesh usage because
`StaticMeshComponent::useMeshMaterials` defaults to `false`, preserving existing
entity-level material overrides. Multi-material cooked meshes can opt into
per-submesh material selection by enabling that flag.

The build-time cooker generated cooked assets successfully for the maintained
game resources. The legacy source files are still copied for development
fallback and tooling input.

## Remaining Legacy Paths

- Runtime PNG/JPG/HDR decoding remains until `.gtex`.
- Procedural/dynamic meshes still upload runtime geometry.
- Engine tool previews and some tests intentionally exercise manual material
  construction.
- Many game materials still use `UnlitMaterialDescriptor` with explicit
  PNG paths. This preserves current visuals until Phase 5 introduces cooked
  texture assets and resolves material texture dependencies as engine assets.
- Cooked `.gmat` texture references are durable asset references, but the
  runtime still ultimately resolves them through `TextureManager` source image
  loading. Direct game-side replacement of all PNG-backed materials was deferred
  to avoid changing texture-path semantics before `.gtex`.
- Source OBJ fallback remains available in development policy, but it is logged
  and can be disabled with `GTS_RUNTIME_ASSET_POLICY=strict`.
- Missing cooked `.gltf`/`.glb` requests fail even in development because there
  is no runtime source glTF loader.
- `GtsModelLoader` remains only as the development fallback bridge from source
  OBJ to the existing runtime mesh upload path.
- `.gmodel` has a serializer/loader, but maintained game scenes currently load
  single-mesh assets directly. No game-side multi-mesh model instantiation was
  required in this migration pass.

## Validation

- `cmake --build /tmp/dc-engine-asset-build --target MaterialRuntimeArchitectureTest RuntimeAssetPolicyTest`
- `ctest --test-dir /tmp/dc-engine-asset-build -R 'runtime_asset_policy|material_runtime_architecture' --output-on-failure`
- `ctest --test-dir /tmp/dc-engine-asset-build -R 'obj_asset_importer|gltf_asset_importer|asset_serialization|cooked_asset_pipeline' --output-on-failure`
- `cmake --build build --target DungeonCrawler`
- `ctest --test-dir build --output-on-failure`
- `cmake --build /tmp/dc-engine-app-build --target GtsScene1 GtsScene2 GtsScene3 GtsPbrValidation GtsRenderingBenchmarks`

The `DungeonCrawler` build produced cooked `.gmesh` and `.gmat` outputs in
`build/src/resources`.

## Next Step

Phase 5 should replace runtime PNG/JPG material texture dependencies with
cooked `.gtex` assets and then tighten shipping-mode policy around texture
source decoding.
