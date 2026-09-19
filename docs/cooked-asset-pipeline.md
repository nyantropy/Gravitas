# Cooked Asset Pipeline

Source interpretation is canonical for both OBJ and glTF/GLB. See
[canonical cooking](model/canonical-cooking.md) for decomposition, capability
validation, publication and the legacy parity inventory.

```text
OBJ / MTL → GtsObjModelImporter ──┐
                                ├→ GtsModelImportBundle → canonical cooking
glTF/GLB → GtsGltfModelImporter ──┘                          ↓
                                                .gmesh/.gmodel/.gmat/.gtex
                                                           ↓
                                                   GtsModelRegistry
                                                           ↓
                                                   GtsModelResource
                                                           ↓
                                                   GtsRealizedModel
                                                           ↓
                                                world material realization
                                                           ↓
                                                   GtsModelInstance
                                                           ↓
                                                generic render extraction
```

Standalone PNG/JPG and canonical model images use `GtsImageDecode`, then
`TextureCookInput → TextureCooker → .gtex`. No legacy model/image importer interface
remains. Material CPU decoding remains separate from `MaterialAssetRealization`.
Lower-level direct mesh and texture resource APIs remain available: `MeshManager`
prefers adjacent `.gmesh`, while `TextureManager` prefers adjacent `.gtex`.

## Cooked Asset Contracts

Cooked data is stored in engine-owned CPU structures in
`engine/modules/assets/serialization/AssetTypes.h` for shared mesh/material/texture data
and `engine/modules/model/serialization/ModelAssetTypes.h` for model hierarchy.

- `AssetReference` stores a placeholder stable `AssetId` plus `logicalPath`.
- `MeshAssetData` stores vertices, indices, submeshes, dependencies, bounds,
  attribute flags, and generated-normal/tangent flags.
- `SubmeshAssetData` stores `firstIndex`, `indexCount`, `material`, and a debug
  name.
- `MaterialAssetData` stores shader family, render state, scalar material
  factors, texture references, dependencies, and `vertexColorOnly`.
- `ModelAssetData` stores package-level node hierarchy, mesh references,
  material references, and dependencies.
- `ModelNodeAssetData` stores a node name, parent index, mesh reference, and
  local transform.
- `TextureAssetData` stores texture dimensions, mip count, format, color space,
  default sampler metadata, mip payloads, and dependencies.
- `TextureMipData` stores mip width, height, row pitch, slice pitch, and bytes.
- `TextureSamplerDesc` stores engine-owned filter, mip-filter, addressing, and
  anisotropy settings.

These structures contain no Vulkan types, descriptor sets, ECS entities,
runtime manager handles, parser-library types, or GPU cache state.

## File Layout

Shared serializers live in `engine/modules/assets/serialization/AssetSerializers.*`;
model hierarchy encoding lives in `engine/modules/model/serialization/ModelAssetSerializer.*`.
`.gmesh`, `.gmat`, `.gmodel`, and `.gtex` are explicit little-endian binary
formats. C++ object memory is not dumped directly.

Every cooked file begins with a 64-byte header:

```text
offset  size  field
0       4     magic: 0x54534147 ("GAST")
4       2     format version
6       2     asset type: 1 mesh, 2 material, 3 model, 4 texture
8       4     flags, currently 0
12      4     header size, currently 64
16      8     asset id
24      8     payload offset
32      8     payload size
40      8     dependency table offset
48      8     dependency table size
56      8     checksum placeholder, currently 0
```

Payload fields are written explicitly. Mesh payloads contain ID, name,
attribute flags, generated-data flags, bounds, vertex/index/submesh counts,
the vertex stream, index stream, and submesh records. Material payloads contain
ID, name, shader family, material factors, render state, `vertexColorOnly`, and
texture references.

Model payloads contain ID, name, node count, mesh count, material count, node
records, mesh references, and material references. A node record contains name,
parent index, mesh reference, and a column-major `glm::mat4` local transform.

Texture payloads contain ID, name, dimensions, mip count, texture format,
color space, sampler metadata, mip descriptors, and mip payload bytes. A mip
descriptor contains width, height, row pitch, slice pitch, absolute payload
offset, and payload size. Mip bytes are stored in deterministic mip order from
largest to smallest.

The dependency table is a counted list of `AssetReference` values. It is stored
outside the payload and referenced by validated header offsets.

