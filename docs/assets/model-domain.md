# Canonical Model Asset Domain

`engine/modules/assets/model/` owns `GtsModelAsset`. The `IGtsModelImporter` interface
and its concrete strategies live together under `engine/modules/assets/importer/`.
The model domain compiles into `gravitas_assets`, which links `gravitas_core` and the CPU-only `gravitas_skin_assets` target
(and transitively `gravitas_skeleton_assets`). It has no renderer, Vulkan, ECS, or parser dependency.

The boundary for future file-backed model importers is:

```text
source file -> IGtsModelImporter -> GtsModelImportResult -> GtsModelImportBundle
                                                               |
                                                primary model + associated definitions
                                                               |
                                                    static preparation / runtime realization
```

[GtsObjModelImporter](obj-importer.md) produces this domain for both OBJ cooking
and development runtime source loading. [GtsGltfModelImporter](gltf-importer.md)
now also produces the canonical domain independently. glTF/GLB consumers remain
on their legacy cooking path until the later cutover.
See [consumer adaptation](obj-consumers.md) for cooked-v1 and runtime limitations.
The source-neutral [static geometry preparation stage](static-geometry.md) now
consumes individual canonical meshes below the format wall. Its generated/default
vertex fields belong only to the prepared static profile and never alter this domain.

## Ownership and geometry

An asset owns nodes, root indices, meshes, materials, and image inputs. Each mesh
owns independent primitives. Each primitive owns semantic attribute streams,
32-bit vertex indices, and an optional index into the asset's materials.
References are local array indices, with `std::optional<uint32_t>` for absent
mesh/material references. They are not persistent asset IDs or runtime handles.

Each attribute has a semantic and an unrestricted numbered set. Canonical values
are `vec3` for Position/Normal, `vec2` for TexCoord, `vec4` for Tangent/Color/Weights,
and `uvec4` for Joints. The variant stores exactly one typed vector per stream.
Importers decode source encodings into these GLM values. Multiple UV, color, joint,
or weight sets require no new vertex structure. Joint values are geometry data;
their skin-local interpretation is checked when a node selects a binding.
No weight normalization, animation, or morph storage is defined.

Primitives use point, line, or triangle lists. Empty indices select sequential
vertices; populated indices must address Position[0]. There are no strips, fans,
or primitive restart markers. The indexed or sequential element count must form
complete groups for the topology. Validation does not reject degenerate geometry
or generate, normalize, or otherwise repair attributes.

## Hierarchy and validation

Nodes store names, local `glm::mat4` transforms (identity by default), children,
and optional mesh/skin-binding indices. Matrices preserve imported transforms without forcing
them through the ECS Euler-angle transform representation. Children are the only
stored parent/child relationship; parents can be derived. World transforms would
compose as `parentWorld * localTransform` during static preparation / runtime realization.

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

## Skeleton uses and skin associations

`GtsModelSkin.h` defines two model-owned association values:

```text
GtsModelAsset
  skeletonUses[]: GtsModelSkeletonUse
    skeleton: shared_ptr<const GtsSkeletonAsset>
  skinBindings[]: GtsModelSkinBinding
    binding: GtsSkinBinding
    skeletonUseIndex: uint32_t
  nodes[]: GtsModelNode
    meshIndex: optional<uint32_t>
    skinBindingIndex: optional<uint32_t>
```

`GtsSkeletonAsset` is the reusable definition; `GtsSkeletonCompatibility` is its
exact structural contract. `GtsSkinBinding` still stores only the required
compatibility contract and paired local-slot remaps/inverse binds. It owns no
skeleton reference. `GtsModelSkeletonUse` supplies actual in-memory definition
identity and shared lifetime. Producers must treat published definitions as
immutable, including through any retained mutable aliases. Persistent identity,
serialization remain deferred. The [import bundle](import-bundle.md) now
enumerates produced definitions and shares their ownership with model uses.

Each skeleton-use entry is a distinct occurrence. Two entries referencing the
same definition are not deduplicated and need not eventually share a runtime
pose. Multiple bindings may target one use, and multiple nodes may select one
binding. One mesh can be instantiated with different bindings. The selected
binding belongs to the node occurrence, never to the mesh/primitive. Sharing a
skeleton pose does not imply sharing final skin matrices: inverse binds remain
binding-specific. There is no mutable pose or model-node/skeleton-node transform
correspondence in these association values yet.

