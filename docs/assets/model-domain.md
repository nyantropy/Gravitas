# Canonical Model Asset Domain

`engine/core/assets/model/` owns `GtsModelAsset` and `IGtsModelImporter`.
It compiles into `gravitas_core` and depends only on the standard library and
the engine's `GlmConfig.h`. It has no renderer, Vulkan, ECS, or parser dependency.

The boundary for future file-backed model importers is:

```text
source file -> IGtsModelImporter -> GtsModelImportResult -> GtsModelAsset
                                                               |
                                                    future runtime realization
```

This is a domain and contract foundation only. No concrete importer or runtime
consumer uses it yet. Existing OBJ loading, tooling OBJ/glTF importers, cooked
asset types, and runtime realization remain unchanged. Their migration is a
separate task; the old model representation is not a permanent alternative.

## Ownership and geometry

An asset owns nodes, root indices, meshes, and minimal named materials. Each mesh
owns independent primitives. Each primitive owns semantic attribute streams,
32-bit vertex indices, and an optional index into the asset's materials.
References are local array indices, with `std::optional<uint32_t>` for absent
mesh/material references. They are not persistent asset IDs or runtime handles.

Each attribute has a semantic and an unrestricted numbered set. Canonical values
are `vec3` for Position/Normal, `vec2` for TexCoord, `vec4` for Tangent/Color/Weights,
and `uvec4` for Joints. The variant stores exactly one typed vector per stream.
Importers decode source encodings into these GLM values. Multiple UV, color, joint,
or weight sets require no new vertex structure. Joint values are geometry data;
no skeleton binding, weight normalization, animation, or morph storage is defined.

Primitives use point, line, or triangle lists. Empty indices select sequential
vertices; populated indices must address Position[0]. There are no strips, fans,
or primitive restart markers. The indexed or sequential element count must form
complete groups for the topology. Validation does not reject degenerate geometry
or generate, normalize, or otherwise repair attributes.

## Hierarchy and validation

Nodes store names, local `glm::mat4` transforms (identity by default), children,
and optional mesh indices. Matrices preserve imported transforms without forcing
them through the ECS Euler-angle transform representation. Children are the only
stored parent/child relationship; parents can be derived. World transforms would
compose as `parentWorld * localTransform` during future runtime realization.

`validateGtsModelPrimitive` checks nonempty Position[0], canonical semantic types,
unique semantic/set pairs, equal nonzero stream lengths, finite attribute values,
valid indices, and topology counts. An absent attribute has no stream entry;
an empty entry is malformed. Primitive validation cannot check material bounds
without the containing asset.

`validateGtsModelAsset` additionally checks material/mesh/node references, finite
local transforms, unique roots, and a forest in which every node is reachable
from one root. Roots have no incoming child references and other nodes have one.
Duplicate children, multiple parents, cycles, and omitted roots fail validation.
Traversal is iterative to support deep hierarchies. Diagnostics identify canonical
locations such as `meshes[1].primitives[0]`.

Empty asset containers, empty meshes, transform-only nodes, and unreferenced
meshes/materials are valid. Every primitive that is present must pass geometry
validation. Multiple nodes may reference the same mesh. Validation checks no
GPU capabilities, filesystem state, or source-format rules.

## Import contract

`IGtsModelImporter::importAsset` accepts a source filesystem path and returns
`GtsModelImportResult` by value. Include `GtsModelImportResult.h` when implementing
or calling the interface; the interface header only forward-declares the result.
No selection registry, importer options, file IO implementation, or loader exists
in this domain yet.

Use `GtsModelImportResult::success(asset, diagnostics)` to finalize an import.
It validates the asset before exposing it. Any validation or importer error
produces failure and discards the asset. Warnings preserve success and are queried
with `hasWarnings()`. `failure(diagnostics)` always returns no asset and at least
one error, adding a generic error if necessary. `succeeded()`, `asset()`, and
`diagnostics()` provide read-only inspection; there is no default or partially
successful result state.

Diagnostics use the existing engine pattern of severity, code, and message,
plus a textual location. Existing `AssetImportResult`/`AssetDiagnostic` reside in
renderer-coupled `AssetTypes.h`, so this domain does not import those types.
Source diagnostics may name source locations; downstream behavior must not branch
on source format or diagnostic text.

## CPU-only test

`GtsModelAssetTest` links only `gravitas_core` and is registered independently of
the existing Vulkan-gated test suite. For an entirely backend-free build:

```sh
cmake -S engine -B /tmp/gravitas-model-domain-cpu -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DGTS_ENABLE_RENDERING=OFF -DGTS_ENABLE_VULKAN_BACKEND=OFF \
  -DGTS_ENABLE_PHYSICS=OFF -DGTS_ENABLE_DEBUGDRAW=OFF -DGTS_ENABLE_TOOLS=OFF \
  -DGTS_BUILD_TEST_SCENES=OFF -DGTS_BUILD_ASSETC=OFF \
  -DGTS_ENABLE_MODULE_SMOKE_TESTS=OFF -DGTS_ENABLE_RUNTIME_SMOKE_TESTS=OFF
cmake --build /tmp/gravitas-model-domain-cpu --target GtsModelAssetTest --parallel 2
ctest --test-dir /tmp/gravitas-model-domain-cpu --output-on-failure
```