## Versioning

The first supported versions are:

- `.gmesh`: `MeshAssetFormatVersion = 1`
- `.gmat`: `MaterialAssetFormatVersion = 1`
- `.gmodel`: `ModelAssetFormatVersion = 1`
- `.gtex`: `TextureAssetFormatVersion = 1`

Deserializers reject unsupported versions, invalid magic, wrong asset type,
truncated files, invalid payload/dependency offsets, invalid counts, corrupted
dependency tables, invalid submesh ranges, invalid model parent indices,
invalid texture formats/color spaces, invalid mip dimensions, invalid mip
pitches, invalid mip payload ranges, and overlapping mip payloads.

## glTF Import Support

`GtsGltfModelImporter` is the authoritative interpreter for both cooking and source
loading. See [glTF contracts](model/gltf-importer.md) for geometry, scene, hierarchy,
material/image, skin and animation semantics. Cooking receives canonical tables,
not parser state. External images retain resolved paths; embedded images retain
encoded bytes. The shared decoder supplies RGBA8 pixels for texture cooking.

Canonical source loading supports skeletal animation, but cooked-v1 cannot persist
skeletons, bindings, skeleton uses, animation clips or skinned profiles. Cooking
rejects those products explicitly instead of emitting a degraded static package.
Material bindings requiring UV sets other than UV0 also fail cooking.

## Import Convention

The engine import convention is:

- right-handed coordinates
- +Y up
- -Z forward
- unit scale 1.0
- triangle winding preserved
- positions, normals, and tangent XYZ are not axis-converted
- UVs are converted to the engine texture convention by the canonical importer
- tangent handedness W is negated when V is flipped

glTF already uses the engine axis convention. OBJ files are assumed to be
authored in the same convention; the OBJ importer applies the same UV V policy.

## Cooker

`assetc` is the command-line cooker:

```text
assetc import robot.obj --output build/assets
assetc import robot.glb --output build/assets
assetc import brick_basecolor.png --texture-role base-color --output build/assets
```

Supported options:

- `--importer <name>`
- `--base-color-texture <path>`
- `--vertex-color-only`
- `--texture-role <base-color|metallic-roughness|normal|ambient-occlusion|emissive|ui|font-atlas|particle|data>`
- `--single-mip`

The cooker selects an importer by source format or explicit importer override,
imports source data, converts imported CPU data to cooked
`MeshAssetData`/`MaterialAssetData`/`TextureAssetData`, computes bounds,
generates texture mip chains, preserves OBJ and glTF primitive/submesh
material assignments, serializes `.gmesh`, `.gmat`, `.gmodel`, and `.gtex`,
and emits structured diagnostics. It does not create Vulkan resources or ECS
entities.

One source asset can now produce multiple cooked assets. A GLB with two meshes
and two materials can produce:

```text
robot_body.gmesh
robot_head.gmesh
robot_red.gmat
robot_blue.gmat
robot_image_0.gtex
robot.gmodel
```

No source-format branches exist in the cooker output path; OBJ and glTF both
flow through `GtsModelImportBundle` and the same canonical cooking core.

## Texture Cooking

`GtsImageDecode` supports filesystem and memory inputs. Canonical embedded image
bytes go directly to this shared decoder. `TextureCookInput` is cooking-local CPU
image data, normalized to RGBA8 before mip generation. Texture packing uses the
shared `GtsScalarImagePacking` helper, preserving canonical channel selection.

Phase 5 emits uncompressed RGBA8 cooked textures:

- `RGBA8_SRgb` for base color, emissive, UI, and particle color textures.
- `RGBA8_UNorm` for metallic/roughness, normal, ambient occlusion, font atlas,
  and data textures.

The file format reserves enum values for BC formats, but the runtime uploader
currently rejects compressed cooked texture formats until Phase 5B.

Full mip chains are generated by default. Each next mip uses:

```text
nextWidth  = max(1, currentWidth / 2)
nextHeight = max(1, currentHeight / 2)
```

Color/data mip generation uses deterministic box averaging. Normal-map mip
generation averages decoded normal vectors, renormalizes them, and re-encodes
to RGB. `--single-mip` disables mip generation for cases such as font atlases
and exact UI images.

Color space is role-driven, not inferred only from filenames:

