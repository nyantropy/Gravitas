# Canonical glTF / GLB Importer

`modules/assets/importer/gltf/GtsGltfModelImporter` implements `IGtsModelImporter`
and returns only `GtsModelImportResult -> GtsModelAsset`. It handles `.gltf` JSON
and `.glb` version 2 containers. No legacy model DTO, renderer vertex, image decoder,
Vulkan object, or runtime/cooked resource is involved.

```text
.gltf / .glb
    -> shared source utilities / GLB framing
    -> GltfSourceReader (JSON, buffers, views, validated accessors)
    -> GtsGltfModelImporter (source interpretation)
    -> canonical validation -> GtsModelImportResult -> GtsModelAsset
============================ FORMAT WALL =============================
    future consumer cutover (not implemented by this change)

existing production glTF route remains:
GltfAssetImporter -> AssetImportResult -> existing cooker -> cooked v1
```

## Decoder ownership and reuse

`gravitas_gltf_importer` links the canonical asset/core JSON target and the small
standard-library-only `gravitas_gltf_source` target. It builds with rendering and
Vulkan disabled. Its public importer header includes only the Strategy interface.
Source reader/utility headers are internal implementation details in this folder.

File byte reads, little-endian word reads, base64/data-URI decoding, component
sizes/counts, and integer normalization were extracted from the legacy importer.
Both importers now share GLB framing, including strengthened total-length, chunk
range/alignment, JSON-first/unique, and BIN-second/unique checks. Unknown chunks
are skipped; the canonical reader reports them. Valid legacy input behavior is
preserved; malformed GLBs previously accepted by that parser can now fail.

Legacy accessor-table loading, buffer orchestration, float-based index reads,
material/mesh conversion, and node orchestration remain temporarily separate.
Those functions are intertwined with legacy DTOs and dependency diagnostics;
changing their contracts would broaden this step into the pending consumer cutover.
The canonical reader supplies the strict replacement, including direct unsigned
index reads. The remaining legacy orchestration should be removed at cutover,
not maintained as an alternative implementation.

## Supported geometry

- POSITION / NORMAL: float VEC3 -> Position[0] / Normal[0].
- TANGENT: float VEC4 -> Tangent[0].
- TEXCOORD_n: float or normalized unsigned byte/short VEC2 -> TexCoord[n].
- COLOR_n: float or normalized unsigned byte/short VEC3/VEC4 -> Color[n], with
  implicit alpha 1 for RGB colors.
- JOINTS_n: non-normalized unsigned byte/short VEC4 -> integer Joints[n].
- WEIGHTS_n: float or normalized unsigned byte/short VEC4 -> Weights[n].

Numbered sets are preserved without a fixed maximum count. glTF sets must start
at zero, be consecutive, and have no leading-zero spelling. Joint/weight sets
must pair and every stream must match POSITION's count. Colors and weights are
finite values in [0,1]; weights are preserved without renormalization or truncation.
Unknown/custom attributes fail explicitly because the canonical domain has no
opaque/custom semantic storage. Quantized position/normal extensions are not
claimed as supported.

Only triangle lists are accepted. Each source mesh and primitive retains its
boundary and source array index; no invalid mesh is skipped or compacted. Indices
must be tightly packed, non-normalized unsigned byte/short/int SCALAR values.
They decode directly to uint32, with vertex-bound and restart-value checks.
Unindexed primitives receive sequential indices. Missing normals, tangents,
colors, and UVs remain absent; nothing is generated or repaired during import.

Positions, normals, tangent XYZ, and local node transforms have no axis conversion.
Each UV set flips V (`1-v`), and tangent W is negated once to match that convention.
Transforms remain node-local; they are never baked into vertices.

## Nodes and scene selection

The complete source forest is checked for invalid child/mesh references,
duplicate children, multiple parents, and cycles, including inactive nodes.
Matrix and TRS forms are supported, mutually exclusively. Matrices use glTF's
column-major order; TRS composes translation * quaternion rotation * scale.
Vector lengths, finite values, affine matrices, quaternion unit length, and
composition overflow are checked.

The default `scene` selects its explicit root list. Without a default, scene 0 is
selected with `GLTF_SCENE_DEFAULT`. With no scenes, the file is treated as an
entity library: all existing parentless node trees are preserved with a diagnostic;
mesh-only libraries gain no invented nodes. Scene root lists must reference valid,
unique parentless nodes. An invalid explicit scene fails.

