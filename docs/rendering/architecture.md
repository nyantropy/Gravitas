# Engine Rendering Architecture

This document describes the current Gravitas rendering architecture. It is
engine-level only; application scene composition belongs in application docs.

## Ownership Model

Rendering follows a descriptor/runtime split:

- Application or scene code writes descriptor components that describe intent.
- Engine lifecycle systems resolve descriptors into GPU/runtime companions.
- Extraction reads engine-managed runtime state and emits backend-independent
  render commands plus upload side channels.
- The Vulkan backend realizes resources, frame graph stages, command buffers,
  screenshots, and presentation.

Application-facing descriptors should stay clean authoring data. GPU handles,
cached paths, dirty flags, temporary upload state, generated runtime data, and
backend bookkeeping belong in engine-owned runtime components or render
runtime state.

## Module Layout

- `modules/rendering/core/`: generic rendering contracts, resource provider
  interfaces, geometry, materials, lighting frame data, UI extraction, preview
  data, viewport/windowing contracts.
- `modules/rendering/ecssetup/`: ECS descriptors, runtime components, lifecycle
  systems, extraction, camera setup, geometry setup, lighting extraction, and
  particles.
- `modules/rendering/runtime/`: engine render runtime integration, retained UI,
  render pipeline ownership, scene viewport metrics, and render commands.
- `modules/rendering/backend/vulkan/`: Vulkan graphics module, resource
  managers, frame graph, forward renderer, render stages, screenshot manager,
  pipelines, shaders, and backend-specific texture/environment handling.
- `modules/rendering/scene/`: scene feature installers for camera, geometry,
  particles, and full renderer setup.

Vulkan types must stay under `modules/rendering/backend/vulkan/`. Generic
rendering contracts, ECS setup, extraction, and scene feature installers remain
backend-agnostic.

## Frame Data Flow

```text
SceneExecutionProfile
  -> fixed simulation systems when enabled
  -> controller systems when enabled
  -> rendering lifecycle systems
  -> transform/camera/material/particle sync
  -> RenderPipeline build when FrameBuildMode allows
  -> retained UI extraction
  -> Vulkan frame graph execution
  -> optional screenshot capture
  -> present
```

`FrameBuildMode` controls whether the renderer builds world commands, reuses a
cached world frame, renders UI only, or renders nothing. This is how modes such
as fullscreen VN can keep the scene loaded but asleep.

`RenderPassVisibilityComponent` is narrower: it can submit empty scene and/or
particle lists without masking ECS system execution or render extraction.

Performance validation for this flow is owned by the rendering benchmark suite
documented in [benchmarks.md](benchmarks.md). Benchmarks generate deterministic
descriptor-driven ECS worlds, run normal lifecycle and extraction systems,
emit JSON timing/counter data, and check invariants such as zero steady-state
material full scans. The benchmark suite is the required measurement gate for
future renderer optimization work. GPU runtime benchmarks collect backend-owned
Vulkan timestamp-query measurements for frame, scene, particle, and UI stages
when the selected device supports them.

## Descriptor To GPU Split

Renderable geometry should use one geometry descriptor at a time:

- `StaticMeshComponent`
- `QuadMeshComponent`
- `DynamicMeshComponent`
- `WorldTextComponent`

Materials are referenced through shared material identity:

- `MaterialReferenceComponent` points to a `MaterialInstanceHandle`.
- Scene authoring creates material instances through `MaterialRuntime` and
  attaches `MaterialReferenceComponent`; the render lifecycle consumes that
  material identity directly.
- `MaterialGpuState` is stored in `MaterialRuntime`, not as an ECS component.

GPU/runtime companions include:

- `MeshGpuComponent`
- `RenderGpuComponent`
- `CameraGpuComponent`
- `WorldTextRuntimeComponent`
- `TextureAnimationRuntimeComponent`
- `ParticleEmitterRuntimeComponent`

Resource cleanup happens through removal callbacks and explicit lifecycle
systems. Descriptor add/remove does not recursively mutate unrelated ECS state.

## Lifecycle System Order

