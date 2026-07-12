# Engine Asset Pipeline Architecture Plan

## Purpose

This document defines the target architecture for the engine asset pipeline.

It is based on the current implementation described in `asset-pipeline-analysis.md`, where:

- OBJ files are loaded directly into runtime mesh resources.
- PNG/JPG textures are decoded and uploaded directly at runtime.
- Materials already exist as runtime objects with fixed texture roles and PBR parameters.
- Material data is not imported from files.
- OBJ material assignments, shape boundaries, names, transforms, and hierarchy are discarded.
- Runtime resources currently combine durable asset data with GPU-specific state.

The goal is to introduce a clean, engine-owned boundary between source assets and runtime resources while preserving current OBJ + PNG workflows and adding Blender/glTF support without creating format-specific runtime paths.

---

# 1. Core Design Principle

The runtime must not care where an asset came from.

The following source paths must converge on the same engine-owned data model:

```text
OBJ + MTL + PNG
        |
        v
ObjAssetImporter
        |
        v
Engine Asset Data

GLB / glTF + textures
        |
        v
GltfAssetImporter
        |
        v
Engine Asset Data
```

Engine asset data is then validated, cooked, serialized, loaded, and converted into runtime resources:

```text
Engine Asset Data
        |
        v
Asset Cooker
        |
        v
.gmesh / .gmat / .gtex / optional package
        |
        v
Runtime Asset Loaders
        |
        v
MeshResource / MaterialInstance / TextureResource
        |
        v
Renderer
```

Source-format concepts must not leak into runtime code.

The renderer must never need to know whether geometry came from OBJ, glTF, Blender, procedural generation, or another importer.

---

# 2. Representation Layers

The architecture should explicitly separate four representations.

## 2.1 Source Assets

Files authored by artists or developers:

```text
robot.obj
robot.mtl
robot.png
robot.glb
robot.blend
```

`.blend` remains an authoring format.

Blender should export to GLB/glTF. The engine toolchain should not parse `.blend` directly.

Source assets may contain source-specific details that the engine does not support.

---

## 2.2 Imported Asset Data

Temporary, CPU-only, tooling-side data produced by source importers.

This representation is flexible enough to preserve information before the cooker decides what the engine supports.

Suggested top-level type:

```cpp
struct AssetImportResult
{
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedTexture> textures;
    std::vector<ImportedNode> nodes;
    std::vector<AssetDependency> dependencies;
    std::vector<AssetDiagnostic> diagnostics;
};
```

The name `AssetImportResult` is preferred over `ImportedScene` because this system is asset-oriented and may import a model package without representing a game scene.

Imported data must contain no Vulkan handles, descriptor sets, ECS entities, or runtime manager handles.

---

## 2.3 Cooked Asset Data

Stable, engine-owned, serializable data.

Cooked data contains only engine concepts and is suitable for deterministic serialization.

Suggested asset types:

```text
MeshAssetData
MaterialAssetData
TextureAssetData
ModelAssetData or AssetPackageData
```

Cooked asset data is the contract between tools and runtime.

It must be:

- Versioned.
- Validated.
- Deterministic.
- Platform-aware where necessary.
- Independent of source format.
- Independent of Vulkan object lifetimes.
- Safe to deserialize.
- Addressable through stable asset identifiers.

---

## 2.4 Runtime Resources

Process-specific and GPU-backed objects:

```text
MeshResource
MaterialInstance / MaterialGpuState
TextureResource
```

Runtime resources may contain:

- Vulkan buffers.
- Vulkan images.
- Image views.
- Samplers.
- Descriptor sets.
- Runtime handles.
- Cache state.
- Lifetime state.

Runtime resources must never be serialized directly.

---

# 3. Main Subsystems

## 3.1 Importer Interface

Every source importer implements the same interface.

```cpp
struct AssetImportRequest
{
    std::filesystem::path sourcePath;
    AssetImportOptions options;
};

class IAssetImporter
{
public:
    virtual ~IAssetImporter() = default;

    virtual bool supports(
        const std::filesystem::path& sourcePath) const = 0;

    virtual AssetImportResult import(
        const AssetImportRequest& request) = 0;
};
```

Initial importers:

```text
ObjAssetImporter
GltfAssetImporter
ImageAssetImporter
```

