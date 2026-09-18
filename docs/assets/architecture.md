# Asset subsystem ownership

The filesystem follows asset responsibilities:

```text
assets/
  importer/             source-format interpretation
    obj/, gltf/         canonical model importers
    StbImageImplementation.cpp  shared CPU image decoder implementation
  processing/           canonical data → prepared CPU profiles
    geometry/static/
    geometry/skinned/
  serialization/        cooked storage contracts, metadata and byte codecs
  loading/              source/cooked policy, loading and resource retention
    RuntimeAssetPolicy.h
    model/              authoritative high-level model registry/resource
    mesh/               existing lower-level static mesh loading
    cooked/             CPU .gmesh/.gmodel/.gmat/.gtex decoding wrappers
  cooking/              source/canonical/prepared data → cooked persistence
    legacy/             existing cooker-only glTF/image DTO import architecture
  realization/          existing flat static CPU model-to-mesh adapter
    model/              complete-model CPU profiles and occurrence associations
```

Canonical model, skeleton, skin and animation domains remain separate data/validation
modules. Neither canonical importers nor geometry processing load runtime resources.
Rendering consumes CPU asset products, owns material/runtime realization and then
creates backend/GPU resources. Loading never invokes Vulkan or `MaterialRuntime`.

## Build ownership

| Target | Responsibility / principal dependencies |
| --- | --- |
| `gravitas_assets` | Canonical model/import-result contracts; no cooker or runtime loader. |
| `gravitas_cooked_assets` | `serialization/AssetSerializers.cpp`; CPU core only. |
| `gravitas_image_decode` | Single stb-image implementation shared by cooking and rendering. |
| `gravitas_model_runtime` | `loading/model`; canonical importers and cooked codecs, no renderer. |
| `gravitas_model_realization` | Complete-model CPU geometry and identity cache; loading contracts and static/skinned preparation. |
| `gravitas_mesh_loading` | `loading/mesh`; existing OBJ/static load path and flat CPU adapter. |
| `gravitas_static_model_realization` | Existing flat identity-root adapter; static processing and cooked contracts. |
| `gravitas_asset_cooking` | Cookers plus explicit legacy DTO importers; importers, CPU processing, codecs and image decoding. |

These are separate targets; `gravitas_assets` does not aggregate all asset systems.
`assetc` links cooking rather than rendering and can build with rendering/Vulkan
disabled. Existing rendering consumers link mesh loading, cooked contracts and the
shared image decoder. Rendering no longer compiles asset loader/cooker/codec sources.

The old CMake-only `assets/storage/` shim, `assets/runtime/` and
`rendering/core/assets/` directories are removed. There are no forwarding headers
at their previous locations. Includes use ownership-qualified `assets/...` paths.

## Small dependency splits

`MaterialAssetLoader` now only decodes a `.gmat` into CPU `MaterialAssetData`.
`rendering/core/material/MaterialAssetRealization.h` contains the existing
`makeInstance`, `loadIntoRuntime` and texture-reference conversion methods. Field
mapping and world-scoped `MaterialRuntime` allocation are unchanged. This is an
extraction of existing behavior, not the future general model-material realization API.

`serialization/AssetMaterialTypes.h` holds the unchanged small enums and
`MaterialRenderState` used by cooked data. `TextureColorSpace.h` is also asset-owned.
Rendering's `MaterialTypes.h` consumes these values, while retaining runtime handles,
instances, GPU/frame data and synchronization types. Serialization no longer includes
that whole renderer contract.

`serialization/MeshAssetGeometry.h` holds the existing bounds/metadata queries,
previously inline in `AssetCooker.h`. The static CPU adapter can use those queries
without depending on cooking. No bounds or geometry algorithm changed.

The reusable `rendering/core/geometry/MeshGeometryProcessor.h` remains a CPU-only
header used by existing processing/procedural/legacy consumers. Its existing location
is separate organization debt; asset targets do not link rendering to use it.

## Legacy import migration inventory

### `cooking/legacy/AssetImporter.h`

This is `IAssetImporter`, `AssetImporterRegistry` and their legacy request/capability
contracts. Production usage is `AssetCooker::cookSourceAsset`, with glTF/image
implementations. `GltfAssetImporterTest` also tests the registry. It returns legacy
`AssetImportResult`, not `GtsModelImportResult`. It remains because the cooker still
uses those contracts. Remove it only after its glTF and image consumers have an
explicit replacement; do not merge it with `IGtsModelImporter`.

### `cooking/legacy/GltfAssetImporter.h/.cpp`

Its sole production entry point is registration by `AssetCooker::cookSourceAsset`
(indirectly used by `assetc`). Tests call it directly and through the cooker. It
produces static `ImportedMesh` buffers, `ImportedMaterial`, `ImportedTexture`, node
hierarchy and dependencies. It preserves legacy UV/tangent conversion and warning
behavior for skipped skins/animations/morphs. It is not a peer of the authoritative
canonical `importer/gltf/GtsGltfModelImporter`.

A cooker cutover is not a one-line replacement: the current canonical
`AssetCooker::cookModelAsset` accepts flat identity-root models through the existing
static adapter, whereas this glTF path emits multi-mesh/hierarchical `.gmodel`
packages. A future task must support canonical hierarchy/instancing in static
cooking and deliberately settle unsupported skeletal data, UV/tangent conventions,
material conversion and diagnostics. No such migration is performed here.

### `cooking/legacy/ImageAssetImporter.h/.cpp`

This performs PNG/JPEG source decoding into the existing `ImportedTexture` carrier
and implements the legacy registry interface. Production callers are
`AssetCooker::cookSourceAsset` and its texture decode/packing paths (including
canonical OBJ cooking). `TextureCooker` consumes decoded texture data; it does not
choose image importers. Existing cooker tests cover PNG/JPEG, embedded inputs,
roles and mip behavior. The legacy location reflects the DTO/interface dependency,
not an assertion that image decoding itself should be discarded. A later image
asset boundary may separate the useful decoder from that legacy interface.

`serialization/AssetTypes.h` still includes the legacy import DTO declarations in
addition to cooked contracts. Their layouts and names were retained intact; splitting
or deleting those DTOs belongs with the consumer migration. Legacy CPU types retain
the `gts::rendering` namespace to avoid a broad unrelated naming migration. Physical
and target ownership are asset-side despite that historical namespace.

## Preserved contracts

- Cooked versions, field order, IDs/references and golden fingerprints are unchanged.
- High-level model source/cooked selection, capabilities, cache/provenance semantics
  and canonical/prepared backing are unchanged; see [model loading](model-runtime.md).
- Shared source policy has one implementation under `loading/RuntimeAssetPolicy.h`;
  callers use `gts::assets` directly. Strict/development/shipping behavior is unchanged.
- Lower-level mesh/procedural APIs, Yune's temporary bridge, playback and rendering
  behavior are unchanged.
- The ownership cleanup did not change cooker semantics or material behavior.
  Complete-model CPU geometry now has its own realization boundary below.

Historical implementation reports retain their old paths as historical inventories;
this document and the feature documentation describe current ownership.

Complete-model realization is documented in [model realization](model-realization.md).
