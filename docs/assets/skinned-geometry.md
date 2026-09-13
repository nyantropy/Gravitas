# CPU skinned geometry preparation

`modules/assets/processing/geometry/skinned/` owns the independent
`gravitas_skinned_geometry` target. It converts one canonical mesh for one
binding into a concrete CPU profile. It has no source-format or pose knowledge.

```text
GtsModelMesh + GtsSkinBinding (immutable inputs)
================== FORMAT WALL ==================
prepareGtsSkinnedMesh(mesh, binding)
    -> GtsSkinnedMeshPreparationResult
    -> GtsPreparedSkinnedMesh
         vertices: GtsSkinnedVertex
         indices: rebased uint32
         primitives: index ranges + canonical material indices + metadata
         metadata / influences: aggregate summaries
```

## Input and result contract

The required `const GtsSkinBinding&` makes missing binding context a compile-time
error. Preparation validates the binding's self-contained compatibility descriptor,
nonempty/assigned joint table and finite affine inverse binds through
`validateGtsSkinBinding`. It additionally checks every remap against the descriptor's
node count. No actual skeleton object is needed for this conversion. Selecting an
actual compatible skeleton use remains the responsibility of canonical model/skin
contextual validation; geometry alone cannot distinguish two intended bindings
when both accept the same skin-local slots.

`validateGtsModelPrimitiveSkin` composes generic primitive validation with the same
bound-influence helper used by model association validation. It checks paired
Joints/Weights for every numbered set, typed/count-matched streams, all slot bounds
(including zero-weight components), finite/nonnegative weights, positive totals,
and `abs(total - 1) <= 1e-4` across all sets. The processor does not repair invalid
canonical weights. Non-triangle topology and combined counts exceeding uint32 fail.
Ignored attributes still undergo canonical validation. An empty mesh produces an
empty prepared mesh, as in the static profile, but still requires a valid binding.

The result exposes `succeeded()`, `mesh()`, `diagnostics()`, and `hasWarnings()`.
Errors never expose partial geometry; warnings can accompany success. Diagnostics
reuse `GtsModelDiagnostic`, with primitive, attribute, vertex/component or binding
locations. Canonical errors retain their codes. Profile errors include
`SKINNED_BINDING_NODE_OUT_OF_RANGE`, `SKINNED_TOPOLOGY_UNSUPPORTED`,
`SKINNED_MESH_TOO_LARGE`, `SKINNED_WEIGHT_TOTAL`, and `SKINNED_VERTEX_NONFINITE`.
Inputs are neither mutated nor retained by the result.

## Vertex and influence profile

`GtsSkinnedVertex` is distinct from unchanged static `GtsStaticVertex`: `vec3 pos/normal`,
`vec4 tangent/color`, `vec2 texCoord`, `uvec4 joints`, and `vec4 weights`.
This is a CPU layout; GPU offsets, alignment, bindings and shader ABI are not
established by this type yet.

Canonical geometry retains arbitrary numbered JOINTS/WEIGHTS sets. This profile
supports four effective (nonzero) entries per vertex:

1. Visit sets in ascending numeric set index, then components in XYZW order.
   Attribute insertion order does not affect results; set indices need not be contiguous.
2. Discard zero-weight entries after validating their joint-slot bounds.
3. If more than four entries remain, stable-sort by descending weight and keep
   the strongest four. Equal weights retain the canonical set/component order.
   With at most four entries, preserve that canonical order.
4. Divide selected weights by their double-precision accumulated total, storing
   float results. This also removes permitted unit-sum drift in non-reduced inputs.
5. Pad unused entries with joint slot 0 and weight 0. No new weighted influence
   is invented. Repeated slots/remaps are not merged.

`SKINNED_INFLUENCES_REDUCED` warns once per affected primitive and reports the number
of reduced vertices. Both primitive and mesh `influences` metadata expose
`reducedInfluenceVertices` and `maxSourceInfluenceCount` (nonzero entries before
reduction); mesh counts sum and maxima take the maximum across primitives.