Later importers may include:

```text
FontAssetImporter
ParticleAssetImporter
UiAssetImporter
AudioAssetImporter
```

An importer understands foreign/source data.

It does not:

- Create Vulkan resources.
- Register ECS components.
- Allocate descriptor sets.
- Create render commands.
- Own runtime resource handles.
- Serialize final cooked files directly.

---

## 3.2 Importer Registry

A registry selects an importer based on source file type and optional explicit importer configuration.

```cpp
class AssetImporterRegistry
{
public:
    void registerImporter(std::unique_ptr<IAssetImporter> importer);

    IAssetImporter* findImporter(
        const std::filesystem::path& sourcePath) const;
};
```

The registry must support explicit overrides because extension-based selection may be insufficient for project-specific manifests.

---

## 3.3 Asset Cooker

The cooker converts imported data into cooked engine asset data.

Responsibilities include:

- Validation.
- Triangulation verification.
- Coordinate-system conversion.
- Unit conversion.
- Normal generation.
- Tangent generation.
- Vertex deduplication.
- Vertex format conversion.
- Index format selection.
- Bounds generation.
- Submesh creation.
- Material-role mapping.
- Texture color-space assignment.
- Mipmap generation.
- Texture compression when added.
- Asset ID assignment.
- Dependency resolution.
- Serialization.
- Diagnostic reporting.

The cooker should be callable from:

- A command-line tool.
- The editor.
- Tests.
- A future Blender integration wrapper.
- Build automation.

Suggested command:

```text
assetc import resources/models/robot.glb --output build/assets
```

---

## 3.4 Runtime Asset Loaders

Runtime loaders understand only cooked engine formats.

Suggested interfaces:

```cpp
class MeshAssetLoader;
class MaterialAssetLoader;
class TextureAssetLoader;
```

Runtime loading path:

```text
.gmesh
  -> MeshAssetData
  -> MeshManager
  -> MeshResource
  -> Vulkan buffers
```

```text
.gmat
  -> MaterialAssetData
  -> MaterialRuntime
  -> MaterialInstance
  -> MaterialGpuState
```

```text
.gtex
  -> TextureAssetData
  -> TextureManager
  -> TextureResource
  -> Vulkan image/view/sampler
```

The shipping runtime should eventually not depend on:

- tinyobj.
- glTF parsing libraries.
- general source-image decoders for cooked textures.
- source-format validation logic.

During migration, legacy runtime loading may remain temporarily available behind the same asset service.

---

# 4. Engine Asset Data Model

## 4.1 Asset IDs

Cooked references must use stable IDs rather than raw source paths wherever possible.

Possible initial representation:

```cpp
using AssetId = uint64_t;
```

Asset IDs may be generated from normalized logical asset paths, GUIDs, or an asset database.

Requirements:

- Stable across rebuilds.
- Independent of runtime memory addresses.
- Serializable.
- Collision-checked.
- Usable for dependency tracking.

A path-to-ID mapping can remain in metadata for debugging and development.

---

## 4.2 Mesh Asset

Suggested representation:

```cpp
struct MeshAssetData
{
    AssetId id;
    std::string debugName;

    VertexAttributeFlags attributes;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubmeshAssetData> submeshes;

    Bounds bounds;

    bool generatedNormals = false;
    bool generatedTangents = false;
};
```

```cpp
struct SubmeshAssetData
{
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    AssetId materialId = InvalidAssetId;
};
```

The first version may reuse the current `Vertex` structure to reduce migration cost.

Long-term, cooked vertex layout should be allowed to differ from the current CPU authoring/import layout.

The cooked format must contain engine concepts only.

It must not contain:

- OBJ shape structures.
- tinyobj material indices.
- glTF accessor indices.
- Blender object internals.
- Source parser pointers.

---

## 4.3 Material Asset

The existing runtime material system already supports a useful PBR baseline.

The material asset should serialize the durable portion of `MaterialInstance`.

