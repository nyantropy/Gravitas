# Static Geometry Preparation

`engine/modules/model/processing/geometry/static/` owns the standalone CPU target
`gravitas_static_geometry`. It consumes one canonical mesh, independently of any
source file, importer, material realization, or resource manager:

```text
GtsModelMesh (immutable, primitive-local semantic streams)
====================== FORMAT WALL ======================
prepareGtsStaticMesh(const GtsModelMesh&)
    -> GtsStaticMeshPreparationResult
    -> GtsPreparedStaticMesh
        vertices: current static GtsStaticVertex layout
        indices: shared, rebased uint32 indices
        primitives: ordered ranges, material indices, per-primitive metadata
        metadata: whole-mesh summary
```

## Contract and ownership

`GtsPreparedStaticMesh` owns its name, `GtsStaticVertex` vertices, indices, primitive records, and
`MeshGeometryMetadata`. `GtsPreparedStaticPrimitive` stores `firstIndex`,
`indexCount`, an optional canonical `materialIndex`, and local geometry metadata.
Primitive records retain canonical order, including A/B/A material runs. There
is no invented primitive name because canonical primitives do not have names.
The canonical mesh name is copied.

Each primitive is validated and prepared independently before concatenation.
Indices are rebased by the preceding vertex count, while range offsets use the
preceding index count. Empty canonical index arrays select sequential vertices;
the prepared result materializes that sequence. Vertices are not merged between
primitives, and neither boundaries nor material assignments are resolved away.

Material indices still refer to the containing `GtsModelAsset::materials`; this
mesh-only API cannot check material bounds or material image UV requirements.
Those checks belong to canonical model validation. Preparation accepts no material
runtime, image inputs, source paths, or source-format options.

`GtsStaticMeshPreparationResult` exposes `succeeded()`, `mesh()`, `diagnostics()`,
and `hasWarnings()`. Failure always has errors and no prepared mesh. Warnings can
accompany success. There is no public default/partially successful result state.
Diagnostics reuse `GtsModelDiagnostic` and identify primitive/attribute locations.

## Static-profile policies

| Canonical stream | Prepared behavior |
| --- | --- |
| Position[0] | Required; copied exactly |
| Normal[0] | Copied exactly when present; otherwise generated |
| Tangent[0] | Copied exactly when present; otherwise generated when UV0 is present |
| Color[0] | Copied exactly, otherwise white RGBA in prepared vertices only |
| TexCoord[0] | Copied exactly, otherwise zero in prepared vertices only |
| Additional non-skinning sets | Ignored with `STATIC_ATTRIBUTE_IGNORED` warning for each stream |
| Joints / Weights, any set | `STATIC_SKINNING_UNSUPPORTED` error, even if all values are zero |

Only triangle-list primitives are accepted; points and lines fail with
`STATIC_TOPOLOGY_UNSUPPORTED`. All canonical primitive validation runs before
typed conversion, including on ignored streams. Missing positions, incorrect
types/counts, duplicate streams, invalid indices, nonfinite values, and malformed
topology fail rather than entering the legacy repair routines. Combined vertex
and index counts must fit uint32; overflow fails before allocation/rebasing.
An empty canonical mesh yields an empty prepared mesh with zero/absent metadata.

The converter has no OBJ V flip or other source-specific interpretation. It never
generates canonical streams, changes the input, or implies that prepared white
colors/zero UVs were authored by the source.

## Algorithm reuse and metadata

The shared `GtsPrimitiveGeometryPreparation` helper calls the existing CPU-only,
header-only
`gts::rendering::prepareMeshGeometry` once per primitive. Its default initialization,
area-weighted normal accumulation, tangent generation, handedness, degeneracy
thresholds and finite fallbacks are shared with direct/procedural geometry.

The existing tangent generator normalizes its working normals. After it returns,
the adapter restores any authored canonical normals exactly; tangent generation
still uses the established normalized working frame. Authored tangents are left
unchanged by the existing presence flags. Missing normals use the established
positive-Z fallback for degenerate geometry.

With no UV0 and no authored tangent, prepared tangents remain the existing
`{1,0,0,1}` default, `generatedTangents` is false, and the Tangent flag is absent.
With UV0 present but degenerate, the existing generator produces a finite tangent
orthogonal to its working normal. Its metadata sets `generatedTangents = true`,
including when all triangles use fallback. Thus “generated” records execution of
the existing generation path, not proof of nondegenerate UV derivatives. The same
principle applies to generated normals and their finite fallback.