Static models naturally leave both tables empty and node binding indices absent.
A node selecting a binding must also select a mesh. Transform-only nodes remain
valid. `skeletonUseIndex` defaults to `InvalidSkeletonUseIndex` (`uint32_t` max),
which is rejected; explicit zero is valid. Default skeleton uses contain null
references and are rejected if inserted into the model.

`validateGtsModelAsset` runs a focused internal model-skin validation phase in
`GtsModelSkinValidation.cpp`. It validates all use definitions and binding
associations, including unused entries. Existing skeleton and skin validators
own hierarchy, transform, compatibility, inverse-bind and skeleton-remap checks.
There is no second compatibility algorithm. Errors retain those domains' codes
with model binding/use context. Null/invalid uses and invalid node/table indices
are diagnosed without dereferencing them.

For **each bound node**, every primitive of its selected mesh must have at least
one paired `Joints[n]`/`Weights[n]` set. Pairing is bidirectional. Generic primitive
validation still owns canonical types, unique keys, finite values and stream
lengths matching Position[0]. The contextual phase checks:

- every joint component is a skin-local slot strictly below the selected binding's
  joint count, including components whose weights are zero;
- weights are nonnegative; every vertex has positive total weight;
- total weight across **all** numbered sets satisfies `abs(total - 1) <= 1e-4`
  (`GtsModelSkinWeightSumTolerance`), accumulated in double precision.

Validation never normalizes, clamps, fills streams, rewrites slots to skeleton
indices, or mutates inputs. Numbered sets need not be consecutive or start at
zero. More than four positive influences are valid. Runtime-profile limits belong
below the format wall. Empty meshes retain the existing policy: they have no
primitives to validate, even when bound.

Unbound JOINTS/WEIGHTS remain permitted under generic primitive validation,
including unpaired sets and weights without a usable total. Such geometry is
not asserted to be a usable skinned instance. The current canonical glTF importer
preserves unbound influence streams and checks individual weights, but does not
check their cross-set sum; later skin import must satisfy this new contextual
contract explicitly. Actual source skin references still fail import. Static
preparation still rejects skeletal semantics.

`tests/assets/GtsModelSkinTest.cpp` covers shared occurrences/definitions,
association/reference failures, contextual compatibility, node-specific binding
reuse, paired and arbitrary-numbered sets, slot bounds (including zero weights),
weight validity/tolerance, eight influences, unbound behavior, generic malformed
streams, deterministic diagnostics and non-mutating validation. It links only
`gravitas_assets` and runs with rendering/Vulkan disabled.

## Material appearance and image inputs

`GtsModelMaterial.h` defines ordinary metallic/roughness surface appearance,
independent of runtime material instances or shader selection. A primitive's
optional material index still addresses `GtsModelAsset::materials`. Materials
can be shared across primitives and meshes; unused materials are valid.

| Property | Default | Canonical meaning / range |
| --- | --- | --- |
| `name` | empty | Optional label, not identity |
| `baseColor` | white RGBA | Linear factor, each component in [0, 1] |
| `metallic` | 0 | Factor in [0, 1] |
| `roughness` | 1 | Perceptual roughness factor in [0, 1]; zero is valid |
| `emissiveFactor` | black RGB | Linear, nonnegative; HDR values are valid |
| `emissiveStrength` | 1 | Nonnegative emission multiplier |
| `normalScale` | 1 | Finite signed multiplier for tangent-space normal X/Y |
| `ambientOcclusionStrength` | 1 | [0, 1]; zero disables occlusion, one uses the sampled value |
| Six image bindings | absent | Base color, metallic, roughness, normal, AO, emissive |
| `alphaMode` | Opaque | Opaque ignores alpha; Mask rejects alpha below cutoff; Blend preserves coverage alpha |
| `alphaCutoff` | 0.5 | [0, 1], used only by Mask |
| `doubleSided` | false | Authored two-sided surface |

All numeric properties must be finite. Defaults match existing engine appearance
defaults, without copying renderer limits such as the minimum roughness of 0.04.
Importers must resolve source defaults explicitly (for example, glTF metallic
defaults to 1). There are no format-dependent defaults in the domain.