```cpp
struct MaterialAssetData
{
    AssetId id;
    std::string debugName;

    MaterialShaderFamily shaderFamily;

    glm::vec4 baseColor;
    float metallic;
    float roughness;
    float normalScale;
    float ambientOcclusionStrength;

    glm::vec3 emissiveFactor;
    float emissiveStrength;

    AssetId baseColorTexture;
    AssetId metallicRoughnessTexture;
    AssetId normalTexture;
    AssetId ambientOcclusionTexture;
    AssetId emissiveTexture;

    MaterialRenderState renderState;
    bool vertexColorOnly;
};
```

The material asset must not contain:

- Resolved runtime texture IDs.
- Descriptor sets.
- Material GPU handles.
- Material frame state.
- Runtime version counters that are not part of the asset contract.

Texture color space should either be:

- Defined by the texture asset, or
- Explicitly specified per material binding where reinterpretation is required.

Expected role defaults:

```text
BaseColor             -> sRGB
Emissive              -> sRGB
MetallicRoughness     -> Linear
Normal                -> Linear
AmbientOcclusion      -> Linear
```

---

## 4.4 Texture Asset

The first migration stage may keep PNG/JPG as runtime dependencies, but the final architecture should define cooked texture data.

Suggested representation:

```cpp
struct TextureAssetData
{
    AssetId id;
    std::string debugName;

    uint32_t width;
    uint32_t height;
    uint32_t mipCount;

    TextureColorSpace colorSpace;
    TextureFormat format;

    TextureSamplerDesc defaultSampler;

    std::vector<TextureMipData> mips;
};
```

The final cooked texture path should support:

- Correct sRGB/linear handling.
- Mipmaps.
- Platform-appropriate GPU compression.
- Explicit sampler defaults.
- Deterministic processing.
- No runtime PNG/JPG decoding requirement.

A staged implementation is acceptable:

```text
Stage 1:
.gmat references source PNG/JPG
Runtime TextureManager remains unchanged

Stage 2:
ImageAssetImporter + TextureCooker produce .gtex
.gmat references .gtex AssetIds
Runtime loads cooked texture payloads
```

This is a staged implementation of one architecture, not a separate temporary architecture.

---

## 4.5 Model or Package Asset

A source file may produce multiple meshes and materials.

A logical model/package asset can describe their relationship without becoming a game scene.

```cpp
struct ModelAssetData
{
    AssetId id;
    std::string debugName;

    std::vector<ModelNodeAssetData> nodes;
    std::vector<AssetId> meshIds;
    std::vector<AssetId> materialIds;
};
```

```cpp
struct ModelNodeAssetData
{
    std::string name;

    int32_t parentIndex;
    glm::mat4 localTransform;

    std::vector<ModelPrimitiveBinding> primitives;
};
```

This layer is optional for the first milestone if only single-object static meshes are needed.

However, the import representation should not prevent adding it.

The term `ModelAssetData` or `AssetPackageData` is preferable to `SceneAssetData` unless the asset explicitly represents a level or scene.

---

# 5. OBJ + PNG Compatibility

OBJ remains a supported source format.

It must be moved behind the importer/cooker boundary rather than removed.

## 5.1 Current Path

```text
OBJ
  -> tinyobj
  -> Vertex / index arrays
  -> MeshResource
  -> Vulkan buffers
```

## 5.2 Target Path

```text
OBJ + optional MTL + optional texture overrides
  -> ObjAssetImporter
  -> AssetImportResult
  -> Asset Cooker
  -> .gmesh / .gmat / texture dependency
  -> Runtime loaders
  -> MeshResource / MaterialInstance / TextureResource
```

## 5.3 OBJ + MTL

Where MTL data exists, map supported fields into engine material concepts.

Possible mapping:

```text
Kd                -> baseColor
map_Kd            -> baseColorTexture
d / Tr            -> opacity / alpha handling
Ke                -> emissiveFactor
map_Ke            -> emissiveTexture
map_Bump / bump   -> normal texture with importer diagnostics
```

MTL is not metallic-roughness PBR.

Defaults should be explicit and importer-configurable:

```text
metallic  = 0.0
roughness = configured default
AO        = none
```

Unsupported or ambiguous mappings must produce diagnostics rather than silently guessing.

---

## 5.4 OBJ + Explicit PNG

The existing project convention of pairing an OBJ with a PNG must be supported through import settings or an asset manifest.

Example manifest:

