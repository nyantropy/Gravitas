# Canonical glTF / GLB Importer

`modules/assets/importer/gltf/GtsGltfModelImporter` implements `IGtsModelImporter`
and returns `GtsModelImportResult -> GtsModelImportBundle` with a primary model
and any selected skeleton definitions and skeletal animation clips. It handles `.gltf` JSON
and `.glb` version 2 containers. No legacy model DTO, renderer vertex, image decoder,
Vulkan object, or runtime/cooked resource is involved.

```text
.gltf / .glb
    -> shared source utilities / GLB framing
    -> GltfSourceReader (JSON, buffers, views, validated accessors)
    -> GtsGltfModelImporter (source interpretation)
    -> canonical bundle validation -> GtsModelImportResult -> model + skeletons + clips
============================ FORMAT WALL =============================
    ├─ canonical cooking → static cooked-v1
    └─ source model loading → resource → realization → instance → extraction
```

## Decoder ownership and reuse

`gravitas_gltf_importer` links the canonical asset/core JSON target and the small
standard-library-only `gravitas_gltf_source` target. It builds with rendering and
Vulkan disabled. Its public importer header includes only the Strategy interface.
Source reader/utility headers are internal implementation details in this folder.

File byte reads, little-endian word reads, base64/data-URI decoding, component
sizes/counts, and integer normalization were extracted from the legacy importer.
The canonical source reader is now the only production glTF model parser, including
strict GLB framing, direct unsigned index reads, buffer/accessor validation, scene
selection, materials, skins and animation. The former cooker-specific orchestration
and DTOs were removed by the [canonical cooking cutover](canonical-cooking.md).
Unknown GLB chunks retain canonical warnings; malformed inputs fail validation.

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

Skin extraction operates on the validated original node forest before pruning.
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

## Skins, definitions and occurrences

`GltfSkinImporter.h/.cpp` is an importer-private source interpretation stage. It
uses the existing source reader for accessors and composes canonical definitions,
bindings and model associations directly. No legacy DTO or rendering vertex is
used. Skeleton and binding data enter the result only through full canonical
bundle validation.

### Extraction and identity

Every source skin is checked for a nonempty, unique `joints` list with valid node
indices. Starting from each joint, extraction retains its full path to the source
forest root. This is the minimal ancestor closure needed to reproduce its global
transform without baking or changing reference frames. It includes non-deforming
ancestors and helpers between joints, but excludes unrelated descendants/siblings.
A source `skin.skeleton`, if present, must be an ancestor of every listed joint;
it provides grouping evidence, not a stopping point that discards its parents.

The source joint hierarchy must have a common root. Although the canonical
skeleton supports forests, a single glTF skin spanning disconnected source trees
fails explicitly. For a selected skinned mesh, its required hierarchy must be in
the selected scene. A cross-scene reference fails `GLTF_SKIN_SCENE`; the importer
does not pull unrelated inactive scene content into the result. In scene-less
library mode all node trees are available. Unused skins are validated but omitted
with `GLTF_SKIN_EXCLUDED`.

Evaluation nodes follow deterministic depth-first parent-before-child order,
using source child-list order and source-array order among forest roots. Stable
local IDs are structural paths such as `root/1/child/0/child/2`. They contain no
node names or raw source node indices. Repeated equivalent imports and display
renames retain IDs; changing root/child ordering may change them. These are
source-local structural identities, not cross-file rig identity. Garment linking
and deliberate identity remapping remain future tooling responsibilities.

Node names remain optional display data and may duplicate. Source TRS retains
translation, exact quaternion components and scale as canonical TRS. Source
matrices remain exact affine matrices, including helper shear, with no decomposition.
Canonical skeleton validation still checks finite/default transforms and quaternion
validity before compatibility is derived.

### Multiple skins and source rig groups

Among skins used by selected nodes, groups are merged transitively when they:

- share at least one listed joint source node;
- have identical required ancestor closures; or
- explicitly identify the same `skin.skeleton` root.

The group skeleton contains the union of required evaluation nodes. Joint order
and inverse binds do not affect grouping. Sharing only a generic scene ancestor
is insufficient: disjoint subsets without explicit shared-root evidence remain
separate, with `GLTF_SKIN_GROUP_SEPARATE` when closures overlap. Unrelated source
joint trees remain separate definitions even if they look structurally similar.
No name matching, retargeting, compatibility-based interning or placement baking
is performed.

Each source rig group is one source hierarchy occurrence, producing one skeleton
use and one shared immutable definition enumerated once in the bundle. Multiple
body/garment nodes targeting that hierarchy share the use. Each used source skin
produces a separate model binding; repeated nodes selecting that skin reuse it.
A mesh selected by nodes with different skins stays one mesh.