`GtsModelImageBinding` contains an `imageIndex` into the containing asset's
`images` and an unrestricted `uint32_t texCoordSet` (default 0). Absence is an
empty optional binding. A default binding has `InvalidImageIndex` (`uint32_t` max),
following the core's invalid-index convention; attaching it to a material fails
validation. Explicit image index 0 remains valid when that image exists.
These are canonical image references, not source texture objects, samplers, or
runtime textures. Base color consumes RGBA,
emissive consumes RGB, and normals consume RGB tangent-space normals decoded
from [0, 1] to [-1, 1]. Color texture RGB is sRGB; alpha and scalar/normal roles
are linear. Color, metallic, roughness, and emissive samples multiply their
factors; absent textures leave those factors unchanged. AO evaluates
`1 + strength * (sample - 1)`; an absent AO map gives no occlusion. An absent
normal map leaves the geometric surface frame unchanged. No fallback textures
or rendering operations are created by this contract.

Metallic, roughness, and AO use `GtsModelScalarImageBinding`: an `image` binding
plus a typed Red/Green/Blue/Alpha channel (default Red). Metallic and roughness
remain **independent inputs**. Separate OBJ maps can retain both images, while
a packed image can be shared by metallic/Blue and roughness/Green (and AO/Red).
This is channel selection, not a source-format flag. Downstream processing can
combine maps using these explicit semantics without inspecting the source
format. Canonical OBJ cooking now packs these channels explicitly into the current
v1 metallic/roughness output; glTF cooking still uses its legacy packed input.

`GtsModelImage.h` provides only the input ownership needed to make embedded
references meaningful. Each image has an optional name and exactly one source:

- An absolute external filesystem path, resolved by the importer. It need not
  exist during domain validation; no filesystem access occurs. Absolute paths
  remove dependence on the caller's working directory, but are import inputs,
  not portable cooked identifiers.
- `GtsModelEmbeddedImage`: nonempty encoded bytes and an optional MIME decoding
  hint. Importers extract these bytes from source containers/data URIs; the
  domain retains no container offsets, parser objects, or model-format metadata.

Materials reference the table rather than duplicating payloads. The same image
may serve multiple materials and roles, so color space belongs to the role,
not globally to the image. Decoded pixels, dimensions, mip chains, content hashes,
cooked paths, dependency tracking, and texture runtime ownership are deferred.
Sampler state and texture transforms are also deferred; neither legacy importer
currently preserves them. Future texture-domain work can extend image/binding
contracts without moving image ownership into materials.

`validateGtsModelMaterial` checks factors and alpha/channel enums, including
invalid enum values introduced by casts. `validateGtsModelAsset` additionally
validates every image source and material image index, even on unused materials.
For every primitive using a material, each present image binding must select an actual
`TexCoord[set]` stream on that primitive; normal primitive validation checks its
type, count, and finite values. All uint32 set identifiers are legal when that
stream exists. An unused material has no geometry against which to check UVs.
Invalid material references still fail model validation. Cutoff is validated
even outside Mask so dormant fields cannot carry malformed values. Validation
does not clamp, decode, generate UVs, or check image channel availability.
`GtsModelImportResult::success` applies these checks automatically.

### Importer mappings (canonical strategies implemented; glTF consumers remain legacy)

| Existing OBJ import output | Canonical destination |
| --- | --- |
| Name; diffuse RGB + dissolve | `name`; `baseColor` |
| Metallic; roughness | `metallic`; `roughness` |
| Emission RGB; strength 1 | `emissiveFactor`; `emissiveStrength` |
| Diffuse texture path | External image + `baseColorImage` |
| Separate metallic/roughness paths | Separate scalar bindings and images; importer chooses channels from source semantics |
| Normal path, otherwise bump path | Normal image binding for actual normal maps; see bump limitation below |
| Ambient texture path | Scalar `ambientOcclusionImage`, retaining the existing AO interpretation |
| Emissive texture path | `emissiveImage` |
| Dissolve < 1 gives Blend | `alphaMode = Blend`; otherwise Opaque |
| Other factors / cutoff / sidedness | Canonical defaults unless the importer interprets additional source data |