```json
{
  "source": "robot.obj",
  "importer": "obj",
  "materialOverrides": {
    "default": {
      "shaderFamily": "Unlit",
      "baseColorTexture": "robot.png"
    }
  }
}
```

This association must not depend solely on matching filenames unless that behavior is explicitly configured as a project convention.

---

## 5.5 Vertex Color Assets

Vertex-color-only OBJ assets remain supported.

They should create ordinary engine material data:

```text
shaderFamily   = Unlit
vertexColorOnly = true
```

Vertex colors must be included in vertex deduplication keys where relevant.

---

# 6. glTF / Blender Path

Blender workflow:

```text
.blend
  -> Blender GLB export
  -> GltfAssetImporter
  -> AssetImportResult
  -> Asset Cooker
  -> engine cooked assets
```

GLB/glTF should initially support:

- Static meshes.
- Multiple mesh primitives.
- Positions.
- Normals.
- Tangents.
- Vertex colors.
- UV0.
- Indices.
- Material assignments.
- Base-color textures and factors.
- Metallic/roughness textures and factors.
- Normal maps and normal scale.
- AO.
- Emissive.
- Alpha mode.
- Double-sided state.
- Node names.
- Node-local transforms.
- Texture dependencies.

Deferred features:

- Additional UV sets.
- Skeletal animation.
- Skins and joints.
- Morph targets.
- Cameras.
- Lights.
- glTF extensions not required by the engine.
- Full scene/level import.

Unsupported features must generate clear diagnostics.

---

# 7. Procedural Meshes

Procedural geometry does not need a source importer.

It should still converge on the same runtime resource creation boundary.

Suggested runtime-facing upload type:

```cpp
struct MeshUploadDescription
{
    std::span<const Vertex> vertices;
    std::span<const uint32_t> indices;
    VertexAttributeFlags attributes;
    Bounds bounds;
    bool dynamic;
};
```

Paths:

```text
.gmesh
  -> MeshAssetLoader
  -> MeshUploadDescription
  -> MeshManager
  -> MeshResource
```

```text
DynamicMeshComponent
  -> MeshUploadDescription
  -> MeshManager
  -> MeshResource
```

This preserves procedural meshes while avoiding serialization requirements for transient data.

---

# 8. Serialization Format

The cooked format must be explicitly versioned.

Do not serialize C++ objects through raw memory dumps.

Do not serialize pointers, STL container layouts, Vulkan types, or padding-dependent structs.

## 8.1 Typed Files

Recommended initial files:

```text
.gmesh
.gmat
.gtex
.gmodel or .gasset
```

Typed files align with the current separate managers and allow independent caching and sharing.

## 8.2 Common Header

Example:

```cpp
struct AssetFileHeader
{
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t assetType;

    uint32_t flags;
    uint64_t assetId;

    uint64_t payloadSize;
    uint64_t dependencyTableOffset;
};
```

Requirements:

- Magic number.
- Asset type.
- Format version.
- Endianness policy.
- Payload bounds validation.
- Dependency table.
- Optional checksum.
- Optional compression flags.
- Clear migration strategy.

## 8.3 References

References between cooked assets use `AssetId`.

Example:

```text
.gmesh submesh -> .gmat AssetId
.gmat texture role -> .gtex AssetId
.gmodel node -> .gmesh AssetId
```

---

# 9. Asset Database and Dependency Tracking

The toolchain needs a record of:

- Source file.
- Importer.
- Import settings.
- Generated cooked assets.
- Dependencies.
- Source hashes or timestamps.
- Importer/cooker version.
- Platform target.
- Diagnostics.

Suggested metadata record:

```cpp
struct AssetBuildRecord
{
    AssetId sourceAssetId;
    std::filesystem::path sourcePath;

    std::string importerName;
    uint32_t importerVersion;

    AssetImportOptions options;

    std::vector<AssetDependency> sourceDependencies;
    std::vector<AssetId> generatedAssets;

    ContentHash sourceHash;
    ContentHash buildHash;
};
```

A rebuild should occur when any of these change:

- Source file content.
- Referenced texture content.
- MTL content.
- Import settings.
- Importer version.
- Cooker version.
- Target platform.
- Cooked format version.

---

# 10. Diagnostics and Validation

Importing must return structured diagnostics.

