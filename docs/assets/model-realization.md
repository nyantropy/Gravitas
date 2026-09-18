# CPU model geometry realization

`assets/realization/model` owns `gravitas_model_realization`. The authoritative
complete-model operation is `realizeGtsModel(GtsModelHandle)`. It accepts the same
immutable resource handle for source and cooked definitions and returns a structured
`GtsModelRealizationResult`: success exposes `shared_ptr<const GtsRealizedModel>`;
failure exposes diagnostics and no partial model.

```text
source → canonical import ───────────────────────┐
                  └→ cooking → cooked storage ──┤
                                               ↓
                         loading / policy / representation selection
                                               ↓
                                       GtsModelResource
                                               ↓
                                    model geometry realization
                                               ↓
                              static and/or skinned CPU profiles
                              + model occurrence associations
```

Loading selects and decodes a model definition. Preparation transforms one mesh
into a concrete geometry profile. Realization interprets complete model associations,
selects preparation per occurrence and shares compatible prepared definitions.
This layer does not realize runtime materials, upload GPU resources or create model
instances. It neither samples animation nor builds poses or skin palettes.

## Output and lifetime

`GtsRealizedModel` retains:

- `model`: the original `GtsModelHandle`, keeping hierarchy, materials, dependencies,
  skeletons, clips and bindings alive without copying them;
- `geometry`: immutable buffer owners with explicit `GtsGeometryProfile::Static`
  or `Skinned`, typed vertex spans, indices, primitive ranges, bounds and metadata;
- `occurrences`: model-node index, geometry index and optional binding/skeleton-use
  indices, all scoped to the retained model.

Consumers use `profile()`, `staticVertices()` / `skinnedVertices()`, `indices()`,
`primitives()`, `bounds()` and `metadata()` regardless of source/cooked backing.
The wrong profile's vertex query returns an empty span. Canonical prepared meshes
also remain available through typed `staticMesh()` and `skinnedMesh()` queries for
existing adapters and detailed per-primitive generation/influence metadata.
`staticMesh()` is absent for cooked geometry; common static buffer queries work
for both. No universal vertex format or inheritance is introduced.

Geometry copies retain their buffer ownership. Spans themselves are non-owning and
must not outlive their geometry/model owner. Canonical meshes own newly prepared
buffers. Cooked geometry uses aliasing shared ownership of the original resource's
`MeshAssetData`; neither vertex nor index arrays are copied. Small range/material
metadata is adapted to the common view. Retaining the resource also preserves
cooked mesh IDs, debug names, dependency/reference tables and directories.

## Profile selection and sharing

Nodes are visited in original numeric model order, including multiple roots and
forward-parent cooked hierarchies. Only nodes with mesh associations emit geometry
occurrences. Hierarchy and local matrices remain in `model->nodes()` and are never
baked into vertices.

The realization key is `(meshIndex, optional skinBindingIndex)`. An absent binding
selects static preparation; a present binding selects skinned preparation. Thus
profile is unambiguously part of the key. Equal keys share one definition. Different
bindings remain distinct even when skeleton compatibility matches. Definitions are
emitted in first-occurrence order; occurrences remain in node order. Lookup-map or
pointer iteration never determines output order.

Canonical static geometry calls `prepareGtsStaticMesh`. Bound geometry calls
`prepareGtsSkinnedMesh` with the selected binding, retaining skin-local joint slots,
reduction metadata, generated normal/tangent flags and warnings. Static and skinned
meshes can coexist. Canonical validation allows an unbound weighted occurrence,
but the existing static profile rejects joint/weight streams; realization reports
that limitation instead of stripping weights or changing preparation policy.

Published model handles can only be created by the validating loading registry.
Realization relies on that immutable boundary for hierarchy, binding, index and
cooked integrity, then applies the existing profile validation. It does not duplicate
canonical or cooked validators. Empty handles, models with no mesh occurrences and
profile failures return errors. Preparation diagnostics retain their codes and
primitive/attribute locations, prefixed with resource identity, node index/name,
mesh index and binding index when applicable. No failed result enters the cache.