```text
preparedVertex.joints[k] = skin-local slot
                           -> binding.joints[slot].skeletonNodeIndex
```

Prepared joints never become skeleton-node indices. The same geometry can be
prepared with different valid bindings; the output does not own/copy a binding.
Later realization must keep the selected binding associated with its geometry.

## Shared geometry behavior

Both profiles use `GtsPrimitiveGeometryPreparation` in `gravitas_primitive_geometry`.
This small extraction retains the existing `gts::rendering::prepareMeshGeometry`
algorithms unchanged, including authored-normal restoration after tangent generation.
The shared helper accepts already validated primitives; profile validation stays
in the separate static/skinned processors. Static preparation still rejects skin streams.

| Canonical stream | Prepared behavior |
| --- | --- |
| Position[0] | Required, copied in stored mesh coordinates |
| Normal[0] | Authored values preserved exactly, otherwise generated |
| Tangent[0] | Authored values preserved exactly; otherwise generated with UV0 |
| Color[0] | Copied, or white RGBA default |
| TexCoord[0] | Copied, or zero default |
| Additional non-influence sets | Ignored with `SKINNED_ATTRIBUTE_IGNORED` per stream |
| Joints[n]/Weights[n] | Validated and reduced across every numbered set |

Without UV0, missing tangents remain `{1,0,0,1}` and are not marked generated.
Degenerate geometry/UVs use the existing finite fallback algorithms. Default
white colors and zero UVs do not imply authored attribute availability.
All prepared floating data is checked for finiteness before publishing success.

Primitives remain separate and ordered, with index ranges, optional canonical
material indices and local metadata. Empty index arrays become sequential indices;
concatenation rebases by preceding vertex count, not index count. Material indices
are not resolved into runtime materials or checked against a model's material table
by this mesh-only API. The mesh debug name is retained; canonical primitives have
no names to copy. Geometry metadata intersects attribute flags, ORs generation
flags, and reports combined vertex/index counts.

## Bounds, dependencies and deferred consumers

Like `GtsPreparedStaticMesh`, this type does not own bounds. Future realization
may compute stored/bind-space bounds. Such bounds alone are insufficient for
animated culling; animated/conservative bounds remain deferred.

`gravitas_skinned_geometry` links CPU `gravitas_primitive_geometry` and
`gravitas_skin_assets`; the shared target links `gravitas_assets` and exposes the
existing CPU-only `rendering/core/geometry` header directory for the algorithms.
`GtsStaticVertex` belongs to `geometry/static/`; common metadata belongs to
`geometry/GtsGeometryMetadata.h`. The skinned prepared header includes common
metadata and math, without depending on the static vertex header. Neither profile links the renderer/backend. Canonical model's
existing dependency on animation *assets* remains transitive; there is no dependency
on animation *evaluation*, ECS, importers, Vulkan or a runtime resource manager.

No positions/normals are pre-skinned, no skeleton/model/world transforms are
applied, and no pose or inverse-bind multiplication occurs. GPU palette access,
normal deformation, vertex ABI, animated bounds, cooking and playback remain
separate future decisions.

## Tests

`tests/assets/processing/geometry/skinned/` contains Release-active tests for
one through eight influences, deterministic ties and strongest-weight selection,
normalization/padding, numbered sets, binding checks, invalid data, primitive
rebasing/materials, attribute generation/defaults and input immutability. A direct
comparison with static preparation protects shared base-geometry behavior.

`GtsImportedSkinnedMeshTest` uses `GltfFixtureBuilder` to import an animated rigged
GLB, selects its node mesh/binding, and prepares it without evaluating a pose.
Its source skin order differs from skeleton order, and the model node has a
nonidentity transform; expected undeformed positions and local slots catch accidental
remapping or transform application. It checks counts, generated attributes and
unit prepared weights. No binary fixture or GPU initialization is needed.

With the CPU configuration in [model-domain.md](model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --parallel 4
ctest --test-dir /tmp/gravitas-model-domain-cpu -L cpu --output-on-failure
```