```cpp
enum class AssetDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

struct AssetDiagnostic
{
    AssetDiagnosticSeverity severity;
    std::string code;
    std::string message;
    std::filesystem::path sourcePath;
};
```

Examples:

```text
OBJ_MISSING_NORMALS
OBJ_AMBIGUOUS_TEXTURE_OVERRIDE
GLTF_UNSUPPORTED_UV_SET
GLTF_UNSUPPORTED_EXTENSION
MATERIAL_NORMAL_MAP_WITHOUT_TANGENTS
TEXTURE_COLOR_SPACE_CONFLICT
MESH_EMPTY_PRIMITIVE
ASSET_DEPENDENCY_MISSING
```

The cooker must refuse to generate invalid output for fatal errors.

Warnings must be visible to developers and ideally stored in build metadata.

---

# 11. Migration Strategy

The migration should preserve a working engine throughout.

## Phase 1 - Define the Engine Asset Boundary

Create CPU-only types:

```text
AssetImportResult
ImportedMesh
ImportedMaterial
ImportedTexture
MeshAssetData
MaterialAssetData
AssetDependency
AssetDiagnostic
```

Define importer and cooker interfaces.

No renderer changes yet.

Acceptance criteria:

- Types contain no Vulkan state.
- Types contain no ECS entities.
- Types contain no source-parser handles.
- Tests can construct and validate asset data without a GPU.

---

## Phase 2 - Refactor OBJ Import

Move tinyobj parsing behind `ObjAssetImporter`.

The importer produces `AssetImportResult`.

Preserve:

- Positions.
- Normals.
- UV0.
- Vertex colors.
- Indices.
- Existing UV V-flip behavior, unless coordinate policy intentionally changes.
- Normal and tangent generation behavior through the cooker.
- Existing OBJ + PNG workflows through explicit overrides/manifests.
- Vertex-color-only workflows.

Improve:

- Retain shape/primitive boundaries as submeshes.
- Retain material assignments.
- Include vertex color in deduplication where needed.
- Emit diagnostics for missing or unsupported data.

Acceptance criteria:

- Existing OBJ assets render identically or with documented corrections.
- OBJ no longer creates Vulkan resources directly.
- OBJ importer is unit-testable without a Vulkan device.

---

## Phase 3 - Cook and Serialize Mesh/Material Assets

Implement:

```text
MeshAssetData -> .gmesh
MaterialAssetData -> .gmat
```

Implement runtime loaders that reconstruct:

```text
.gmesh -> MeshResource
.gmat -> MaterialInstance
```

Keep source textures temporarily if necessary.

Acceptance criteria:

- Runtime can render an OBJ-derived asset without parsing OBJ.
- Runtime can create its material without C++ hard-coded construction.
- Cooked output is deterministic.
- Version mismatch produces a clear error.
- Malformed files are rejected safely.

---

## Phase 4 - Add glTF/GLB Import

Implement `GltfAssetImporter`.

Map glTF data into the same `AssetImportResult` used by OBJ.

Acceptance criteria:

- A Blender-exported GLB renders through the same cooked runtime path as OBJ.
- Multiple primitives and material assignments work.
- Supported PBR material roles populate the existing material system.
- Unsupported data produces diagnostics.
- Renderer code contains no glTF-specific branches.

---

## Phase 5 - Cook Textures

Implement:

```text
ImageAssetImporter
TextureCooker
.gtex serializer
TextureAssetLoader
```

Add:

- Mipmaps.
- Color-space validation.
- Texture format metadata.
- Compression strategy.
- Sampler defaults or explicit material sampler policy.

Acceptance criteria:

- Runtime does not decode PNG/JPG for cooked game assets.
- Base color/emissive use sRGB correctly.
- Data textures use linear formats.
- Mips are present.
- Texture build output is deterministic.

---

## Phase 6 - Asset Database and Incremental Builds

Implement:

- Stable IDs.
- Dependency graph.
- Build records.
- Incremental rebuilds.
- Cache invalidation.
- Source-to-cooked mapping.
- Human-readable diagnostics.

Acceptance criteria:

- Editing a PNG rebuilds dependent materials/textures.
- Editing import settings rebuilds affected cooked assets.
- Unchanged assets are not rebuilt.
- Removing a dependency produces a useful error.

---

## Phase 7 - Optional Model/Package Assets

