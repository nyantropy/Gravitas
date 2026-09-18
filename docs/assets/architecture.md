# Asset and model pipeline architecture

Source formats are interpreted once into the canonical model domain. Cooking and
runtime source loading consume that domain; cooked runtime loading consumes already
prepared storage data without reconstructing canonical vertex streams.

```text
OBJ → GtsObjModelImporter ─────┐
glTF/GLB → GtsGltfModelImporter┴→ GtsModelImportBundle / GtsModelAsset
                                            │
                             ┌──────────────┴─────────────┐
                             ▼                            ▼
                    canonical cooking              source loading
                             │                            │
                  static/image processing                 │
                             ▼                            │
                      cooked-v1 assets                    │
                             │                            │
                        cooked loading ───────────────────┘
                                            ▼
                                     GtsModelRegistry
                                            ↓
                                     GtsModelResource
                                            ↓
                                 GtsModelRealizationCache
                                            ↓
                                     GtsRealizedModel
                                            ↓
WORLD: MaterialRuntime → GtsModelMaterialRealization → GtsRealizedModelMaterials
                                            ↓
ENTITY:                              GtsModelInstance
                                            +
                                  authoritative ECS transform
                                            ↓
RENDERER:                       generic model render extraction
                                            ↓
                              static / skinned frame draws
                                            ↓
                           shared GPU geometry, frame palettes
```

## Module responsibilities

| Location | Responsibility |
| --- | --- |
| `assets/model`, `skeleton`, `skin`, `animation` | Immutable canonical definitions and validation. No world, cooking or renderer state. |
| `assets/importer/` | `IGtsModelImporter` strategies for OBJ and glTF/GLB; shared image decoding. Source conventions end here. |
| `assets/processing/` | Transform individual geometry profiles; shared image/scalar-channel processing. |
| `assets/loading/` | Runtime source/cooked policy, capability selection, loading and shared resource retention. |
| `assets/serialization/` | Cooked storage contracts and byte encoding/decoding. |
| `assets/cooking/` | Canonical model decomposition, output names, IDs/references, texture production and serialization. |
| `assets/realization/model/` | Shared prepared geometry and model occurrence associations. |
| `model/runtime/` | Mutable model instances, skeleton occurrences and renderer-neutral material association contracts. |
| `rendering/core/material/` | World material realization, texture resources and existing `MaterialRuntime` integration. |
| `rendering/core/model/` | World instance-creation facade and read-only model frame extraction. |

The former `rendering/core/assets/`, `assets/runtime/`, CMake-only `assets/storage/`
and `assets/cooking/legacy/` locations are removed. No forwarding headers or legacy
model-import compatibility adapters remain. `MaterialAssetLoader` decodes CPU data;
`MaterialAssetRealization` creates world runtime state downstream of assets.

## Loading, preparation and realization

**Loading** selects and decodes the model definition. `GtsModelRegistry` owns shared
immutable resources, referenced by `GtsModelHandle`. A resource retains either a
canonical import bundle or `GtsPreparedModelDefinition`. Requests validate required
capabilities and obey development/strict source policy. Static cooked data cannot
satisfy a request requiring skeletons, skinning or animation.

**Preparation** transforms one mesh into an explicit static or skinned CPU profile.
`GtsStaticVertex` and `GtsSkinnedVertex` remain distinct layouts. Skinned preparation
reduces influences through the existing deterministic policy and retains skin-local
joint slots; it evaluates no pose or palette.

**Model realization** interprets the complete resource. It selects profiles per
occurrence, deduplicates definitions by mesh/profile/binding requirements, and retains
node, primitive, material and skin/skeleton-use associations. Mixed static/skinned
models are valid. Canonical meshes use preparation; cooked meshes are referenced
unchanged, including vertices, ranges, bounds and metadata. The cache keys immutable
model-resource identity. The lower-level flat static adapter remains useful for
direct mesh loading; it is not a competing complete-model API.

See [loading](model-runtime.md), [realization](model-realization.md),
[static preparation](static-geometry.md) and [skinned preparation](skinned-geometry.md).

## Ownership and runtime state

| Owner | State | Lifetime rule |
| --- | --- | --- |
| Engine model registry | `GtsModelResource` | Outstanding model handles retain the immutable definition. |
| Engine realization cache | `GtsRealizedModel` | Shared references retain derived geometry; instances do not copy it. |
| World material service/runtime | `GtsRealizedModelMaterials`, actual material instances | Association sets share within a world and weakly track runtime lifetime; reset invalidates their handles. |
| Entity/component | `GtsModelInstance` | Retains definition, geometry and material-set references; owns independent skeleton occurrence state. |
| Skeleton occurrence | Playback, evaluated pose, binding palettes | One pose evaluation feeds its bindings; no mutable animation state lives on shared assets. |
| ECS transform system | World placement | Neither the model instance nor shared geometry duplicates this authority. |
| Renderer/backend | Frame snapshots and GPU resources | Geometry cache identity is shared realized geometry; dynamic palette resources are occurrence/frame scoped. |