- BaseColor, Emissive, UI, ParticleColor: sRGB.
- MetallicRoughness, Normal, AmbientOcclusion, FontAtlas, Data: Linear.

Runtime font loaders explicitly request linear atlas textures, including nearest
sampling through `requestPixelTexture(path, TextureColorSpace::Linear)`. Filtering
and color space are independent; ordinary pixel-art textures still default to sRGB.
The texture manager rejects cooked/requested color-space mismatches rather than
reinterpreting cooked pixels.

If the same source texture is used through different roles, the cooker emits
`TEXTURE_COLOR_SPACE_CONFLICT` and writes separate cooked variants for those
roles.

Sampler metadata is stored in `.gtex` for Phase 5. Runtime callers can still
request source PNG/JPG paths with nearest/clamp flags for development fallback,
but cooked `.gtex` loading uses the cooked texture's default sampler. Future
sampler separation can move sampler state into material bindings without
changing the mip payload format.

## Dependency Handling

Cooked assets use `AssetReference`, not raw runtime handles. Stable asset IDs
are placeholders derived from logical paths so future real asset IDs can replace
path lookup without changing the cooked structures.

Generated material references use cooked output filenames as logical paths.
Generated texture references use `.gtex` output filenames as logical paths.
`MaterialAssetRealization` resolves relative texture references against the `.gmat`
file location before creating runtime material instances.

## Runtime Submesh Ranges

Cooked meshes preserve submesh ranges in `MeshAssetData::submeshes` and
`MeshResource::submeshes`. `SceneRenderStage` expands a mesh with submeshes into
one prepared draw batch per submesh and calls:

```text
vkCmdDrawIndexed(indexCount, instanceCount, firstIndex, 0, 0)
```

This allows cooked OBJ and GLB meshes to draw the recorded index ranges.
Direct `StaticMeshComponent` usage preserves the entity-level
`MaterialReferenceComponent` material by default for compatibility. When
`StaticMeshComponent::useMeshMaterials` is enabled, static mesh binding resolves
cooked submesh `.gmat` references into runtime `MaterialInstanceHandle`s and the
renderer uses the matching material for each submesh draw range.

## Remaining Direct Source Loads

These runtime paths still load source assets directly for compatibility:

- In development policy, `MeshManager::loadMesh` can fall back to
  canonical OBJ import and static preparation when no cooked `.gmesh` exists. This is
  logged with the source path and expected cooked path.
- Development source fallback currently supports OBJ only. Missing cooked
  `.gltf`/`.glb` requests fail clearly because the runtime has no source glTF
  loader.
- `GTS_RUNTIME_ASSET_POLICY=strict`, `GTS_SHIPPING_BUILD`, or
  `GTS_DISABLE_RUNTIME_SOURCE_ASSET_FALLBACK` reject missing cooked meshes
  instead of falling back.
- Runtime code can still request an OBJ path for compatibility. There is no
  runtime glTF parser path.
- In development policy, `TextureManager::loadTexture` can fall back to
  PNG/JPG/JPEG decoding when no adjacent `.gtex` exists. This is logged with
  `TEXTURE_RUNTIME_SOURCE_FALLBACK`, the source path, expected cooked path,
  active policy, and an `assetc` remediation command.
- `GTS_RUNTIME_ASSET_POLICY=strict`, `GTS_SHIPPING_BUILD`, or
  `GTS_DISABLE_RUNTIME_SOURCE_ASSET_FALLBACK` reject missing cooked textures
  instead of falling back to source image decoding.
- HDR environment textures still use `EnvironmentResourceManager` and
  `stbi_loadf` because they produce irradiance, prefiltered specular, and BRDF
  LUT resources. This is intentionally deferred to a future cooked environment
  format, not ordinary `.gtex`.
- Screenshot PNG writing is output-only and is not part of the asset import
  pipeline.

## Module ownership

Model cooking, hierarchy codecs and model loading live under `model/cooking`,
`model/serialization` and `model/loading`. Shared texture cooking, mesh/material/texture
codecs and loaders remain under `assets/`. `assetc` dispatches to the separate CPU
model and standalone image cooking targets;
rendering is not required to run it. The legacy glTF/image DTO importer directory
has been removed. See
[model subsystem ownership](model/architecture.md) and [shared asset infrastructure](assets/architecture.md).