Core glTF has no independently transformed instancing of the same joint-node
tree: reusing the same skin/joints references the same hierarchy occurrence;
changing only the mesh node transform does not create a second pose occurrence.
Copied independently placed joint trees produce distinct uses and definitions.
The canonical domain permits two uses of one definition, but this importer does
not manufacture such sharing between distinct source trees. That deliberate
linking/deduplication remains separate from decoding source occurrence semantics.

### Slots, inverse binds and contextual geometry validation

```text
vertex JOINTS_n component = skin-local slot k (unchanged)
    -> source skin.joints[k] = source node N
    -> source-to-evaluation mapping[N] = canonical skeleton node S
    -> binding.joints[k] = {S, inverseBindMatrix[k]}
```

Inverse binds use non-normalized, tightly packed MAT4 FLOAT accessors with at least
as many matrices as joint slots. Matrices are decoded in slot order; permitted
extra elements are not retained. Consumed matrices must be finite and affine.
Absent inverse binds become identity, never inverse default-pose transforms.
Sparse/implicit-zero accessor policies remain unchanged and explicit.

Bindings obtain exact compatibility from their group's emitted skeleton.
`node.skin` becomes `GtsModelNode::skinBindingIndex`; skin without mesh fails.
Full canonical model validation then checks every bound primitive: paired
JOINTS/WEIGHTS, local-slot bounds even for zero weights, finite nonnegative weights,
positive totals and `abs(total - 1) <= 1e-4` across all influence sets. No normalization,
slot rewriting, influence truncation or four-influence restriction occurs.
Quantized weights that do not meet the total tolerance fail and require an explicit
source conversion decision; the importer does not silently repair them.

### Correspondence and transform ownership

The small canonical addition is `GtsModelSkeletonUse::modelNodeIndices`: an optional
full array mapping evaluation-node index to returned model-node index. glTF fills
it before pruning with source indices and remaps it alongside returned model nodes.
It preserves authored node identity without storing glTF indices below the wall.
The model validator checks count, bounds, uniqueness and mapped parent edges.
Generic model producers may still leave it empty when correspondence is unspecified.
There are no pose buffers or runtime traversal caches.

Positions, normals, node transforms, skeleton defaults and inverse binds use the
same existing basis. UV V flips and tangent-W negation remain unchanged; neither
requires a skeleton/inverse-bind basis conversion. Full ancestor retention means
skeleton root defaults are relative to the model reference frame. No transforms
are baked into vertices or inverse binds.

For eventual skinning, mapped joint globals and binding-specific inverse binds
form the deformation in that reference frame. The skinned mesh node's ordinary
transform must not be multiplied onto that output again. Its authored transform
is retained for hierarchy/correspondence and possible children, while the joint
hierarchy supplies skinning transforms. The mapping records the relationship but
does not choose runtime transform authority. Future realization must use one
pose authority, handle attachments/overlapping helper mappings deliberately, and
apply external model/character world placement once.

