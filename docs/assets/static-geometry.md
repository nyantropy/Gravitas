# Static Geometry Preparation

`engine/modules/assets/processing/geometry/` owns the standalone CPU target
`gravitas_static_geometry`. It consumes one canonical mesh, independently of any
source file, importer, material realization, or resource manager:

```text
GtsModelMesh (immutable, primitive-local semantic streams)
====================== FORMAT WALL ======================
prepareGtsStaticMesh(const GtsModelMesh&)
    -> GtsStaticMeshPreparationResult
    -> GtsPreparedStaticMesh
        vertices: current static Vertex layout
        indices: shared, rebased uint32 indices
        primitives: ordered ranges, material indices, per-primitive metadata
        metadata: whole-mesh summary
```

## Contract and ownership

`GtsPreparedStaticMesh` owns its name, vertices, indices, primitive records, and
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

The implementation calls the existing CPU-only, header-only
`gts::rendering::prepareMeshGeometry` once per primitive. Its default initialization,
area-weighted normal accumulation, tangent generation, handedness, degeneracy
thresholds, and finite fallbacks are reused unchanged. There is no legacy
`ImportedMesh` intermediate and no second implementation of the geometry math.

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

Bounds are deliberately deferred in this step. The current `AssetBounds` is in
renderer-coupled `AssetTypes.h`, and `computeAssetBounds` is in `AssetCooker.h`.
The runtime alternative is an ECS `BoundsComponent`. Reusing those would import
forbidden dependencies; moving them would change cooker/runtime contracts outside
this task. No duplicate bounds type or calculation is introduced. A narrow shared
CPU bounds contract can be extracted when the next consumer migration needs it.

The preparation target links `gravitas_assets` (and transitively core) and exposes the existing narrow
`rendering/core/geometry` header directory for `Vertex` and metadata. The reused
processor header itself includes only standard headers and `Vertex.h`. No linking
to `gravitas_rendering`, GLFW, Vulkan, TinyOBJ, or the importer is required, and no
existing geometry source needed extraction. The module location preserves the
rule that core domain headers must not depend on module contracts.

The target is available with rendering disabled and is not linked into the runtime
umbrella. No importer, cooker, serializer, runtime loader, ECS binding, or renderer
calls it yet. Existing production paths continue to use their unchanged preparation.

## Tests

`tests/assets/processing/geometry/GtsStaticMeshPreparationTest` constructs canonical
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