The shared renderer feature installs controller systems in this order:

1. `TransformSystem`
2. Mesh binding systems: static mesh, quad mesh, dynamic mesh, world text
3. `MaterialBindingSystem`
4. `RenderObjectLifecycleSystem`
5. `RenderableCleanupSystem`
6. `RenderGpuSystem`
7. `TextureAnimationSystem`
8. `CameraLifecycleSystem`
9. camera control systems
10. `CameraGpuSystem`
11. camera view ID allocation and active-camera export systems
12. particle effect hot reload and particle emitter simulation

The order is intentional. Transform resolution runs before render GPU sync;
mesh lifecycle runs before material lifecycle; material lifecycle runs before
render-object readiness; cleanup runs after readiness; camera matrix generation
precedes active-camera export; particles simulate after current camera matrices
exist.

## Render Commands

Extraction emits backend-agnostic `RenderCommand` values:

```text
RenderCommand {
    meshID
    material
    materialGpu
    variantKey
    renderQueue
    objectSSBOSlot
    cameraViewID
    sortKey
}
```

Render commands carry identity, not material parameters. They do not contain
texture IDs, base color/tint, raw blend flags, Vulkan descriptor sets,
pipelines, or ECS entity IDs.

Material frame data is emitted beside command lists. Object uploads carry
per-object model matrix and UV transform. Camera uploads carry view/projection,
camera world position, and `LightingFrameData`.

Final scene object GPU layout:

```text
ObjectUBO {
    mat4 model
    vec4 uvTransform   // xy = scale, zw = offset
}
```

Object data must not store shared material color, tint, texture IDs, or blend
state. Material color belongs to material GPU state; object GPU state contains
placement and per-object presentation data only.

Render queues are explicit:

- `Opaque`
- `AlphaMasked`
- `Transparent`

Transparent commands sort by camera depth first, with variant/material/mesh
ordering as stable secondary keys.

## Geometry Surface Contract

Renderable scene geometry uses one backend-independent vertex contract:

```text
Vertex {
    vec3 pos
    vec3 normal
    vec4 tangent   // xyz = tangent direction, w = bitangent handedness
    vec4 color
    vec2 texCoord
}
```

Geometry defines the surface frame; materials define how light interacts with
that surface. Materials must not synthesize geometry data, and extraction must
not inspect vertex arrays to decide command behavior.

The renderer uses counter-clockwise front faces. Missing normals and tangents
are generated deterministically where possible. Missing colors default to white;
missing UVs default to zero.

Material/mesh requirements:

- `Unlit`: requires position; UV is required only for textured materials.
- `Unlit` with `vertexColorOnly`: requires position and meaningful color
  for useful output.
- `StandardSurface`: requires position and normal.
- `StandardSurface` with normal mapping: requires position, normal, tangent,
  and UV0.

Unsupported lit batches fall back deterministically and emit bounded
diagnostics rather than consuming undefined surface data.

## Dynamic Mesh Runtime Contract

`DynamicMeshComponent` is authored CPU geometry. Its `geometryVersion` is the
authoritative change signal. Application code should mutate the descriptor and
call `markDynamicMeshChanged(...)`, which increments the version and schedules
dynamic-mesh work once through the scene-local geometry lifecycle queue.

`DynamicMeshBindingSystem` drains that queue. An unchanged dynamic mesh performs
only a version comparison and does no geometry processing, vertex/index copying,
GPU upload, resource recreation, material synchronization, or object-slot work.
If a geometry version was already attempted and failed, it is not retried until
the authored version changes.

`MeshGpuComponent` stores the cached dynamic mesh state:

- uploaded geometry version
- last attempted geometry version
- current GPU mesh identity
- used vertex/index byte counts
- allocated vertex/index capacities

Changed geometry is validated, prepared, uploaded, and published once per
version. Generated normals/tangents and bounds are owned by runtime preparation,
not by the authored descriptor. Invalid meshes leave the renderable unready and
record the attempted version so the same bad data is not retried every frame.