Each primitive's metadata reports:

- Its local vertex and index counts.
- Attribute flags for copied static-profile streams plus generated normals/tangents.
  Default white Color and zero UV0 fields do not add availability flags.
- Whether its normals/tangents were generated.

Whole-mesh metadata reports total counts, the **intersection** of primitive
attribute flags, and the **OR** of primitive generation booleans. This is a
conservative summary, not permission to discard richer primitive data. For example,
`generatedTangents` can be true while the mesh Tangent flag is absent if one
primitive generated tangents and another had no UV0. Per-primitive metadata remains
authoritative for future consumers needing individual capabilities.

## Bounds and dependencies

The prepared type does not own bounds. Downstream realization and cooking compute
mesh-local `AssetBounds` from prepared positions using the CPU queries in
`assets/serialization/MeshAssetGeometry.h`. This keeps storage/runtime headers out
of preparation and avoids a dependency on cooking.

The preparation target links `gravitas_primitive_geometry`, which links
`gravitas_model_domain` and `gravitas_asset_contracts`. Shared CPU algorithms,
`GtsStaticVertex.h` and `GtsGeometryMetadata.h` live in `modules/assets/geometry/`. The processor
includes these CPU profile headers without a renderer/backend link. No linking
to `gravitas_rendering`, GLFW, Vulkan, TinyOBJ, or the importer is required.
Primitive stream conversion is shared with the separate
[skinned profile](skinned-geometry.md); geometry math remains unchanged. The module
location preserves the rule that core domain headers must not depend on module
contracts.

The target is available with rendering disabled. Canonical OBJ/glTF cooking and
model realization consume static preparation. Direct OBJ mesh loading uses the
flat-model adapter; procedural geometry can use shared geometry algorithms directly.

## Tests

`tests/assets/processing/geometry/static/GtsStaticMeshPreparationTest.cpp` constructs canonical
fixtures directly and links only the preparation target. It covers copied streams,
normal/tangent generation, finite fallbacks, mixed per-primitive availability,
indexed and sequential rebasing, ranges/materials, exact authored-data retention,
input immutability, ignored extra sets, rejected skinning/topology, malformed
canonical inputs, and per-primitive/whole-mesh metadata.

With the backend-free configuration documented in [model-domain.md](model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --target GtsStaticMeshPreparationTest --parallel 2
ctest --test-dir /tmp/gravitas-model-domain-cpu -R '^gts_static_mesh_preparation$' --output-on-failure
```

## Explicit prepared vertex profiles

```text
GtsVertexAttribute semantic streams (canonical)
================ FORMAT WALL =================
static preparation  -> GtsStaticVertex
skinned preparation -> GtsSkinnedVertex
```

`GtsStaticVertex` is the concrete static layout with member defaults and
component-wise GLM equality. The current GLM configuration yields size 64,
alignment 4, and offsets `pos=0`, `normal=12`, `tangent=24`, `color=40`, `texCoord=56`.
CPU tests freeze this ABI and existing Vulkan tests verify matching stride,
locations, offsets and formats. Cooked-v1 serialization still writes the same
individual floats; its pre-rename 407-byte triangle fixture is protected by a
golden fingerprint and full vertex round-trip checks.

`MeshResource`, `DynamicMeshComponent`, dynamic mesh resource APIs and
`VulkanStaticVertexDescription` still describe this static profile. Procedural meshes,
world text, debug geometry, previews and canonical model cooking now
spell their existing output `GtsStaticVertex`; none are migrated through canonical
model assets. Skinned model instances use a distinct prepared profile and Vulkan resource path;
these direct/procedural APIs remain static-profile APIs.

The shared primitive converter continues using a `GtsStaticVertex` working buffer
for existing CPU normal/tangent algorithms before copying surface fields into the
separate `GtsSkinnedVertex` layout. No inheritance, universal vertex abstraction,
algorithm duplication, or new profile target/dependency is introduced by the rename.

Both prepared vertex profiles are plain aggregates with member defaults and
component-wise equality. Static position/color/UV shortcuts use designated
initializers; RGB colors explicitly supply alpha 1. There are no convenience
constructors, and omitted normals/tangents keep their profile defaults.