These source rules follow the [glTF skin and joint-hierarchy specification](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc#skins).

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

## Skeletal animation import

`GltfAnimationImporter.h/.cpp` directly decodes skeletal animation using the
existing `GltfSourceReader`; no legacy DTO, tween, sampler runtime, or renderer
representation is involved. `GltfSkinImportResult` privately retains each
definition's evaluation-node-to-original-source-node correspondence. It is copied
before model scene pruning remaps `modelNodeIndices`, so source animation indices
cannot accidentally become pruned model indices.

```text
glTF animation channel target.node
    -> skin extraction's original source-node memberships
    -> one resolved skeleton definition
    -> canonical evaluation-node index
    -> GtsAnimationTrack
```

### Source membership versus compatibility versus occurrence

For every channel, candidates are only definitions that actually contain its
source node. Names and structural similarity never add candidates. Intersect
these sets across the whole animation:

- A target with no membership fails `GLTF_ANIMATION_NOT_SKELETAL`.
- An empty intersection fails `GLTF_ANIMATION_MULTIPLE_RIGS`; the animation is
  never split, partially imported, or used to merge rig definitions.
- More than one surviving definition fails `GLTF_ANIMATION_AMBIGUOUS`, even if
  those definitions happen to be compatible. A shared helper alone cannot choose
  a rig; a helper plus a rig-specific joint channel can disambiguate it.
- Exactly one candidate supplies the clip's `GtsSkeletonCompatibility`. The clip
  stores no source rig index, pointer, bundle index or skeleton-use index.

After source resolution, any exactly compatible definition may consume the clip.
Bundle validation requires at least one such listed definition; several matches
are valid and produce no duplicate clips. Multiple model occurrences likewise do
not create clips. One source animation produces exactly one clip in source order.
Source names become display names; an absent name becomes `animation_<index>`.

Current skin extraction gives copied source trees distinct path IDs and keeps
their definitions separate. It does not automatically unify compatible rigs or
create multiple occurrences of one definition. The bundle supports deliberate
sharing; tests exercise that separately from the source extraction policy.

### Keys and source rules

Input accessors must be tightly packed, non-normalized SCALAR FLOAT with finite,
nonnegative, strictly increasing times. Required `min`/`max` must match decoded
first/last times. Output translation/scale is tightly packed VEC3 FLOAT; rotation
is VEC4 FLOAT or normalized signed/unsigned 8/16-bit values using the existing
integer decoder. Bounds and sparse/implicit-zero policies remain those of the
source reader. Missing interpolation means LINEAR; unknown modes fail.

- STEP and LINEAR require one output element per input time.
- CUBICSPLINE requires at least two source keys and three output elements per
  key, ordered incoming derivative / value / outgoing derivative.
- Rotation values explicitly convert source XYZW into `glm::quat(w,x,y,z)`.
  Cubic rotation derivatives remain XYZW `glm::vec4` values, never normalized or
  scaled by segment duration. Vector values preserve source axes; no geometry UV
  or tangent conversion applies to animation.
- Independent times and producer channel order survive. Duration is the maximum
  last key time across tracks. One-key STEP/LINEAR constants are valid, including
  time zero. No uniform timeline, resampling, clamping, or quaternion repair occurs.
- Channels may reuse a sampler while retaining independent canonical tracks.
  Duplicate source node/property pairs fail with channel context. Final clip
  validation enforces canonical quaternion validity and all remaining invariants.

Required evaluation helpers are ordinary skeletal track targets. Exact matrix
nodes cannot receive TRS tracks and are never decomposed. Ordinary model nodes,
missing-node/extension targets, morph weights, unsupported paths, mixed channels,
and empty animations fail explicitly. All source animations are considered; an
animation for a rig excluded by scene selection fails rather than disappearing.
Standalone animation-only glTF without an extracted selected skin hierarchy
remains unsupported. No artificial skeleton or model is created.

Errors retain animation/channel context and sampler/accessor indices where
relevant. A later unsupported or malformed animation discards the entire bundle,
including previously decoded valid clips. Warnings retain a fully valid graph.

## Failure policy and validation limits

- All accessors are checked against both their buffer-view range and the buffer's
  declared length, independently of extra backing-file bytes or GLB padding.
  Count/range calculations avoid overflow. Stride, component alignment, vector
  dimensions, accessor references, and semantic encodings are validated.
- Declared malformed optional streams fail; they do not become absent streams.
- Sparse accessors fail with `GLTF_SPARSE_UNSUPPORTED`; expand them first.
  Accessors without a bufferView also fail with a materialization diagnostic.
  Matrix accessor forms other than MAT4 are currently unsupported, even if unused.
- Actual skins are supported as described above. Unused skins are validated and
  omitted with a diagnostic. Joint/weight geometry without node skin references
  remains allowed under the existing generic geometry policy.
- Morph targets or authored mesh/node morph weights fail. Skeletal TRS animations
  are supported under the rules above; unsupported animation channels fail rather
  than silently becoming static content.
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
scene selection options, sparse support, ordinary-node/morph animation and skinned
realization remain focused future decisions. No animation sampling or runtime skinning is implemented.

`GtsGltfSkinImporterTest` shares generated JSON/binary fixture utilities with the
static importer suite. It covers one-joint and helper-rich rigs, out-of-order source
nodes, exact transforms/IBMs, source slot order, repeated deterministic imports,
scene pruning/correspondence, grouping/union/reordered skins, separate occurrences,
shared meshes, malformed roots/indices/accessors/hierarchy, eight influences,
weight totals, zero-weight slot bounds and unchanged unsupported policies.
`GtsModelSkinTest` additionally checks malformed correspondence values.

`GtsGltfAnimationImporterTest` uses `GltfFixtureBuilder` animation/time/sampler/
channel helpers and direct JSON/binary mutations. It covers external/data-URI/GLB
animation, every TRS/interpolation combination, normalized rotation encodings,
XYZW derivatives, independent timing/duration, helper and source-index mapping,
malformed samplers/keys, ambiguous and unrelated rigs, deterministic results and
all-or-nothing failure. A private resolver test supplies compatible definitions
with separate source memberships to verify that compatibility never introduces
source candidates. Bundle tests cover multiple actual occurrences and compatible
definitions consuming one clip, without changing skin extraction's occurrence policy.