The Vulkan procedural mesh backend keeps mesh identity stable where possible.
Capacity-stable updates reuse the existing host-visible vertex and index
buffers and upload only used bytes. If vertex or index data exceeds capacity,
that buffer grows independently with a doubling policy. Buffers do not shrink
automatically in this phase; smaller later updates reuse the existing capacity.
Old procedural resources are released through the existing mesh lifetime path.

Dynamic mesh content changes invalidate only mesh-derived render state. They do
not alter material identity, object slots, or shared material GPU state.

## Render Transform Synchronization

`TransformSystem` is the sole writer of `WorldTransformComponent`. Its
`version` remains the authoritative transform change signal. Rendering observes
world-transform publication through a backend-agnostic callback registered by
the geometry scene feature; the transform module does not depend on rendering.

The geometry lifecycle owns the scene-local render-transform synchronization
queue. Its only responsibility is tracking entities whose resolved world
transform may need to be copied into render runtime state. The queue does not
own transforms, object slots, materials, meshes, render commands, or Vulkan
objects.

Synchronization flow:

```text
TransformSystem publishes WorldTransformComponent version
  -> renderer bridge queues entity once
  -> RenderGpuSystem drains queued entities
  -> component/object-slot readiness is validated
  -> WorldTransformComponent::version is compared with uploadedWorldTransformVersion
  -> changed ready objects copy one model matrix and mark object data dirty
  -> extraction emits one ObjectUploadCommand for the object slot
```

Queue entries are deduplicated and deterministic. Stale entries caused by
component removal, entity destruction, unready objects, or slot release are
skipped once and discarded. Newly ready render objects and object-slot
replacement also queue one sync so current transforms are uploaded without
waiting for a future transform mutation.

Version comparison remains mandatory even when work was queued. Queue presence
only schedules a check; it is not proof that authored state changed. Static
steady state therefore drains no render-transform work, performs no full
renderable scan, copies no matrices, and emits no object uploads.

## Batch-Ready Hot Paths

The transform, render-sync, and snapshot hot paths are structured as
single-threaded batch pipelines. No worker pool, render thread, physics thread,
lock, atomic hot path, or concurrent ECS mutation exists yet. The current
implementation is the single-thread reference path that a future executor must
reuse.

Transform resolution phases:

```text
dirty transform queue
  -> collect TransformWorkItem records
  -> assign hierarchy depths and deterministic batch ranges
  -> calculate world matrices into a result buffer
  -> publish WorldTransformComponent versions
  -> emit the canonical world-transform published callbacks
```

Hierarchy depth is derived for changed work, not rebuilt globally for ordinary
local transform edits. Parents are scheduled before children by depth. Within a
depth level, work items are stored contiguously and partitioned into fixed-size
ranges. Publication is a barrier: component writes and callback emission happen
after matrix calculation, and each entity has at most one destination write.

Render-transform synchronization phases:

```text
render-transform sync queue
  -> validate ECS readiness and compact RenderTransformSyncItem records
  -> process batch ranges into isolated RenderStateUpdate outputs
  -> publish RenderGpuComponent state and RenderDirtyComponent flags
  -> queue snapshot dirty entries
```

The batch records contain copied entity, version, object-slot, and model-matrix
data. They do not contain Vulkan state or long-lived component pointers. The
publish barrier remains the only phase that mutates render ECS state.

Snapshot extraction phases:

```text
snapshot dirty queue
  -> gather SnapshotUpdateItem records for valid dirty renderables
  -> refresh independent dense snapshot slots by batch range
  -> emit object upload commands from refreshed entries
  -> run serial aggregate work such as camera-depth refresh and material-frame population
```

Dense snapshot storage remains stable by object slot. Per-entry refresh is the
parallelizable unit; camera changes, material-frame aggregation, content-version
publication, and invalidation-queue clearing are aggregate merge work.

Future task-graph barriers are therefore explicit:

```text
hierarchy topology ready
  -> parent transform levels complete
  -> world-transform publication complete
  -> render-sync outputs complete
  -> snapshot entry refresh complete
  -> aggregate extraction and backend submission may begin
```

