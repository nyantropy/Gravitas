# Runtime model render extraction

The authoritative high-level model route is:

```text
GtsModelRegistry → GtsModelResource → GtsModelRealizationCache
                                           ↓
                                    GtsRealizedModel
                                           ↓
WORLD: MaterialRuntime → GtsModelMaterialRealization → GtsRealizedModelMaterials
                                           ↓
ENTITY: TransformComponent + ModelInstanceComponent → GtsModelInstance
                                           ↓ read only
RENDERER: extractModelFrame → extractModelRenderState → GtsModelFrameData
                                           ↓
                  static commands/batches + skinned primitive draws
                                           ↓
                   shared GPU geometry + frame palette resources
```

`rendering/core/model` owns the CPU-testable `gravitas_model_frontend` target.
`GtsModelInstanceRuntime` lives here because its world creation facade orchestrates
geometry and renderer material services. The instance, skeletal occurrence state,
immutable hierarchy evaluator, material association interface and opaque material
instance handle remain in `model/runtime`. That target depends only on CPU model
realization and animation; it does not include/link rendering, material realization
or Vulkan. The material frontend implements `GtsRealizedModelMaterials` as a live,
weakly runtime-scoped association. No runtime implementation is moved into assets.

## What extraction owns

`ModelInstanceComponent` contains only `shared_ptr<GtsModelInstance>`. The ECS needs
copyable components for archetype migration; every independent occurrence is created
separately. The generic query pairs that component with `WorldTransformComponent`,
the resolved output of the existing transform system. No renderer-specific state is
added to the instance. Direct mesh components remain supported for procedural,
debug, text, preview and other mesh-level consumers.

`extractModelRenderState` traverses realized occurrences in order and returns either
a complete frame result or contextual diagnostics with no partial draws. Invalid
instances, foreign/expired material scopes, malformed geometry/node associations,
and missing palettes are errors. ECS extraction adds the entity ID and throws under
the existing fail-fast renderer policy. Resource upload/allocation errors remain
renderer/backend errors, not animation or extraction errors.

`GtsModelFrameData` has static and skinned draw lists. Each primitive draw references:

- immutable `GtsRealizedGeometry`, using an aliasing shared pointer into its realized owner;
- the primitive index, without copying the primitive table;
- a current `MaterialFrameState`, including runtime material identity;
- a value object transform and, for static sorting, camera depth;
- an opaque occurrence owner and occurrence index for renderer placement lifetime.

Skinned draws additionally reference an immutable `GtsSkinPalette` snapshot and its
occurrence-local palette slot. Extraction copies matrices once per skin binding per
instance, sharing that snapshot across its primitive/node draws. Static extraction
allocates no palettes. The opaque owner is never cast back to a model by the backend.

Geometry arrays, model hierarchy, skeletons, clips, bindings and canonical materials
are never copied into frame packets. The old persistent presentation types and their
skinned-only ECS query have been removed. A frame is derived draw data, not another
persistent model definition or occurrence.

## Transforms

`gtsModelNodeTransforms` evaluates immutable authored roots/children, including
prepared cooked hierarchies. Static placement is exactly:

```text
entityWorld × modelNodeToRoot
```

Shared static geometry under two nodes produces two placements/draws and one GPU
geometry resource. No transform is baked into vertices.

Skinned placement is exactly **entityWorld after palette deformation**. Palettes
already contain skeleton model transforms multiplied by inverse binds. The authored
skinned mesh-node transform is not reapplied. This profile/bind-space rule has no
Yune-specific branch. Entity placement never changes the CPU palette.

Yune's collider/interaction anchor remains the parent entity. A normal ECS child
has local Y = -1.02 and owns the generic render component; parent movement/facing is
resolved by the existing transform hierarchy. Dungeon floor ownership tracks both
entities for teardown. No new scene graph or instance world transform is introduced.

## Material and frame freshness

Extraction resolves `instance.materialFor(occurrence, primitive)` from the current
material set and checks its weak runtime scope against the supplied runtime. It
uses ordinary `MaterialRuntime::synchronizeGpuState` and frame-state conversion.
Per-slot and model-wide material overrides resolve through the same instance lookup
(per-slot → model-wide → base); extraction has no separate override policy. Setting/clearing
one requires no presentation refresh. Canonical versus cooked origin is invisible here. The headless null-provider path
uses the same runtime's CPU frame description for tests.

After runtime reset, extraction fails until materials are rebound. Calling
`instance.rebindMaterials(service.realize(instance.geometry()).materials)` makes the
next frame use the new live set without recreating geometry, animation or any
presentation. Later animation and ECS transform updates likewise need no refresh.
Earlier extracted frames retain their captured material, transform and palette
values. Cached-world frame mode deliberately reuses the previously extracted frame,
matching the existing paused-world behavior.

## GPU resources and existing pipelines

Static draws become ordinary `RenderCommand` ranges and enter `SceneRenderStage`'s
existing material/pipeline selection and batches. Their commands join the existing
opaque/masked and depth-sorted transparent queues. `MeshManager::realizeGeometry`
caches by retained immutable geometry identity. Its `MeshResource` borrows the shared
prepared data and uploads spans with existing static buffer helpers; its legacy CPU
vectors remain empty for this route. Direct mesh resource behavior is unchanged.

`VulkanSkinnedSceneRenderer` caches one `VulkanSkinnedMeshResource` per retained
realized geometry. Geometry is retained until renderer/cache shutdown, independent
of any instance. Material state is frame-owned rather than cached with geometry.
Placement resources use opaque occurrence identity + occurrence index. Palette
resources use opaque occurrence identity + palette slot, never model-resource
identity. One binding is uploaded once per frame even when many primitives use it.

Both adapters allocate ordinary object SSBO slots. Their frame references retain
placement/palette resources until the corresponding frame fence is waited and its
command buffer reset. Only that frame's object/palette allocation is written.
Other in-flight palette allocations remain untouched. No per-frame mesh upload,
geometry preparation, animation evaluation or device-wide wait is introduced.

Static/skinned vertex layouts, shaders, material push constants, descriptor layouts
and pipelines are unchanged. Static draws never allocate/bind a skin palette.
Skinned opaque draws precede existing static queues. The existing skinned restriction
to opaque depth-writing materials remains explicit; transparent/masked skinned
submission is not implemented here. Models currently bypass bounds culling; skinned
bounds remain bind/static bounds, not conservative animated bounds.

## Guardrails and evidence

A **model** is a composed asset/world occurrence using `GtsModelInstance`. A **mesh**
is a lower-level geometry resource and may still be rendered directly. New models
must not add another persistent presentation mirror.

Extraction is the boundary: above it live nodes, skeleton uses, skin bindings and
logical material associations; below it live geometry, primitive ranges, runtime
materials, object placement, palette bytes and draws. Vulkan never consumes an
instance, model hierarchy, skeleton or animation clip.

`GtsModelRenderExtractionTest` runs with rendering/Vulkan disabled and covers static,
skinned/mixed, shared definitions, hierarchy, primitive/material association, frame
isolation, independent occurrences, cooked geometry, invalid scopes and rebinding.
`YuneAnimationTest` uses the real merchant GLB and game cube OBJ through the same
instance/extraction API. The spatial test scene's potion shelf is also a generic
static model instance. The GPU smoke exercises extracted static/skinned resource
paths and checks geometry cache reuse when a Vulkan device is available. A skipped
device test is not visual verification.
