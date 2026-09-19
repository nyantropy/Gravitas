# Shared CPU model loading

`model/loading` owns the synchronous, backend-independent
`gravitas_model_loading` target. `GtsModelRegistry::requestModel` is the authoritative
entry point for migrated high-level runtime model requests:

```text
model request (identity + required capabilities)
    → engine source/cooked policy
    → compatible representation selection and cache lookup
    → cooked adapter OR canonical source importer
    → validated shared GtsModelResource
    → GtsModelHandle
```

A model resource is an immutable CPU definition, not a model instance, Vulkan
mesh, material-runtime instance or animation player. Loading does not prepare
geometry, realize materials, evaluate animation or create GPU resources.
Legacy `requestMesh`, `MeshManager` and `RuntimeMeshLoading` remain operational.

## Request and capability contract

```cpp
auto ordinary = registry.requestModel(path);
auto character = registry.requestModel(GtsModelRequest{
    path,
    {.geometry = true, .skeletons = true, .skinBindings = true, .animations = true}
});
```

`GtsModelCapabilities` has five independent boolean requirements/observations:
geometry (at least one mesh), hierarchy (at least one parent/child edge), skeleton
definitions, skin bindings and animation clips. False request fields impose no
requirement. `resource.capabilities()` derives observations from actual definition
contents; source extensions do not establish capabilities. Every published entry
must satisfy its request. Missing capabilities cause structured failure.

Policy comes from the shared `assets/loading/RuntimeAssetPolicy.h`, evaluated on
every request. Rendering callers include the CPU-owned header and use `gts::assets` directly;
there is no rendering forwarding header. Mesh cooked-path helpers and
source permission rules are shared; complete models never pass through
`MeshResource` or the static single-mesh realization adapter.

## Source/cooked policy

Existing engine semantics are unchanged:

- Development fallback is the default, including Release configurations.
- `GTS_RUNTIME_ASSET_POLICY=strict`, `shipping`, `cooked-only` or `cooked_only`
  forbids source fallback (case insensitive).
- Compile definitions `GTS_SHIPPING_BUILD` or
  `GTS_DISABLE_RUNTIME_SOURCE_ASSET_FALLBACK` forbid fallback regardless of the
  environment. There is no per-model override that can weaken this restriction.

Supported direct inputs: `.obj`, `.gltf`, `.glb`, `.gmesh`, `.gmodel`. Source
formats use the canonical `GtsObjModelImporter` and `GtsGltfModelImporter` inside
the model-runtime module. No legacy glTF DTO bridge is used.

Selection is deterministic:

1. Normalize the requested identity. Reject unknown input types.
2. Evaluate current policy before considering cached source data.
3. For source requests, inspect adjacent `.gmodel`, then `.gmesh`. An explicit
   cooked request selects only that artifact and never falls back to a source.
4. Reject candidates that cannot preserve the requested definition/capabilities.
   Reuse a satisfying cached cooked entry or load/validate the selected candidate.
5. If no compatible cooked candidate remains, source is allowed only under the
   current development policy. Reuse a satisfying cached source snapshot or
   import/validate it.
6. Publish only a fully valid, capability-satisfying resource.

**V1 fidelity limitation:** current cooked formats are static and have no manifest
attesting full fidelity to a glTF source. They cannot automatically substitute for
any `.gltf`/`.glb` model request, even one with default requirements: doing so could
silently lose skins, animation, helper hierarchy or other authored data. Such
requests retain the canonical source definition in development and fail in strict
mode until compatible cooked support exists. A caller explicitly requesting a
`.gmodel`/`.gmesh` requests that static definition. This is conservative resolution,
not a claim that all GLBs contain animations.

Static OBJ requests prefer an adjacent compatible `.gmodel`, then `.gmesh`, using
the existing static cooked-profile contract. Skeleton/skin/animation requirements
exclude both v1 formats. A hierarchy requirement excludes flat `.gmesh`; actual
`.gmodel` contents are checked before acceptance. No source parser is run merely
to guess whether a cooked artifact might be safe in strict mode.

Missing candidates allow permitted source fallback. A selected, potentially
compatible cooked artifact that fails decoding, dependency loading or validation
fails the request; it does not fall back to source. Known-incompatible artifacts
are skipped without decoding them. Filesystem errors are explicit diagnostics.

## Definitive CPU definition, with two geometry representations

The resource is no longer synonymous with the importer bundle. Its private backing
is exactly one of:

- const `GtsModelImportBundle`, preserving full canonical source semantics;
- const `GtsPreparedModelDefinition`, preserving already-prepared static geometry.

Common queries are `identityPath()`, `nodes()`, `rootNodes()`, `meshCount()`,
`capabilities()`, `skeletons()` and `clips()`. The explicitly named read-only
`canonicalModel()` and `preparedModel()` views return a pointer to the applicable
geometry representation and null for the other. The old canonical-only `model()`
accessor was replaced to make that assumption visible. The public request/handle
remains the same for either representation; only later preparation/realization
consumers need to interpret geometry storage. No raw import-bundle accessor exists.

### `.gmesh`

The existing v1 codec supplies `MeshAssetData`: concrete `GtsStaticVertex` values,
indices, submesh ranges/material references, bounds, attribute/generation metadata
and dependencies. The adapter retains those fields and creates one identity-root
mesh occurrence. It does not invent source vertex streams, recompute normals,
reprepare the mesh or claim skeleton/animation/hierarchy capabilities. An empty
submesh table remains the original implicit whole-mesh range convention.