Canonical material slots, external cooked material references and unassigned materials
resolve through one world material API. Canonical images use shared path/memory
decoding; scalar packing uses AO red, roughness green and metallic blue. Color and
emissive textures use sRGB semantics; normal/scalar textures use linear semantics.
Unsupported UV sets and incompatible scalar dimensions fail explicitly. Logical slots
remain distinct; no runtime handles enter shared asset/geometry definitions.

A static instance allocates no skeletal state. A skinned instance starts with a valid
default pose, even without clips. Playback is scoped to a skeleton use, validates
resource-scoped clips, and does not restart an already-active clip. Multiple instances
share definitions while keeping independent playback, poses and CPU palettes.
Entity components hold stable instance references to satisfy ECS copyability.

See [material realization](../rendering/model-material-realization.md) and
[instance ownership and APIs](../model/runtime-instances.md).

## Extraction is the model/rendering boundary

The generic ECS route discovers `ModelInstanceComponent` with the resolved world
transform. Extraction reads current instance material bindings and palettes; it does
not import, prepare geometry, realize materials or evaluate animation. Static draws
compose `entityWorld × modelNodeToRoot`. Skinned draws apply entity placement after
palette deformation; they do not reapply the authored skinned mesh-node transform.

Frames retain immutable geometry owners and capture dynamic transform/material state.
A palette snapshot is shared across draws using that instance binding. Material
rebinding, animation updates and entity movement appear in the next extraction without
refreshing a persistent presentation. The temporary model presentation types and
Yune-specific render submission path have been removed.

Model semantics end at extraction. Backend inputs are geometry, primitive ranges,
runtime materials, object placement and palette bytes. Existing static/skinned vertex
and material ABIs remain unchanged. See [render extraction](../rendering/model-extraction.md).

A **model** is a composed asset/world occurrence using this route. A **mesh** is a
lower-level geometry resource: procedural, dynamic, text, debug and direct mesh APIs
remain valid and do not need dummy model instances.

## Cooking and dependency boundaries

OBJ and glTF feed the same `AssetCooker::cookModelBundle` core. One identity-root
single-mesh model emits `.gmesh`; other model structures emit `.gmodel` with shared
subordinate meshes. Exact local matrices, helper nodes, material slots and primitive
ranges survive. Shared image decoding and scalar packing serve cooking and runtime.
The legacy glTF importer, importer registry, model DTO graph and redundant image
wrapper are removed; `TextureCookInput` remains a cooking-local image input.

Cooked-v1 formats, IDs, references and vertex layouts are unchanged. Cooking rejects
skeletons, skin bindings, animation clips and weighted geometry rather than silently
emitting a static approximation. Animated source models such as Yune remain supported
by canonical runtime loading. See [canonical cooking](canonical-cooking.md) for
capability checks, decomposition, publication and parity differences, and
[cooked storage/CLI](../cooked-asset-pipeline.md) for file contracts.

Canonical domains depend on neither cooking nor runtime. Importers and processing
remain CPU-only. Cooking links canonical importers, processing and serialization,
never model instances or rendering. Loading does not invoke cooking. Model runtime
consumes CPU realization and animation; renderer frontend consumes model runtime.
`assetc`, loading, realization and headless model tests build with rendering/Vulkan
disabled. World creation orchestration stays in the frontend so the instance module
itself remains renderer-independent.

## Regression guardrails and remaining features

`CanonicalModelCookingTest` generates fresh OBJ, glTF and embedded-image GLB packages
and follows them through loading, realization, instances and headless extraction.
It covers hierarchy, sharing, deterministic bytes and material semantics. The
[retained parity fixture](../../tests/assets/fixtures/canonical-cooking/README.md)
protects a complete byte-identical legacy package without retaining the legacy parser.
Model loading/realization, material, instance and extraction suites cover their
individual boundaries. Game `YuneAnimationTest` exercises the real animated source
and a static OBJ through the same runtime route. Device-dependent Vulkan tests may
skip without a compatible GPU; headless success is not visual verification.

Future work includes animated cooked storage, blending, character assembly/clothing,
root motion, material overrides and conservative animated bounds. Current renderer
limits include opaque depth-writing skinned submission and model bounds-culling work.
These features should extend the existing model path; they must not introduce another
source interpretation or persistent renderer presentation mirror.
