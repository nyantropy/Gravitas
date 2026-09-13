# Shared CPU model resources

`assets/runtime/model` defines the synchronous, backend-independent
`gravitas_model_runtime` target:

```text
requestModel(path)
    → GtsModelRegistry
    → GtsModelResource
    → GtsModelHandle
```

A resource is one shared immutable CPU model definition. It is not a model
instance, Vulkan mesh, material-runtime instance or animation player. Geometry
preparation, material realization, pose/palette evaluation and renderer submission
remain downstream and are not performed by this registry.

## Resource and reference API

`GtsModelResource` privately owns a const `GtsModelImportBundle` as its initial
implementation backing. Only the registry can construct a resource, after the
canonical result and bundle validation succeed. No bundle accessor is exposed.

The public queries are:

- `model()`: const canonical model definition, including hierarchy, mesh/material/
  image data, skeleton uses and skin-binding associations.
- `skeletons()`: read-only enumeration of the associated immutable definitions.
- `clips()`: read-only enumeration of skeletal clips.
- `findClip(name, optionalSkeletonUseIndex)`: generic name lookup, optionally
  checking exact compatibility with the model's selected skeleton use.
- `clip(reference)`: resolves a resource-scoped clip reference, returning null for
  a reference owned by another resource.
- `sourcePath()`: normalized source identity for this initial loading policy.

`GtsModelHandle` is `shared_ptr<const GtsModelResource>`. A null handle is invalid;
copying a handle refers to the same immutable entry. There are no packed IDs,
generations or persistent asset identities. Registry identity comes from the
normalized request path, not geometry equality or skeleton compatibility.

`GtsModelClipReference` is a non-owning `(owning resource, clip index)` reference.
Keep the registry or a model handle alive while using it. Its explicit `index()`
can adapt to existing playback APIs, but consumers should resolve it through the
owning resource first. Cross-resource references fail even when indices coincide.
Names match exactly, including case. Missing names fail; duplicate names fail as
ambiguous even if one of them alone matches the requested use. Canonical duplicate
clip names remain intact and can be inspected through enumeration. Compatibility
checks delegate to `validateGtsAnimationClip`; no new matching algorithm exists.
The engine does not assign gameplay meanings such as Idle or Walk.

Static models naturally have no skeletons/clips/bindings. Skinned, animated and
mixed model capabilities use the same resource type. A primary model is required
by this resource; model-less bundle products remain valid importer concepts but
are not loaded as model resources here. Skeleton definition identity and all
model occurrence associations are preserved, without compatibility-based merging.

## Registry lifetime, identity and diagnostics

`requestModel(path)` returns `GtsModelRequestResult` with `succeeded()`, `handle()`
and operation diagnostics. `lookup(handle)` returns the corresponding resource
only if it belongs to that registry. `size()` reports published entries.

Requests become absolute paths, then `weakly_canonical` resolves existing symlinks
and dot segments. Relative/absolute aliases share an entry; symlink aliases use
the target file's directory for relative source dependencies. There is no custom
case folding or hard-link/content deduplication. Different normalized paths remain
different entries, even for byte-identical files and compatible skeletons.

Only successful loads are cached. Errors retain actionable importer/validation
messages and publish no handle or partial resource. Failures are not memoized and
can be retried after repair. Importer warnings survive both initial requests and
cache hits. Diagnostic storage remains on registry request/cache entries, outside
canonical definitions.

The registry strongly retains successful resources until its destruction. There
is no eviction, reference-count-triggered unload, reload or streaming. A retained
handle may safely extend a resource's lifetime beyond registry destruction. A
cached resource remains the original immutable snapshot if its source changes;
this is not a hot-reload mechanism. Requests and lookups are main-thread-only;
there are no asynchronous/pending states or locks.

`GravitasEngine` owns one registry across scene transitions, registers it in
`EngineServiceRegistry`, and supplies `EcsControllerContext::models` during scene
load. The context pointer is used during the call; game code retains resulting
model handles rather than caching frame-context pointers. The registry itself can
be instantiated directly in CPU tests without constructing an engine or GPU.

## Initial loader scope and Task 2

The initial implementation dispatches only `.gltf` / `.glb` to the canonical
importer. The game-facing method remains format-neutral `requestModel(path)`.
OBJ, `.gmesh`, `.gmodel` and future animated cooked formats fail explicitly here.
Existing `MeshManager`, `ModelAssetLoader`, `RuntimeMeshLoading`, `IResourceProvider`
and `MaterialRuntime` retain their previous responsibilities and behavior.