Structural ECS mutation is not allowed inside batch-processing loops. Creation,
component add/remove, object-slot allocation, material/mesh lifecycle, and
resource cleanup stay in lifecycle systems or publish/cleanup barriers.

The minimal future executor requirement is range execution with a single-thread
fallback:

```text
parallelFor(itemCount, batchSize, processRange)
  -> one output buffer per range
  -> deterministic merge in range-index order
  -> profiling context per range
```

A real executor must provide dependency barriers and per-task scratch/output
storage, but it should not change the algorithms above.

Render-thread readiness depends on extraction producing a complete immutable
frame package: render commands, material frame data, object uploads,
camera/light/environment uploads, particles, UI commands, and resource lifetime
commands. The current extraction boundary is close to that shape, but backend
submission still runs on the main thread and uses live renderer/runtime owners.
No render thread is installed.

Physics-thread readiness is separate. The future boundary should be:

```text
immutable physics input snapshot
  -> fixed-step physics execution
  -> deterministic result publication
  -> gameplay/render-prep consumption barrier
```

Current physics remains a main-thread ECS system and does not own a worker or
double-buffered publication path.

Object uploads remain object-owned data:

```text
ObjectUploadCommand {
    objectSSBOSlot
    modelMatrix
    uvTransform
}
```

Material changes, mesh uploads, and ordinary shared-material synchronization do
not schedule render-transform work. Transform changes of transparent renderables
continue to mark snapshot state dirty so transparent ordering can be refreshed;
opaque transform changes do not change command topology merely because the
model matrix changed.

## Material Runtime

`MaterialRuntime` owns:

- `MaterialDefinition`
- `MaterialInstance`
- `MaterialGpuState`
- the deduplicated material synchronization queue
- built-in fallback materials
- default standard-surface material

`Unlit` and `StandardSurface` are explicit shader families.

`StandardSurface` is a metallic-roughness material family. Authoritative
parameters include:

- `baseColorFactor`
- `metallicFactor`
- perceptual `roughnessFactor`, clamped to at least `0.04`
- `normalScale`
- `ambientOcclusionStrength`
- `emissiveFactor`
- `emissiveStrength`

Supported texture roles:

- `BaseColor`: sRGB color texture
- `MetallicRoughness`: linear/data texture, glTF-compatible `R = AO`,
  `G = roughness`, `B = metallic`
- `Normal`: linear/data tangent-space normal map
- `AmbientOcclusion`: linear/data AO override
- `Emissive`: sRGB color texture

Missing maps bind backend fallback textures so descriptor layout stays stable.
Texture roles and material parameters are shared material state; render
commands and object uploads never carry texture-role details.

Base color data flow:

```text
MaterialInstance::baseColor
  -> MaterialRuntime::synchronizeGpuState
  -> MaterialGpuState::parameters.baseColor
  -> MaterialFrameData / MaterialFrameState
  -> SceneRenderStage material push constants
  -> scene shader
```

Scene shaders evaluate final surface color as material base color multiplied
by the base-color texture when present and by vertex color. Lit and unlit scene
shaders do not read material color from object buffers. Per-object UV animation
remains object data.

Material versions are the authoritative change signal. The material queue is
only a scheduler that avoids rediscovering changed materials by scanning all
`MaterialReferenceComponent`s every frame.

Incremental synchronization flow:

```text
MaterialInstance version changes
  -> MaterialRuntime queues MaterialInstanceHandle
  -> MaterialBindingSystem drains queued handles
  -> MaterialRuntime synchronizes GPU state when uploadedVersion is stale
  -> scene material user index returns affected entities when invalidation is needed
```

The material user index is scene-local runtime state in the geometry binding
lifecycle. It tracks only:

```text
MaterialInstanceHandle -> entities using that material
entity id -> current MaterialInstanceHandle
```

It does not own material instances, GPU resources, object slots, meshes,
render commands, or Vulkan state. `MaterialReferenceComponent` add/remove
callbacks and the material reference writer update the index incrementally.
Scene reset clears the index and queue state with the geometry lifecycle and
material runtime.

