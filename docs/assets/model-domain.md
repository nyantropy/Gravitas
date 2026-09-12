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
format. Existing cooking currently chooses the metallic path or the roughness
path, and does not combine the two; that implementation is unchanged here.

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

### Mapping the existing importers (no migration yet)

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
| Other factors / cutoff / sidedness | Canonical defaults unless the future importer interprets additional source data |

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

The OBJ fallback that calls a bump/height map a normal map is not a correct height
interpretation. This contract defines normal maps only; future OBJ migration
must deliberately convert, diagnose, or omit height maps rather than label them
as tangent normals. OBJ ambient color, specular/shininess, optical density,
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

`GtsModelAssetTest` and `GtsModelMaterialTest` link only `gravitas_core` and are registered independently of
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
