# Model material realization

`rendering/core/material/model` owns the frontend target `gravitas_model_materials`.
It is available without rendering/backend libraries so material semantics and
association tests can run with rendering and Vulkan disabled. Asset loading and
geometry realization have no dependency on it.

```text
GtsModelResource + GtsRealizedModel
                    |
     GtsModelMaterialRealization (one MaterialRuntime/resource-provider scope)
                    |
       GtsRealizedModelMaterials
                    |
        MaterialInstanceHandle
                    |
       existing rendering/material backend
```

## API and ownership

Use `modelMaterialRealization(world, resources).realize(realizedModel)` during
setup. Standalone frontend tests can construct
`GtsModelMaterialRealization(runtime, resources)` directly. The realized model
retains its `GtsModelHandle`; no separate canonical/cooked choice is exposed.
The result contains either a complete shared material set or contextual
`GtsModelDiagnostic` errors. Shared model/geometry objects remain immutable and
contain no runtime handles.

`materialFor(owner, geometryIndex, primitiveIndex)` returns the handle for a
logical primitive association. The exact realized-model owner scopes these
indices; foreign owners, invalid ranges and expired/destroyed handles return an
invalid handle. `frameStateFor` provides an existing `MaterialFrameState` snapshot
for temporary presentation adapters. Snapshots use the same semantic mapping as
`MaterialRuntime::synchronizeGpuState`; they do not constitute a second material
conversion.

The ECS singleton owns the service, and the existing world `MaterialRuntime`
owns definitions/instances. A weak runtime lifetime token invalidates material
sets on runtime reset/destruction, even if external references retain the sets.
The service is recreated after reset. Its resource provider is non-owning and
must remain valid throughout that scope. Use the renderer's existing world/resource
reset ordering; independent texture eviction or provider replacement does not
refresh snapshots held by a presentation. Access remains synchronous/main-thread.

Successful associations are retained for the service/world lifetime. Canonical
handles are keyed by resource ownership and numeric material slot. Distinct slots
stay distinct even when their values match. External references use asset ID plus
normalized resolved path. Repeated primitive use and equivalent realizations of
one resource reuse handles. Cache hits verify handle generations/liveness.
Unassigned ranges use the existing runtime-owned `defaultMaterial()`; no per-model
or per-primitive default is allocated.

## Canonical interpretation

The adapter fills the existing `MaterialInstance` contract and allocates via
`MaterialAssetRealization::createInstance`, shared with cooked realization.
Canonical materials use `StandardSurface`. It preserves base color, metallic,
roughness, emissive factor/strength, normal scale, AO strength, alpha mode/cutoff
and double-sided state. Blend disables depth writing using the existing policy.
Existing material parameter sanitization remains authoritative. Material names
are diagnostic data, never cache identity.

External `GtsModelImage` paths use their imported absolute source location;
embedded encoded bytes decode in memory. `assets/importer/image/GtsImageDecode`
provides the single stb-backed decoding wrapper, also used by legacy image import.
There are no temporary image files or additional decoder implementations.

Base-color/emissive textures use sRGB; normals and scalar maps use linear data.
`IResourceProvider::requestMemoryTexture` accepts immutable decoded RGBA8 pixels.
The Vulkan texture manager caches image ownership plus color space and uses its
existing CPU texture upload description/constructor and descriptor path. Source
images shared between color/data roles receive distinct texture interpretations.
The initial memory route uploads one mip with existing default sampler behavior.
Cooked texture metadata must match the requested role's color space; incompatible
requests fail instead of reusing an incompatible cached texture.

Every present binding must use UV0. UV1 is explicitly rejected, never remapped.
Canonical scalar inputs stay independent upstream. Shared
`assets/processing/image/GtsScalarImagePacking` performs channel extraction:

- metallic → blue and roughness → green in the MR texture;
- AO → red in the existing separate AO texture;
- absent channels → 255.

The renderer already has separate MR/AO bindings; no new combined ORM shader ABI
is introduced. MR sources must have equal dimensions; no implicit resampling
occurs. AO dimensions may differ because AO has a separate texture. The cooker
uses the same extracted packing helper with unchanged output semantics. Decoded
images, packed source/channel combinations and role-specific texture IDs are
cached within the service scope.

## Cooked associations

`GtsExternalMaterialReference.referenceDirectory` resolves the retained material
`AssetReference`. `MaterialAssetLoader` decodes `.gmat`; `MaterialAssetRealization`
creates the same `MaterialInstance` semantic input. Texture paths resolve relative
to the material file, then use the normal resource-provider texture path and its
existing source/cooked policy. No cooked-to-canonical conversion or in-memory fake
serialization is performed. ID-only references fail explicitly because there is
no ID-to-path resolver in this runtime route yet.

Semantic conversion and canonical image IO are staged before material allocation.
Cooked texture paths resolve through existing runtime synchronization. Any
allocation or texture failure rolls back newly created instances/definitions;
no partial material set is published. Successfully decoded/uploaded texture
resources may remain in their existing caches after a failed request for reuse.
Failures remain retryable. Diagnostics identify model identity and logical slot
or external reference. Importer warnings remain upstream; they are not repeated.

## Presentation and limits

`GtsModelSkinnedPresentation` is a temporary adapter into the unchanged by-value
`GtsSkinnedModelData`: it copies prepared parts and snapshots unique referenced
material handles. It does not interpret canonical factors or images. Static and
mixed-profile models use the same material service; this particular skinned
presentation adapter still requires all its occurrences to be skinned.

Alpha mask/blend and double-sided state survive material realization. Existing
renderer limitations remain, including the current skinned bridge's opaque,
depth-writing draw restriction. Material realization does not implement sorting
or pipeline variants. Yune no longer requires canonical backing for appearance;
its existing clip/pose bridge still uses the canonical skeleton-use contract.

No model-instance, overrides, live presentation-refresh policy, cooker cutover,
new cooked formats, animation behavior or Vulkan material/shader redesign belongs
to this layer.

## Verification

`GtsModelMaterialRealizationTest` exercises actual runtime instances with a fake
resource provider: factors/render state, external/embedded images, colorspaces,
channel packing, texture reuse, cooked path resolution and errors, UV/dimension
rejection, distinct equal-value slots, mixed geometry profiles, scoped lookup,
world reset/lifetime and transactional failures. It is part of the headless asset
test suite. Existing material runtime, serialization/cooking and Yune tests cover
the shared helper extraction and production adapter.