User invalidation rules:

- parameter-only changes such as base color, metallic, roughness, AO strength,
  normal scale, and emissive factors synchronize material GPU state without
  dirtying object data or rebuilding render commands
- texture replacements synchronize material GPU resources and preserve material,
  mesh, and object identity unless they change variant topology
- topology changes such as shader family, alpha mode, double-sided state,
  depth-write state, vertex-color-only state, or normal-map presence mark only
  indexed users material-dirty and queue command/snapshot refresh
- destroyed materials substitute the default fallback for indexed users and
  invalidate only those users

Steady-state contract: when no material versions change, the material queue is
empty, `MaterialBindingSystem` performs zero material synchronizations, and no
full scene material-reference scan occurs.

## Lighting And IBL

Scene lights are descriptor components on entities. Spatial meaning is resolved
from `WorldTransformComponent` during extraction.

Supported descriptors:

- `DirectionalLightComponent`
- `PointLightComponent`
- `SpotLightComponent`
- `EnvironmentLightComponent`

Extraction publishes bounded `LightingFrameData`: up to 2 directional lights,
32 point lights, 16 spot lights, one selected environment, ambient fallback, and
diagnostics. Selection is deterministic by priority, distance where relevant,
and stable entity ID. Authored values are sanitized during extraction.

`StandardSurface` direct lighting uses one shared Cook-Torrance
metallic-roughness BRDF for directional, point, and spot lights.

Environment lighting is frame-level state. The Vulkan backend resolves the
selected environment through `EnvironmentResourceManager` into irradiance
cubemap, GGX-prefiltered specular cubemap mip chain, and BRDF LUT. Intensity
and rotation are frame-level values and do not mutate material versions or
variant keys.

No sky/background pass is currently included. A future sky pass should consume
selected environment state independently from material draw commands.

## Texture Animation

`TextureAnimationComponent` is the engine path for animated textures on regular
scene geometry. Runtime state lives in `TextureAnimationRuntimeComponent`.

Supported modes:

- `UvScroll`
- `FlipbookAtlas`

The feature updates per-object UV scale/offset through object upload data. It
does not swap material texture paths or rewrite mesh vertices every frame.
Texture animation is visual controller-space behavior and must not own
simulation timing, collision, movement, cooldowns, or simulation-authored state.

## Particles

Particles are an engine rendering feature with asset-facing authoring,
ECS-facing playback descriptors, and a separate Vulkan render stage.

Feature layout:

- `ecssetup/particles/components/`: emitter descriptors, runtime state, and
  authoring types.
- `ecssetup/particles/assets/`: effect asset data, schema migration,
  load/save, module/graph authoring, and compiler.
- `ecssetup/particles/systems/`: emitter simulation and effect hot reload.
- `ecssetup/particles/extraction/`: per-frame particle draw data.
- `backend/vulkan/rendering/particles/`: particle render stage.

`ParticleEffectAsset` owns metadata, preview settings, one or more emitters,
module authoring data, graph data, and compiled runtime programs. The compiler
produces runtime descriptors consumed by ECS playback; editor UI structures do
not drive simulation directly.

Particles extract into `ParticleFrameDataComponent`, not `RenderCommand`.

## Viewport And Internal Resolution

`RenderViewportComponent` tracks scene viewport restrictions in output pixels.
Tooling uses it so scene rendering does not cover tool chrome.

The renderer can separate scene render resolution from output resolution.
Borderless fullscreen keeps the desktop-sized swapchain, renders scene and
particles into an offscreen target at the requested render resolution,
composites that image into the frame output, and renders UI at output
resolution.

## Screenshots

Manual and automated screenshots use the same renderer path:

```text
requestScreenshot
  -> ForwardRenderer
  -> ScreenshotManager
  -> swapchain image copy/readback
  -> async PNG write job
```

`ScreenshotManager` writes files under the requested directory, or under
`screenshots/` by default. Tooling preset automation is documented in
[../tooling/presets.md](../tooling/presets.md).