This centralizes the animated development-source exception; it does **not** unify
`RuntimeAssetPolicy`. Source import still occurs regardless of the existing
cooked-only mesh policy, exactly as the prior Yune path did. Task 2 must establish
source/cooked resolution and its relationship to cache identity before claiming
strict-mode support. In particular, a policy change must not accidentally reuse a
cached development-source entry that the new policy would forbid. Logical model
identity versus resolved artifact identity needs to be settled there; this task
does not silently alias source paths and cooked siblings.

## Transitional Yune bridge

Yune supplies its existing source path to the engine registry. `YuneCharacterAsset`
now retains a model handle, its still-temporary prepared geometry/material bridge,
and model-scoped Idle/SlowWalk selections. It owns no canonical import bundle and
calls no importer. Generic clip lookup and compatibility moved into the resource;
Yune's semantic names and gameplay selection remain game-side.

The function-local weak cache is removed. The dungeon scene retains its prepared
Yune bridge for the scene lifetime and passes it to floor/merchant attachment;
test scenes construct their bridge during scene load. Model definitions remain
registry-owned across scene changes. Preparation/material realization are still
performed by game code and may repeat for separate scene bridges until the later
realization task. Mutable playback/pose/palettes remain per occurrence.

## Tests and dependencies

`GtsModelRegistryTest` uses small generated glTF sources and runs without rendering
or Vulkan. It covers static/rigged/animated contents, normalized/symlink aliases,
separate identities, retained warnings, failed-load isolation/retry, strong
registry retention, handle lifetime, scoped clips, ambiguity, incompatible uses
and preserved skeleton associations.

The real-asset `YuneAnimationTest` now loads via a registry and verifies shared
model identity with independent playback, existing deformation/slot behavior and
immutable keys. Generated sources replace tests that previously mutated Yune's
owned bundle: multiple bindings still share one pose; missing required names and
foreign-resource clip selections fail explicitly.

`gravitas_model_runtime` publicly depends on canonical `gravitas_assets` and
privately on `gravitas_gltf_importer`. It has no rendering, geometry-preparation,
material-runtime, animation-evaluation or Vulkan dependency. The engine umbrella
exposes the model service. The production game no longer links the importer
directly; the fixture-based tests keep a test-only importer dependency. Existing
game skinned-preparation/palette/playback links remain until their later migration.

## Ownership-refactor change inventory and verification

Engine additions:

- `engine/modules/assets/runtime/model/GtsModelHandle.h`
- `engine/modules/assets/runtime/model/GtsModelResource.h/.cpp`
- `engine/modules/assets/runtime/model/GtsModelRegistry.h/.cpp`
- `engine/modules/assets/runtime/model/CMakeLists.txt`
- `tests/assets/runtime/GtsModelRegistryTest.cpp`
- This document.

Engine updates: `GravitasEngine.hpp`, `EcsControllerContext.hpp`, module/assets
CMake files, the asset test CMake file, and the architecture index.

Game updates: Yune's `YuneCharacterAsset.h/.cpp`, `YuneAnimationState.cpp`,
`YuneModelPresentation.h`, `YuneDungeonShop.h/.cpp`, spatial/doorway scene setup,
`DungeonFloorController.h/.cpp` and `DungeonTestScene.h/.cpp`. These changes pass
scene-owned preparation state to attachment while definitions stay in the registry.
Production/test CMake, `YuneAnimationTest.cpp` and the game architecture/Yune docs
were updated accordingly. The model source path, source assets, shader code and
Vulkan backend were not changed.

Verification commands:

```sh
cmake --build build --parallel 6
cmake --build engine/build_release --parallel 6
cmake --build /tmp/gravitas-model-domain-cpu --parallel 6
ctest --test-dir build --output-on-failure
ctest --test-dir /tmp/gravitas-model-domain-cpu -L cpu --output-on-failure
ctest --test-dir engine/build_release \
  -R '^(gts_model_registry|runtime_asset_policy|render_lifecycle_ownership|skinned_frame_extraction|asset_serialization|cooked_asset_pipeline|material_runtime_architecture)$' \
  --output-on-failure
```

Both engine and game builds pass. All 8 game tests, all 21 CPU asset/animation tests
and all 7 focused engine regressions pass. The CPU configuration has
`GTS_ENABLE_RENDERING=OFF` and `GTS_ENABLE_VULKAN_BACKEND=OFF`. Source-boundary
searches find no `GtsGltfModelImporter`, `GtsModelImportResult` or
`GtsModelImportBundle` usage in production `src/`.

This change adds no geometry/material realization migration, model-instance
redesign, Vulkan migration, animation orchestration, blending or clothing behavior.
The existing animated rendering path is exercised through unchanged CPU
preparation/pose/palette checks; this ownership task does not claim a new GPU
visual verification.