Add a model/package layer when hierarchy and multi-object instantiation are needed.

Acceptance criteria:

- One GLB can produce multiple mesh/material assets.
- A model/package asset can instantiate their relationships.
- Game-level scenes remain separate from model/package assets.

---

# 12. Recommended First Implementation Slice

The first implementation should be narrow but architecturally final.

Build:

1. `AssetImportResult`.
2. `ImportedMesh`.
3. `ImportedMaterial`.
4. `MeshAssetData`.
5. `MaterialAssetData`.
6. `IAssetImporter`.
7. `ObjAssetImporter`.
8. `AssetCooker` mesh/material conversion.
9. `.gmesh` serializer/deserializer.
10. `.gmat` serializer/deserializer.
11. Runtime bridges into `MeshManager` and `MaterialRuntime`.
12. Tests for round-trip serialization and existing OBJ behavior.

Keep temporarily:

- Existing `Vertex` layout.
- Existing `TextureManager`.
- Runtime PNG/JPG loading behind material texture references.
- Existing renderer.
- Existing ECS binding model.

Do not include in the first implementation:

- glTF.
- Texture compression.
- Skeletal animation.
- Additional UV sets.
- Model hierarchy instantiation.
- Editor UI.
- Hot reload redesign.
- Asset package compression.

This is not a throwaway half-solution.

It establishes the final boundaries and migrates one existing source path end to end.

---

# 13. Testing Strategy

## Importer Tests

Use small fixture assets for:

- OBJ with positions only.
- OBJ with normals.
- OBJ with UVs.
- OBJ with vertex colors.
- OBJ with multiple shapes.
- OBJ with multiple materials.
- OBJ + MTL.
- OBJ + explicit PNG override.
- Invalid OBJ.
- Missing dependency.

Validate imported CPU data without Vulkan.

## Cooker Tests

Validate:

- Normal generation.
- Tangent generation.
- Attribute flags.
- Bounds.
- Submesh ranges.
- Material references.
- Default PBR values.
- Deterministic output.

## Serialization Tests

Validate:

- Round trip.
- Version rejection.
- Truncated file rejection.
- Invalid offsets.
- Oversized counts.
- Missing dependencies.
- Stable byte output.

## Runtime Integration Tests

Validate:

- `.gmesh` creates a valid `MeshResource`.
- `.gmat` creates a valid `MaterialInstance`.
- Existing OBJ-derived assets render through cooked data.
- Procedural meshes still use the same resource manager.
- Renderer behavior is unchanged.

---

# 14. Non-Goals and Guardrails

Do not:

- Parse `.blend` directly.
- Make glTF the runtime format.
- Keep OBJ as a special renderer path.
- Serialize `MeshResource` directly.
- Serialize Vulkan handles.
- Put importer logic in `MeshManager`.
- Put source format fields in cooked assets.
- Make material assets equal to texture files.
- Introduce a general game-scene format before model/package needs are clear.
- Couple procedural meshes to disk serialization.
- Rewrite the renderer as part of the asset-pipeline migration.

---

# 15. Final Target Architecture

```text
                           SOURCE / AUTHORING

       .obj + .mtl + images                .blend
                 |                            |
                 |                            v
                 |                      Blender GLB export
                 |                            |
                 v                            v
        ObjAssetImporter              GltfAssetImporter
                 \                          /
                  \                        /
                   v                      v
                     AssetImportResult
             meshes / materials / textures / nodes
                             |
                             v
                         Asset Cooker
              validation / conversion / dependencies
                             |
                             v
                 Engine Cooked Asset Data
           MeshAsset / MaterialAsset / TextureAsset
                             |
                             v
              .gmesh / .gmat / .gtex / .gmodel
                             |
                     RUNTIME BOUNDARY
                             |
                             v
                    Runtime Asset Loaders
                             |
             +---------------+----------------+
             |               |                |
             v               v                v
       MeshManager     MaterialRuntime    TextureManager
             |               |                |
             v               v                v
       MeshResource    MaterialInstance   TextureResource
             \               |                /
              \              |               /
                       Renderer
```

The central invariant is:

> Source formats are tooling concerns. Cooked assets are engine contracts. Runtime resources are process-specific realizations of those contracts.