| Existing glTF import output | Canonical destination |
| --- | --- |
| Name, base color, metallic, roughness | Corresponding concrete name/factors after resolving source defaults |
| Emissive factor and emissive-strength extension | `emissiveFactor`, `emissiveStrength` |
| Normal scale; occlusion strength | `normalScale`; `ambientOcclusionStrength` |
| Base color / normal / emissive texture index | Importer maps source texture/image to a model image binding |
| Metallic-roughness texture index | Same image for metallic/Blue and roughness/Green |
| Occlusion texture index | AO/Red binding |
| Texture `texCoord` (currently discarded) | Each binding's `texCoordSet` |
| External URI / embedded image | Resolved external path / encoded image table entry |
| Opaque / Mask / Blend, cutoff, double-sided | `alphaMode`, `alphaCutoff`, `doubleSided` |

The old `ImportedMaterial` optionals become concrete factors; source omission is
resolved by import rather than retained as runtime ambiguity. Its parallel path
and signed texture-index fields become one model-local binding representation.
`renderState.depthWrite`, blend-state selection, and `vertexColorOnly` are
downstream policy, as are `MaterialShaderFamily`, feature flags, variants, handles,
versions, and resolved texture IDs. `.gmat` IDs, dependencies, and cooked references
are storage concerns. None enter this domain. The legacy importer does not
implement a source-authored unlit material semantic.

The former OBJ fallback treated height maps as normal maps. Canonical OBJ import
now diagnoses and omits height maps, preserving only actual normal maps. OBJ ambient color, specular/shininess, optical density,
illumination modes, displacement, separate opacity maps, map options, and glTF
material extensions other than emissive strength are not currently preserved by
the legacy importers and are not added here. Samplers, UV transforms, height-map
processing, and broader material workflows need focused follow-up work if required
by content. A source texture on geometry without its selected UV stream likewise
requires an explicit importer diagnostic/conversion decision, not silent repair.

## Import contract

`IGtsModelImporter::importAsset` accepts a source filesystem path and returns
`GtsModelImportResult` by value. Include `GtsModelImportResult.h` when implementing
or calling the interface; the interface header only forward-declares the result.
No selection registry, importer options, file IO implementation, or loader exists
in the model domain. Source file IO belongs to the neighboring importer strategies.

Use `GtsModelImportResult::success(asset, diagnostics)` for a model-only import,
or `success(bundle, diagnostics)` for associated skeleton definitions. Both
validate a [canonical import bundle](import-bundle.md). Errors discard all
products; warnings preserve success. The result exposes `bundle()`, `asset()`,
`succeeded()`, `diagnostics()` and `hasWarnings()`. The primary model is optional,
so a successful skeleton-only bundle has a null `asset()`; use `succeeded()` to
distinguish success. `failure(diagnostics)` exposes no bundle and always reports
at least one error. Current OBJ/glTF strategies remain model-only.

Diagnostics use the existing engine pattern of severity, code, and message,
plus a textual location. Existing `AssetImportResult`/`AssetDiagnostic` reside in
renderer-coupled `AssetTypes.h`, so this domain does not import those types.
Source diagnostics may name source locations; downstream behavior must not branch
on source format or diagnostic text.

## CPU-only test

`GtsModelAssetTest` and `GtsModelMaterialTest` link `gravitas_assets` and are registered independently of
the existing Vulkan-gated test suite. For an entirely backend-free build:

```sh
cmake -S engine -B /tmp/gravitas-model-domain-cpu -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DGTS_ENABLE_RENDERING=OFF -DGTS_ENABLE_VULKAN_BACKEND=OFF \
  -DGTS_ENABLE_PHYSICS=OFF -DGTS_ENABLE_DEBUGDRAW=OFF -DGTS_ENABLE_TOOLS=OFF \
  -DGTS_BUILD_TEST_SCENES=OFF -DGTS_BUILD_ASSETC=OFF \
  -DGTS_ENABLE_MODULE_SMOKE_TESTS=OFF -DGTS_ENABLE_RUNTIME_SMOKE_TESTS=OFF
cmake --build /tmp/gravitas-model-domain-cpu --target GtsModelAssetTest GtsModelMaterialTest --parallel 2
ctest --test-dir /tmp/gravitas-model-domain-cpu --output-on-failure
```
