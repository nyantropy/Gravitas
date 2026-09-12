# OBJ Cooking and Runtime Consumers

OBJ has one interpreter, `modules/assets/importer/obj/GtsObjModelImporter`.
The former legacy OBJ importer and backend model loader are removed, including
registry entries and tests dedicated to the old DTO interface.

```text
OBJ / MTL -> GtsObjModelImporter -> validated GtsModelAsset
================================ FORMAT WALL ================================
canonical meshes -> prepareGtsStaticMesh -> GtsPreparedStaticMesh
                  -> realizeGtsFlatStaticModel -> MeshAssetData
                         |                        |
               AssetCooker::cookModelAsset   loadRuntimeMeshAsset
                         |                        |
                  .gmesh / .gmat / .gtex      MeshManager::loadMeshCpu
                         |                        |
                  existing cooked loaders ------> MeshResource
                                                   |
                                           existing Vulkan upload

glTF / GLB -> GltfAssetImporter -> AssetImportResult -> existing cooker
                                                   -> existing v1 assets
```

## Single-resource adaptation

`modules/assets/realization/GtsStaticModelRealization` is CPU-only and source-neutral.
It validates a flat model with exactly one identity root per mesh. Hierarchy,
nonidentity transforms, and instancing are rejected explicitly, not flattened away.
Each mesh is prepared independently, then buffers are concatenated and indices and
primitive ranges rebased. No cross-primitive welding or boundary merging occurs.
Shape names become submesh debug names; canonical material associations determine
cooked submesh references. Bounds encompass all prepared positions. Attribute flags
are the intersection across nonempty meshes and generated flags are any-of, following
preparation's per-primitive rules. V1 cannot store those per-primitive metadata flags.

OBJ continues to emit one `<source>.gmesh` containing all shapes. This preserves
adjacent cooked lookup and existing single-resource requests. No `.gmodel` is needed
for this flat adaptation, matching previous OBJ output. glTF model output is unchanged.

## Materials and images

`AssetCooker::cookModelAsset` consumes canonical appearance directly and writes
`MaterialAssetData`; it never reconstructs legacy model DTOs. Factors, alpha mode,
cutoff, and double-sided state map to v1 fields. Blend disables depth writes as
realization policy. The explicit `vertexColorOnly` option selects Unlit; otherwise
StandardSurface is used. Missing material assignments receive a cooked default.
The base-color override retains its existing default-material-only behavior.

Image decoding and mip generation reuse `ImageAssetImporter`, `ImportedTexture`
(the shared CPU image carrier), `TextureCookCache`, and `TextureCooker`. No model
geometry or material passes through `AssetImportResult`/`ImportedMesh`/`ImportedMaterial`.
External and embedded encoded inputs are supported. Ordinary images deduplicate
by source identity and role; different roles produce separate color-space variants.
Repeated scalar bindings deduplicate by image indices, selected channels, and role.

Metallic and roughness images are sampled from their explicit scalar channels and
packed into blue and green, respectively. An absent map contributes byte 255 so
its scalar factor remains effective. AO is remapped into red. Packing precedes
existing linear-data mip generation. Unequal metallic/roughness dimensions fail
with `ASSET_COOK_SCALAR_IMAGE_SIZE`; resampling policy remains a later decision.
Scalar decode failure is an error. Ordinary missing/undecodable images retain the
existing warning plus runtime-fallback policy. All v1 material image bindings must
select UV0; nonzero sets fail with `ASSET_COOK_UV_SET_UNSUPPORTED`.

Cooked references remain local output filenames with existing deterministic IDs.
Mesh -> material -> texture dependencies remain intact after source removal.
The canonical import result has no source dependency manifest; the old OBJ's
OBJ/MTL dependency table was not consumed by the cooker. This migration does not
add a source-provenance API or change build discovery/invalidation policy.

## Runtime policy

`modules/assets/runtime/RuntimeMeshLoading` centralizes CPU path resolution.
Adjacent cooked `.gmesh` wins even when the requested source is absent or malformed.
Strict/shipping policy rejects missing cooked assets; development permits OBJ only.
There is no direct runtime glTF parser and no corrupt-cooked fallback to source.

`MeshManager::loadMesh` preserves its path/alias cache and warning behavior, uses
its public `loadMeshCpu` phase to populate the resource, then performs the existing
buffer uploads. CPU loading can be tested without a Vulkan context or GPU.
Development source loading retains primitive ranges but uses caller/default runtime
materials, as before: it does not cook images or create source material instances.
Cooked submesh material references still support the existing opt-in mesh-material path.

## Observable changes and retained limits

- Vertex storage may grow because primitive-local vertices are not welded across
  shapes/material boundaries. Triangle topology and primitive ordering survive.
  Generated normals are local to each primitive, so smoothing across those old
  shared boundaries can change.
- Missing/partial attributes follow canonical per-primitive policy. Good normals
  and UVs in other primitives survive; authored normals retain their exact values.
- Missing colors remain absent canonically and become white only during preparation.
- Canonical material defaults apply (white base color, roughness 1). MTL-relative
  paths are resolved by the canonical importer. Unsupported height/opacity maps
  produce importer diagnostics instead of being misinterpreted as normal maps.
- A material requiring UVs on a primitive that lacks them fails canonical validation.
- Both metallic and roughness maps now influence the packed result with correct channels.
- V1 remains a fixed static layout with one UV set and conservative mesh-wide metadata.
  General hierarchy realization, provenance, samplers, resampling, glTF migration,
  and semantic-stream serialization are separate future work.

## Build and tests

The rendering target compiles the small CPU realization/runtime adapters and links
`gravitas_obj_importer` and `gravitas_static_geometry`. It no longer directly links
TinyOBJ. Canonical domain/import/preparation targets remain usable without rendering.
The reusable normal/tangent algorithms remain in rendering/core/geometry because
legacy glTF and procedural callers still use them; relocation is deferred.

`CanonicalObjPipelineTest` exercises public CPU runtime loading, strict policy,
source removal/cooked preference, boundaries, authored/generated attributes,
material factors and dependencies, scalar packing, deterministic files, and v1
limits. Existing cooker integration retains glTF/GLB/image coverage. Canonical
importer and preparation tests retain tuple seam and attribute-policy coverage.
