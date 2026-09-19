# Model subsystem architecture

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
MODEL EXTRACTION:               generic model render extraction
                                            ↓
                              static / skinned frame draws
                                            ↓
                           shared GPU geometry, frame palettes
```

## Encapsulated model subsystem

`modules/model/` owns source interpretation through renderer-ready frame extraction.
The ordinary application boundary is `model/public/GtsModels.h`:

```cpp
auto requested = requestGtsModel(context, path);
auto created = modelInstances(world, context).create(requested.handle());
// Check diagnostics, then attach ModelInstanceComponent to the entity.
auto clip = findGtsModelClip(requested.handle(), "clip name", skeletonUse);
// The instance supplies play/updateAnimations/material-slot overrides.
```

The existing engine context supplies registry, geometry cache and resource-provider
references internally. `modelInstances(world)` reuses the configured world service.
Engine setup, tools, import/cooking tests and extraction may intentionally use the
phase contracts; game setup does not name resource/realization/preparation internals.
`ModelInstanceComponent` and the instance remain in `runtime/`, with explicit public
exposure through the facade. This is not a monolithic library or an inaccessible API.

| Phase | Folder | Targets |
| --- | --- | --- |
| Application requests, handles, clips, facade | `public/` | implemented by loading/world targets |
| Immutable model / skeleton / skin / clip contracts | `domain/` | `gravitas_model_domain`, `gravitas_skeleton_assets`, `gravitas_skin_assets`, `gravitas_animation_assets` |
| Bundle/result validation and source import | `import/` | `gravitas_model_import`, `gravitas_obj_importer`, `gravitas_gltf_source`, `gravitas_gltf_importer` |
| Primitive/static/skinned preparation, scalar packing | `processing/` | `gravitas_primitive_geometry`, `gravitas_static_geometry`, `gravitas_skinned_geometry`, `gravitas_model_image_processing` |
| Model package persistence | `cooking/`, `serialization/` | `gravitas_model_cooking`, `gravitas_model_serialization` |
| Resource retention and source/cooked selection | `loading/` | `gravitas_model_loading` |
| Immutable prepared geometry/occurrences | `realization/` | `gravitas_model_realization` |
| Mutable occurrence and skeleton coordination | `runtime/` | `gravitas_model_instances` |
| World material realization and instance creation | `world/` | `gravitas_model_materials`, `gravitas_model_world` |
| Read-only traversal, ECS query and frame data | `extraction/` | `gravitas_model_frontend` |

Dependency direction is upstream leaf contracts → import/preparation/loading →
realization → instances/world → extraction. Cooking is a separate branch from
canonical import/preparation to shared serialization. It does not consume loading,
instances, material runtime or rendering. Animation algorithms remain under
`animation/`; they depend on the skeleton/skin/clip leaf targets, never on the model
aggregate or mutable model occurrences.

World integration consumes `gravitas_material_frontend`, the lightweight existing
material/resource-service contracts. It does not link `gravitas_rendering`.
`MaterialRuntime`, `MaterialAssetRealization`, resource providers, generic texture
services and GPU implementations remain external. The shared opaque material handle
is in `assets/material/`; no renderer implementation enters model instances.

`GtsModelFrameData` is the subsystem output: shared geometry views, ranges,
material frame state, object transforms and immutable frame palette snapshots.
It includes `GtsRealizedGeometry`, not `GtsRealizedModel` or resource/hierarchy types.
The generic renderer discovers instances through the model-owned ECS extraction
adapter, then consumes those draws. GPU geometry caches and all Vulkan code stay
in rendering. No model target depends on Vulkan or aggregate rendering.

`assets/` retains [shared asset infrastructure](../assets/architecture.md), including
static geometry ABI used by text and procedural meshes. The flat canonical-to-direct
mesh adapter lives in `assets/loading/mesh/`; it is not complete-model realization.
No old forwarding headers, duplicate source trees or target aliases remain.

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
Instance material lookup resolves per-logical-slot overrides, then a model-wide
fallback override, then unchanged shared base materials. Overrides select live
handles from the same world, retain weak runtime lifetime tokens and never mutate
asset or shared realization data. Clearing restores the next fallback immediately.

See [material realization](../model/model-material-realization.md) and
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
and material ABIs remain unchanged. See [render extraction](../model/model-extraction.md).

A **model** is a composed asset/world occurrence using this route. A **mesh** is a
lower-level geometry resource: procedural, dynamic, text, debug and direct mesh APIs
remain valid and do not need dummy model instances.

## Cooking and dependency boundaries

OBJ and glTF feed the same `GtsModelCooker::cookModelBundle` core. One identity-root
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
root motion, per-slot material variants and conservative animated bounds. Current renderer
limits include opaque depth-writing skinned submission and model bounds-culling work.
These features should extend the existing model path; they must not introduce another
source interpretation or persistent renderer presentation mirror.