Only nodes reachable from the selected scene are returned, in original source
order, with child/root indices remapped. Excluded nodes produce a diagnostic.
Meshes, materials, and images remain a shared library and mesh references retain
their original indices. This is necessary because canonical node validation
requires every returned node to be reachable from a root; there is no multi-scene
container or inactive-node state in `GtsModelAsset`.

Scene semantics and data encoding follow the
[glTF 2.0 specification](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc).
Choosing scene 0 when no default is declared is an importer policy, not a claim
that glTF requires that choice.

## Materials and encoded images

PBR base color, metallic/roughness factors, emissive factor/strength, normal scale,
AO strength, alpha mode/cutoff, and double-sided appearance map to canonical values.
`KHR_materials_emissive_strength` maps to the ordinary emissive-strength field.
Source defaults are resolved here: glTF metallic defaults to 1 even when no PBR
object exists. Primitives without a source material share an appended canonical
default material with metallic 1. Explicit source material indices are validated
before that synthetic default is appended, avoiding accidental reference repair.

Material texture objects resolve to image indices with each binding's `texCoord`
selection retained. Metallic/roughness bindings share an image but independently
select Blue/Green; AO selects Red. Import performs no packing or texture cooking.

External URI paths are percent-decoded and resolved to absolute normalized local
paths. Missing external images warn and retain their identity. Data-URI and
buffer-view images preserve encoded bytes and MIME metadata. Identical external
paths or identical embedded bytes/MIME share one canonical image entry; the first
source name wins. No image pixels are decoded. Local files and base64 data URIs
are supported; network URI schemes are rejected.

Explicit sampler references are validated and warn with `GLTF_SAMPLER_UNSUPPORTED`.
Sampler objects, filters, wrapping policy, source texture objects, and texture
transforms are not stored in canonical materials. Optional texture-transform or
other unsupported extensions are diagnosed rather than silently claimed supported.

## Failure policy and validation limits

- All accessors are checked against both their buffer-view range and the buffer's
  declared length, independently of extra backing-file bytes or GLB padding.
  Count/range calculations avoid overflow. Stride, component alignment, vector
  dimensions, accessor references, and semantic encodings are validated.
- Declared malformed optional streams fail; they do not become absent streams.
- Sparse accessors fail with `GLTF_SPARSE_UNSUPPORTED`; expand them first.
  Accessors without a bufferView also fail with a materialization diagnostic.
  Matrix accessor forms other than MAT4 are currently unsupported, even if unused.
- Actual node skin references fail with `GLTF_SKIN_BINDING_UNSUPPORTED` because
  essential binding data cannot be retained. Unreferenced skin tables warn.
  Joint/weight geometry without node skin references is allowed.
- Morph targets or authored mesh/node morph weights fail. Nonempty animations
  fail, including animations outside the selected scene. No deformation data is
  silently treated as equivalent static content.
- Unknown required extensions fail. Unknown optional extensions warn once per
  name, including extensions attached without a top-level declaration.
  Cameras warn; their node transforms can still be represented.
- Canonical success validation remains unchanged and runs last. Numeric material
  ranges, image bindings, required material UV sets, topology counts, finite
  geometry/transforms, and hierarchy must all satisfy that contract.

Decoder errors unwind privately to the importer boundary, which converts them
into structured errors with source/accessor/mesh/primitive/node/material context.
Ordinary file/format failures do not escape as exceptions or successful partial assets.
This is a supported-feature importer, not a complete glTF conformance validator.

## Tests and later cutover

`tests/assets/importers/gltf/GtsGltfModelImporterTest` uses generated JSON/binary
fixtures and links only the canonical importer. It covers both containers, all
buffer/image input forms, interleaving, integer normalization and index encodings,
semantic sets, material factors/channels, scene selection/remapping/transforms,
malformed ranges/GLB/hierarchy, explicit unsupported policies, and deterministic
canonical geometry. Assertions remain active in Release through explicit checks.

No cooker, runtime loader, static preparation, realization, cooked format,
renderer, or ECS consumer was migrated. The importer result still lacks a source
provenance/dependency carrier; external buffer and source-file dependencies cannot
be exported as a manifest. Image identity remains available in the model.

Before cutover, general model realization must preserve glTF hierarchy/instancing
(the current OBJ adapter accepts only flat identity roots). Nonzero material UV
sets need an explicit downstream capability policy. Sampler/texture transforms,
scene selection options, sparse support, and separate skin/animation/morph contracts
remain focused future decisions; none were added to the canonical model here.