Codec validation checks binary/count/index/range integrity. Model loading adds
finite vertices, nonempty indexed triangle geometry, triangle range coherence and
finite ordered bounds when present. Malformed data is rejected without repair.

### `.gmodel`

The existing package codec supplies node names, parents, local matrices, mesh and
material references and dependencies. The adapter retains node order, builds
children/root tables, loads listed subordinate `.gmesh` definitions and maps node
references to that mesh table. Multiple nodes can share one mesh entry; it does not
flatten hierarchy or bake transforms into vertices. Forward parent references are
supported; invalid parents, cycles, non-finite/non-affine matrices and unlisted
node mesh references fail explicitly. Singular affine transforms remain valid.

Mesh references resolve relative to the actual package directory. ID-only mesh
references fail with an actionable unsupported-resolution diagnostic; no asset
resolver is invented. Material and other dependency references remain unresolved
CPU association data, including each mesh's own dependencies. Package and mesh
reference directories are retained so later realization can resolve paths correctly,
including when a cooked artifact is reached through a symlink. Missing/corrupt
subordinate meshes fail the whole request. Material/texture realization is deferred.

Serializers and storage contracts live under `model/serialization` for model hierarchies and `assets/serialization` for shared mesh/material/texture data; CPU
loaders live beside their owning model/shared asset phases. `gravitas_cooked_assets` owns the
codecs and links CPU core only. Legacy type namespaces remain unchanged, but no
renderer runtime header or Vulkan library is needed. Material runtime realization
is separate under `rendering/core/material/MaterialAssetRealization.h`. No serialized
layout, cooked version or byte interpretation changed. See [asset ownership](architecture.md).

## Identity, cache, lifetime and diagnostics

A requested path becomes absolute then `weakly_canonical`: dot segments and
existing symlink aliases converge, with no custom case folding, hard-link or
content deduplication. Distinct normalized requests remain distinct identities;
requesting `crate.obj` and `crate.gmesh` does not invent a persistent alias between
them. `identityPath()` reports the request identity, not whichever artifact won.

Internally, entries distinguish request identity, normalized representation path
and source/cooked provenance. One identity may retain multiple immutable
representations. New requests reconsider preferred artifacts and requirements;
source-to-cooked selection may therefore return a different handle with the same
identity path after a compatible cooked artifact becomes available. Old handles
remain valid. Provenance is not exposed as gameplay model identity.

A strict request cannot reuse a cached source entry. A failed strict or
capability-required request does not remove a valid development entry. Compatible
cached cooked snapshots remain reusable, including after the backing file changes
or is removed: this is stable resource retention, not hot reload. Warnings from
loading are stored with entries and returned on cache hits; resolution warnings
are computed for the current request. Failures expose diagnostics and no handle,
are not cached, and can be retried after repair.

`GtsModelHandle` is `shared_ptr<const GtsModelResource>`. The registry retains all
successful entries until destruction; handles can extend that lifetime. There are
no jobs, locks, eviction, streaming or pending handles. Requests are main-thread-only.
`lookup(handle)` checks ownership by this registry, not structural equality.

`GravitasEngine` owns the registry across scene transitions and supplies it through
`EcsControllerContext::models`. Callers retain handles, not frame-context pointers.

## Scoped clip discovery

`findClip(name, optionalSkeletonUseIndex)` returns a non-owning resource-scoped
`GtsModelClipReference`. Names match exactly; missing or duplicate names fail.
Compatibility delegates to `validateGtsAnimationClip`. `clip(reference)` rejects
foreign-resource references even if the numeric index or skeleton structure matches.
Keep a handle or the registry alive while using a clip reference. Static cooked
resources enumerate no clips/skeletons and fail clip lookup cleanly.

Skeleton compatibility remains separate from identity; no compatible definitions
or occurrences are merged. Engine loading assigns no Idle/Walk gameplay meaning.

## Yune and later realization

Yune keeps its content path and requests geometry + skeletons + skin bindings +
animations through the ordinary engine policy. Development fallback loads the
animated merchant GLB. Strict policy fails explicitly; the old static merchant
artifact cannot satisfy this request. No Yune-only source-permission exception
remains. Required game clip names remain `Yune · quiet idle` and `Yune · slow walk`.

`YuneModelConfiguration` retains content/clip policy and temporary renderer setup.
[Generic model instances](../model/runtime-instances.md) retain shared realization
references and own playback, poses and palettes per skeleton use. Generic geometry work now belongs to
[model realization](model-realization.md), which prepares canonical geometry once
per mesh/profile/binding and references cooked prepared buffers directly. It preserves
hierarchy, materials and reference directories without introducing runtime material
or GPU state. [Model material realization](../model/model-material-realization.md)
resolves canonical or cooked material associations generically; Yune no longer
interprets canonical appearance data. Resource `skeletonUses()` and `skinBindings()` queries expose shared associations
without requiring canonical-backing inspection.

Full cooked glTF substitution still needs a future fidelity/capability contract;
geometry realization alone cannot make current static v1 a safe substitute.

## Verification

CPU tests cover all source formats, both cooked adapters, hierarchy/dependencies,
capabilities, malformed data, missing/corrupt selection, warnings, identity and
provenance-aware cache reuse. A separate strict-policy test also runs in a build
with source fallback disabled at compile time. The legacy mesh-policy test directly
exercises OBJ fallback, cooked preference, strict loading and corrupt/missing cases.
Game integration tests exercise the existing cube OBJ and real animated Yune through
the same request API, strict rejection after source cache warmup, clip discovery
and independent playback. See the task report for commands and final results.
