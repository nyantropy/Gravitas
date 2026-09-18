# OBJ Cooking and Runtime Consumers

OBJ has one interpreter, `modules/assets/importer/obj/GtsObjModelImporter`.
The former legacy OBJ importer and backend model loader are removed, including
registry entries and tests dedicated to the old DTO interface.

```text
OBJ / MTL → GtsObjModelImporter ─┐
                               ├→ canonical bundle → canonical cooking → cooked-v1
glTF / GLB → GtsGltfModelImporter┘
```

See [canonical cooking](canonical-cooking.md) for the authoritative model cooking
contract. One identity node/one mesh emits `<source>.gmesh`; multiple meshes,
hierarchy, transforms or sharing produce `.gmodel` with separate mesh definitions.
This source-neutral rule replaces the former OBJ-only flattening cooker. Canonical
material factors, independent scalar channels and image sources flow through the
same core for both formats. Duplicate slots stay distinct. Non-UV0 material bindings
and unsupported animation/deformation fail explicitly.

`realizeGtsFlatStaticModel` remains a lower-level mesh-loading adapter. It accepts
only flat identity-root models, concatenates geometry for a direct mesh request,
and rejects hierarchy/instancing. It is no longer part of complete-model cooking.
The helper reuses authoritative static preparation; it does not interpret source files.

## Runtime policy

`modules/assets/loading/mesh/RuntimeMeshLoading` centralizes CPU path resolution.
Adjacent cooked `.gmesh` wins even when the requested source is absent or malformed.
Strict/shipping policy rejects missing cooked assets; development permits OBJ only.
This lower-level mesh API does not load glTF and never falls back from corrupt
cooked data to source. Complete OBJ/glTF models use the separate canonical-backed
[model registry](model-runtime.md), including source materials and hierarchy.

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
  Sampler preservation, resampling, animated cooking and semantic-stream
  serialization remain separate future work.

## Build and tests

The CPU `gravitas_mesh_loading` and `gravitas_static_model_realization` targets
own the existing loading/flat-adapter implementations. Rendering consumes them;
`gravitas_asset_cooking` shares the canonical importer/preparation targets.
TinyOBJ remains private to the canonical OBJ importer. Canonical domain/import/preparation targets remain usable without rendering.
The reusable normal/tangent algorithms remain in rendering/core/geometry because
static preparation and procedural callers still use them; relocation is deferred.

`CanonicalObjPipelineTest` exercises public CPU runtime loading, strict policy,
source removal/cooked preference, boundaries, authored/generated attributes,
material factors and dependencies, scalar packing, deterministic files, and v1
limits. Existing cooker integration retains glTF/GLB/image coverage. Canonical
importer and preparation tests retain tuple seam and attribute-policy coverage.