## Cooked geometry and materials

A `.gmesh` is already prepared. Realization retains its exact vertex/index storage,
attribute flags, generation flags and bounds. Empty cooked submesh tables retain
the established implicit whole-mesh range convention in the common view; the stored
table is untouched. No normals, tangents, canonical streams or tight bounds are
regenerated.

A `.gmodel` retains its source node order, parent/child relationships and transforms
through the original resource. Repeated nodes referencing one subordinate mesh
share one geometry definition. Material and dependency references and their original
package/mesh directories remain available; no files are resolved during realization.

`GtsRealizedMaterial` is a small value variant:

- `monostate`: unassigned/default;
- `uint32_t`: the original canonical model material slot, without reordering;
- `GtsExternalMaterialReference`: the unchanged cooked `AssetReference` and its
  mesh reference directory.

There are no world-local material handles. Material conversion and texture work
remain separate future responsibilities.

Canonical static bounds use the existing stored-vertex bounds calculation. Skinned
bounds use stored/bind-space positions only; they are not animated culling bounds.
No node, object or world matrix is applied. In particular, preserving a skinned
node's matrix as definition data is not permission to apply it again after skinning.
The existing Yune presentation continues placing skin-deformed reference-space
geometry using the character object/world transform alone.

## Cache and engine service

`GtsModelRealizationCache::realize(handle)` calls the same operation and retains
successful models plus diagnostics. It is synchronous and main-thread-only, with
no eviction, lazy mutations on assets, IO or backend state. Exact shared resource
identity is the key; paths alone cannot identify a particular immutable representation
snapshot. All profile requirements are currently derived from that snapshot, so
there is no additional request configuration. Ownership comparison is used only
for lookup, never for geometry ordering or structural matching.

`GravitasEngine` owns this cache alongside the loading registry and supplies it via
`EcsControllerContext::modelRealizations`. Cache lifetime spans scenes; returned
shared models can outlive the cache. Loading policy still runs before a game requests
realization, so this cache does not bypass strict/source restrictions.

## Existing adapter and Yune

`GtsStaticModelRealization::realizeGtsFlatStaticModel` remains the specialized flat
identity-root-to-v1-mesh adapter used by cooking and legacy mesh loading. It does
not accept a `GtsModelResource` and is not the complete-model authority. Its existing
format/flatness restrictions and consumers are unchanged. Generic realization uses
the same profile preparation directly without imposing those flattening restrictions.

Yune now retains a model handle and shared realized geometry. Production Yune code
no longer prepares meshes or discovers node/mesh/binding geometry associations.
Its temporary rendering adapter iterates realized occurrences and copies each
prepared skinned mesh into the existing by-value `GtsSkinnedModelData::parts`
contract. This one-time presentation copy remains until renderer/model-instance
ownership is addressed; there is no preparation or geometry recreation per frame.

Scalar material conversion, Idle/SlowWalk content selection, playback, palette
snapshots and renderer presentation remain unchanged and outside generic realization.
Yune's material bridge still requires canonical materials and its presentation still
requires skinned parts; those content-specific restrictions do not constrain the
engine realization contract.

## Verification

`GtsModelRealizationTest` runs without rendering/Vulkan. It covers OBJ, generated
animated GLB, static sharing/hierarchy/material order, mixed profiles, different
bindings, influence reduction, warnings/cache identity, cooked zero-copy byte/metadata
preservation, `.gmodel` forward parents/sharing, ownership beyond registry lifetime,
implicit cooked ranges and all-or-nothing profile failure. Existing importer,
preparation, loading and serialization tests remain authoritative at their layers.
`YuneAnimationTest` verifies reuse of the shared realization, presentation geometry
and binding equivalence, 12 parts / 35 ranges, and unchanged independent playback.
