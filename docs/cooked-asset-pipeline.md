# Cooked Asset Pipeline

This document describes the Phase 3 cooked asset implementation.

## Runtime Flow

OBJ source assets are tooling input. Runtime rendering can now consume cooked
engine assets:

```text
OBJ
  -> ObjAssetImporter
  -> AssetImportResult
  -> AssetCooker
  -> .gmesh
  -> MeshAssetLoader
  -> MeshManager
  -> MeshResource
  -> Renderer
```

```text
OBJ + MTL
  -> ObjAssetImporter
  -> AssetImportResult
  -> AssetCooker
  -> .gmat
  -> MaterialAssetLoader
  -> MaterialRuntime
  -> MaterialInstance
  -> Renderer
```

`MeshManager::loadMesh` prefers an adjacent `.gmesh` when a source mesh path is
requested and falls back to the existing OBJ path when no cooked mesh exists.
`MaterialReferenceHelpers` treats descriptor paths ending in `.gmat` as cooked
material assets and loads them through `MaterialAssetLoader`.

## Cooked Asset Contracts

Cooked data is stored in engine-owned CPU structures in
`engine/modules/rendering/core/assets/AssetTypes.h`.

- `AssetReference` stores a placeholder stable `AssetId` plus `logicalPath`.
- `MeshAssetData` stores vertices, indices, submeshes, dependencies, bounds,
  attribute flags, and generated-normal/tangent flags.
- `SubmeshAssetData` stores `firstIndex`, `indexCount`, `material`, and a debug
  name.
- `MaterialAssetData` stores shader family, render state, scalar material
  factors, texture references, dependencies, and `vertexColorOnly`.

These structures contain no Vulkan types, descriptor sets, ECS entities,
runtime manager handles, parser-library types, or GPU cache state.

## File Layout

Serializers live in `engine/modules/rendering/core/assets/AssetSerializers.*`.
Both `.gmesh` and `.gmat` are explicit little-endian binary formats. C++ object
memory is not dumped directly.

Every cooked file begins with a 64-byte header:

```text
offset  size  field
0       4     magic: 0x54534147 ("GAST")
4       2     format version
6       2     asset type: 1 mesh, 2 material
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

The dependency table is a counted list of `AssetReference` values. It is stored
outside the payload and referenced by validated header offsets.

## Versioning

The first supported versions are:

- `.gmesh`: `MeshAssetFormatVersion = 1`
- `.gmat`: `MaterialAssetFormatVersion = 1`

Deserializers reject unsupported versions, invalid magic, wrong asset type,
truncated files, invalid payload/dependency offsets, invalid counts, corrupted
dependency tables, and invalid submesh ranges.

## Cooker

`assetc` is the command-line cooker:

```text
assetc import robot.obj --output build/assets
```

Supported options:

- `--importer <name>`
- `--base-color-texture <path>`
- `--vertex-color-only`

The cooker selects an importer, imports source data, converts imported CPU data
to cooked `MeshAssetData`/`MaterialAssetData`, computes bounds, preserves OBJ
primitive/submesh material assignments, serializes `.gmesh` and `.gmat`, and
emits structured diagnostics. It does not create Vulkan resources or ECS
entities.

## Dependency Handling

Cooked assets use `AssetReference`, not raw runtime handles. Stable asset IDs
are placeholders derived from logical paths so future real asset IDs can replace
path lookup without changing the cooked structures.

Generated material references use cooked output filenames as logical paths.
Texture references use source-relative logical paths when possible and retain a
logical path fallback otherwise. Texture cooking is not implemented in Phase 3.

## Remaining Direct Source Loads

These runtime paths still load source assets directly for compatibility:

- `MeshManager::loadMesh` falls back to `GtsModelLoader`/`ObjAssetImporter` when
  no cooked `.gmesh` exists.
- Texture loading still goes through `TextureManager` and reads image files
  directly; cooked `.gtex` does not exist yet.
- Game-side material descriptors that name PNG files still create runtime
  `MaterialInstance` objects directly from texture paths.
