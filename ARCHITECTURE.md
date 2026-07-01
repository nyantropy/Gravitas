# Gravitas Engine — Architecture

## 1. Overview

Gravitas is a C++20 modular game engine built around a two-tier ECS
(Entity-Component-System) architecture. The engine separates simulation logic
from optional feature modules such as rendering, physics, tooling, and future
audio, and provides a fixed-timestep simulation loop decoupled from the render
frame rate.

**Core philosophy:**
- ECS-first: all game state lives in components, all behavior lives in systems
- Strict separation between CPU logic and GPU state
- Modular subsystems assembled through explicit engine and scene feature hooks
- Registered scenes are factories; only the active scene owns runtime ECS/GPU state
- Descriptor/GPU component split: application writes descriptors, engine manages GPU resources
- Lifecycle work is explicit: descriptor changes queue intent, lifecycle systems perform structural mutation

**Dependency rules:**
- `engine/core/` must not include headers from `engine/modules/`.
- Base feature modules should depend on `gravitas_core` and only the modules
  they explicitly integrate with.
- Rendering module code may define renderer-facing ECS descriptors and
  extraction contracts, but Vulkan-specific types stay under
  `modules/rendering/backend/vulkan/`.
- Base physics must not depend on rendering. Physics visualization belongs in
  debug/bridge modules such as `debugdraw/physics/`.
- Tools and debug bridges are allowed to depend on multiple modules because
  integration is their purpose.
- Public module headers should stay small and stable. Heavy implementation
  dependencies belong in `.cpp` files or backend-private headers.

---

## 2. Project Structure

```
Gravitas/
├── engine/
│   ├── core/               # Pure C++ ECS framework, input, scene, UI, events
│   │   ├── ecs/            # Entity, component storage, execution profiles, systems
│   │   ├── input/          # Raw keys, semantic action mapping
│   │   ├── scene/          # SceneManager and transition data
│   │   ├── module/         # Engine module lifecycle contracts
│   │   ├── services/       # Optional service interfaces owned by core
│   │   ├── ui/             # Retained-mode UI document model
│   │   └── event/          # Event bus
│   ├── modules/            # Feature modules built on core
│   │   ├── transform/      # TransformComponent (position, rotation, scale)
│   │   ├── hierarchy/      # Parent-child transform hierarchy
│   │   ├── animation/      # Keyframe animation
│   │   ├── tween/          # Reusable value tween/easing helpers
│   │   ├── dialogue/       # Generic dialogue graph runtime and ECS bridge
│   │   ├── debugdraw/      # Debug primitives and module bridge renderers
│   │   ├── physics/        # Physics world, PhysicsSystem, body components
│   │   ├── tools/          # Optional in-engine inspection/editing toolchain
│   │   ├── visualnovel/    # VN stage, command runtime, and retained UI overlay
│   │   └── rendering/      # Renderer-facing contracts, ECS setup, backends
│   │       ├── core/       # Generic rendering contracts, UI extraction, geometry data
│   │       ├── ecssetup/   # Camera, geometry, text, and particle ECS setup
│   │       ├── runtime/    # Engine render runtime integration
│   │       └── backend/    # Optional backend implementations, currently Vulkan
│   ├── GravitasEngine.hpp  # Main engine class
│   ├── GtsGameLoop.h       # Fixed-timestep accumulator
│   └── GtsPlatform.h       # OS/graphics abstraction
├── applications/           # Example scenes
├── external/               # Third-party dependencies (GLM, stb, etc.)
├── resources/              # Engine-level assets (fonts, models, textures)
└── shaders/                # GLSL sources + compiled SPIR-V
```

---

## 3. Core Architecture

### Engine Modules and Services

Engine-level modules implement `IEngineModule` and register borrowed services in
`EngineServiceRegistry`. The registry is intentionally non-owning: module
objects keep their lifetime, while controllers and tools can discover optional
services without making `engine/core/` include module headers.

Scene lifecycle hooks (`beforeSceneLoad`, `afterSceneLoad`,
`beforeSceneUnload`, `afterSceneUnload`) are used for cross-cutting runtime work
such as rendering scene-state reset. Scene feature installers remain the right
place to add ECS components and systems owned by a specific scene.

### ECS Model

**Entity** — a lightweight `uint32_t` ID. No metadata.

**Component Storage** — archetype-based, signature-driven storage:
- Each entity belongs to exactly one archetype identified by its component signature
- Each archetype stores entities plus one tightly packed column per component type (SoA layout)
- Adding/removing a component migrates the entity between archetypes
- Hot-path iteration walks matching archetypes directly instead of probing sparse component stores per entity
- Component signatures use four 64-bit words, giving the engine 256 registered component-type bits

**Queries and Structural Mutation**
- `forEach<C1, C2, ...>(fn)` is the normal hot-path query API and is intended for non-structural iteration
- `forEachSnapshot<C1, C2, ...>(fn)` snapshots entity IDs up front and is used when mutation-safe traversal is required
- Structural ECS changes from systems should go through `world.commands()` and are flushed at controlled points
- Valid structural operations are: `createEntity`, `destroyEntity`, `addComponent`, `removeComponent`, `createSingleton`

**ECSWorld** — the central registry. Key operations:

| Operation | Description |
|-----------|-------------|
| `createEntity()` / `destroyEntity(e)` | Lifecycle management |
| `addComponent<C>(e, c)` / `getComponent<C>(e)` | Per-entity component access |
| `forEach<C1, C2, ...>(fn)` | Iterate all entities with a component set |
| `forEachSnapshot<C1, C2, ...>(fn)` | Mutation-safe snapshot iteration |
| `commands()` / `flushCommands()` | Deferred structural mutation path |
| `createSingleton<C>()` / `getSingleton<C>()` | Enforce single-instance components |
| `registerRemoveCallback<C>(fn)` | Hook fired when a component is removed |
| `addSimulationSystem<S>(group, ...)` | Register a fixed-step system with execution group metadata |
| `addControllerSystem<S>(group, ...)` | Register a per-frame system with execution group metadata |
| `pushExecutionProfile(profile)` / `popExecutionProfile()` | Push/pop runtime execution modes |
| `shouldExecuteGroup(group)` | Query the active execution profile from scene-local orchestration code |

Singletons are used for global state: camera, physics world, time context.

---

### System Types

The engine defines two system base classes with distinct contracts:

#### ECSSimulationSystem
```
update(const EcsSimulationContext& ctx)
```
- Runs at **fixed timestep**
- Context provides: `world`, fixed `dt`, read-only `InputBindingRegistry`
- Deterministic; suitable for physics, animation, game logic
- Systems run in registration order — document and maintain that order
- Examples: `PhysicsSystem`, `TransformAnimationSystem`

#### ECSControllerSystem
```
update(const EcsControllerContext& ctx)
```
- Runs **every frame**
- Context provides: `world`, `resources`, `input`, `time`, `physics`, `ui`, `engineCommands`, window dimensions
- All context pointers are valid for the duration of the `update()` call only — do not cache them
- Can issue typed engine commands (scene change, pause/resume, screenshots, graphics settings) via `ctx.engineCommands`
- Examples: `CameraGpuSystem`, `RenderGpuSystem`, `StaticMeshBindingSystem`

### Execution Profiles And System Groups

Every registered simulation or controller system is assigned one broad
`EcsSystemGroup`. `ECSWorld` stores systems as registered entries containing the
system instance plus its group, and the simulation/controller hotpaths skip
entries whose group is not enabled by the active `SceneExecutionProfile`.

Current system groups are intentionally coarse:

- `Always`
- `Gameplay`
- `Physics`
- `Camera`
- `RenderPrep`
- `Particles`
- `Animation`
- `Audio`
- `Ui`
- `Dialogue`
- `VN`
- `Tools`

Do not introduce feature-specific groups such as enemy AI, shop logic, or a
particular effect unless a broad engine-level category stops being expressive
enough. Most application logic belongs in `Gameplay`; engine renderer binding
and GPU sync systems belong in `RenderPrep`; retained UI sync/input systems
belong in `Ui`; global or scene debug tooling belongs in `Tools`.

`SceneExecutionProfile` combines three independent policy areas:

- `enabledSystems`: a bitmask of awake `EcsSystemGroup` values
- `frameBuildMode`: whether the engine rebuilds world render commands,
  reuses cached world commands, renders UI only, or renders nothing
- `timePolicy`: the intended clock-domain policy for the mode

`ECSWorld` owns a stack of execution profiles. The base profile is always
`SceneExecutionProfile::gameplay()`, and nested runtime modes push additional
profiles. Popping restores the previous mode. Named pop is available so systems
that push a profile can refuse to pop if another nested mode is currently on
top.

Built-in profile factories currently include:

| Profile | Enabled systems | Render build | Intended use |
|---------|-----------------|--------------|--------------|
| `gameplay` | all current broad groups | `FullWorld` | normal scene operation |
| `dialogue_overlay` | `Always`, `Camera`, `RenderPrep`, `Ui`, `Dialogue`, `VN`, `Audio`, `Tools` | `FullWorld` | dialogue over a live scene |
| `fullscreen_dialogue` | `Always`, `Ui`, `Dialogue`, `VN`, `Audio`, `Tools` | `UiOnly` | full VN/dialogue presentation while the loaded world sleeps |
| `pause_menu` | `Always`, `Ui`, `Audio`, `Tools` | `CachedWorldFrame` | menu-like modes over a frozen world snapshot |

`FrameBuildMode::FullWorld` runs the normal render extraction path.
`UiOnly` skips `RenderPipeline::build(world)` and submits no world particles,
but still extracts retained UI. `CachedWorldFrame` reuses the extractor's last
visible command list without rebuilding the ECS render snapshot. `None` skips
world build and UI extraction.

`TimePolicy` is part of the profile contract but is not yet a full multi-clock
implementation. Today, gameplay/physics time effectively stops because those
system groups are masked out. Future work should map these policies onto
separate gameplay, physics, UI, dialogue, and real-time domains without changing
the profile stack API.

---

## 4. Frame Data Flow

```
┌────────────────────────────────────────────────────────┐
│  Active SceneExecutionProfile                          │
│    └─ Masks simulation/controller system groups         │
│    └─ Chooses render build policy                       │
├────────────────────────────────────────────────────────┤
│  Fixed-Timestep Loop (1+ ticks per frame)              │
│  ECSSimulationSystem::update(dt) if group is enabled    │
│    └─ PhysicsSystem, AnimationSystem, [user systems]   │
├────────────────────────────────────────────────────────┤
│  Per-Frame Controller Pass                             │
│  ECSControllerSystem::update(ctx) if group is enabled   │
│    └─ Lifecycle systems (descriptor → GPU companions)  │
│    └─ Cleanup systems (GPU companion teardown)         │
│    └─ RenderGpuSystem (sync world matrices)            │
│    └─ Camera control / CameraGpuSystem / view IDs      │
│    └─ ParticleEmitterSystem (particle frame data)       │
│    └─ [user systems] (input handling, gameplay logic)  │
├────────────────────────────────────────────────────────┤
│  Data Extraction                                       │
│  RenderPipeline build stages if FrameBuildMode allows  │
│    └─ Frustum cull against camera planes               │
│    └─ Emit sorted RenderCommand list + upload commands │
│  UiSystem::extractCommands() if UI rendering is enabled │
│    └─ Flatten retained UI tree → draw calls            │
├────────────────────────────────────────────────────────┤
│  Vulkan Render                                         │
│    └─ Wait current-frame fence                         │
│    └─ Upload current-frame camera UBO                  │
│  Frame Graph execute()                                 │
│    └─ SceneRenderStage (geometry + materials)          │
│    └─ ParticleRenderStage (billboard particles)         │
│    └─ UiRenderStage (overlay)                          │
│    └─ Swapchain present                                │
└────────────────────────────────────────────────────────┘
```

**EcsControllerContext** is the data hub passed to every controller system each frame:

```
EcsControllerContext {
    ECSWorld&                   world
    IResourceProvider*          resources
    InputBindingRegistry*       input             (always live; pause-aware queries)
    const TimeContext*          time
    GtsCommandBuffer*           engineCommands    (scene/engine requests)
    UiSystem*                   ui
    IGtsPhysicsModule*          physics
    float windowAspectRatio, windowPixelWidth/Height
    float sceneViewportPixelX/Y/Width/Height
    float sceneViewportAspectRatio
}
```

**EcsSimulationContext** is passed to every simulation system each fixed tick:

```
EcsSimulationContext {
    ECSWorld&               world
    float                   dt              (fixed timestep)
    const InputBindingRegistry* input       (query-only input registry)
}
```

---

## 5. Rendering Model

`RenderingRuntime` is the engine-facing rendering module. It owns retained UI,
the CPU-side `RenderPipeline`, scene viewport metrics, and rendering extension
commands. `GravitasEngine` delegates per-frame render work to this runtime and
only supplies platform callbacks such as runtime graphics settings.

Generic rendering code owns renderer-facing data contracts such as `Vertex`,
`RenderCommand`, `IResourceProvider`, `IGtsGraphicsModule`, and
`GraphicsBackendRegistry`. Backends install non-owning
`IGraphicsBackendProvider` instances into that registry before `GtsPlatform`
creates the configured graphics module. Backend-specific layout and API details
stay under `modules/rendering/backend/vulkan/`; for example
`VulkanVertexDescription` maps the generic `Vertex` layout to Vulkan vertex
input state, while `VulkanGraphicsBackendProvider` is the backend-local provider
that constructs `VulkanGraphics`.

At build time, `gravitas_rendering` can be configured without a concrete
graphics backend. The generic module still owns the GLFW window abstraction and
renderer-facing ECS/runtime contracts, while selected backend targets such as
`gravitas_vulkan_backend` are added separately. CTest module smoke builds cover
valid option combinations and expected dependency rejections so accidental
module coupling is caught during engine verification. The engine also registers
a headless runtime startup smoke for top-level builds with the Vulkan backend;
it runs when a Vulkan-capable GPU is available and reports a CTest skip when
the test process has no Vulkan hardware.

### Descriptor → GPU Component Split

Application code writes **descriptor components** (high-level intent). Engine systems translate these into **GPU components** (backend handles) through explicit lifecycle passes.

```
StaticMeshComponent("path/to/mesh.obj")   ← written by application
        ↓
StaticMeshBindingSystem (controller)      ← allocates backend mesh, assigns meshID
        ↓
MeshGpuComponent { mesh_id_type meshID }  ← written by engine
        ↓
RenderCommandExtractor reads both         ← emits RenderCommand
```

GPU resource release still happens through component removal callbacks, but descriptor add/remove no longer performs recursive ECS mutation. Binding/cleanup decisions are queued and executed by explicit lifecycle systems.

The render lifecycle is split by concern:
- **Geometry lifecycle** queues and resolves static mesh / quad mesh / dynamic mesh companion state
- **Renderable cleanup** tears down geometry GPU companions regardless of descriptor source
- **Camera lifecycle** owns `CameraGpuComponent` creation/removal independently of geometry lifecycle
- **Render invalidation** queues transform and snapshot dirtiness explicitly so steady-state extraction does not scan every renderable

Scene-level render pass visibility is controlled at two layers. The stronger
layer is the active `SceneExecutionProfile`: its `FrameBuildMode` determines
whether `RenderingRuntime` runs `RenderPipeline::build(world)`, reuses cached world
commands, extracts only retained UI, or skips UI extraction as well. This is the
path for CPU-saving runtime modes such as fullscreen dialogue because it can
also mask `RenderPrep`, `Camera`, `Particles`, `Physics`, and `Gameplay`
systems before render extraction.

The optional `RenderPassVisibilityComponent` singleton is a narrower graphics
handoff switch. `GravitasEngine` reads it after the profile render policy has
chosen a render list and can submit an empty scene render list and/or empty
particle frame data while leaving ECS system execution and render extraction
untouched. Use it for pass-level presentation control; use execution profiles
when the world should stay loaded but asleep.

For cameras, application code should only author `CameraDescriptionComponent`
plus transform/control descriptors. The engine-owned camera lifecycle
creates/removes `CameraGpuComponent`, `CameraGpuSystem` computes matrices, and
`CameraBindingSystem` allocates the stable camera view ID. The renderer owns the
actual camera UBO upload: extraction packages the active camera matrices into
`CameraUploadCommand`, and `ForwardRenderer` writes the current frame's camera
UBO only after waiting for that frame's fence. This keeps camera data in sync
with uncapped rendering without racing frames in flight. Engine consumers that
need final matrices outside the renderer should read the read-only
`ActiveCameraViewStateComponent` singleton instead of touching GPU companion
components directly.

### Scene Lifetime

`SceneManager` stores a registered scene catalog plus factory functions. It
does not keep inactive scene instances alive. `GravitasEngine` owns one active
`GtsScene` instance at a time, and scene changes wait for graphics idle before
calling `GtsScene::unload(...)`, clearing the active scene world, destroying the
old scene instance, constructing the next scene from its factory, and then
calling `onLoad(...)`.

This means scene ECS worlds, physics worlds, retained scene UI, camera view IDs,
render object slots, and owned procedural mesh resources are scene-runtime
state. They must be rebuilt by the newly loaded scene. Long-lived application
state that should survive a scene change belongs outside the scene instance and
is passed into scene factories by the application.

`GtsScene::unload(...)` calls the optional scene `onUnload(...)` hook before
clearing the world. Scenes use that hook to write back persistent application
state, while renderer remove callbacks release GPU companions as ECS components
are destroyed. Engine shutdown also unloads the active scene before graphics
shutdown so scene-owned resources are not destroyed after the backend is gone.
After a scene unload, `RenderPipeline::resetSceneState()` clears extraction,
command, and visibility caches so the next scene starts from a clean render
snapshot even if entity IDs, SSBO slots, or allocator addresses are reused.

### Descriptor Components (application-facing)

Transform descriptors live in the transform module. Render and particle
descriptors live with the renderer ECS setup, next to their runtime companion
components and systems.

| Component | Purpose |
|-----------|---------|
| `TransformComponent` | Position, rotation, scale |
| `StaticMeshComponent` | Asset path to mesh |
| `QuadMeshComponent` | Width/height for shared generated quad meshes |
| `DynamicMeshComponent` | Runtime-authored vertices/indices plus `geometryVersion` for uploaded mesh updates |
| `MaterialComponent` | Texture path, tint color/opacity, culling, and optional vertex-color-only rendering |
| `TextureAnimationComponent` | Optional per-object scene-material UV scrolling or flipbook atlas animation |
| `WorldTextComponent` | World-space text, font asset path, scale, and tint |
| `BoundsComponent` | Local AABB used for frustum culling |
| `CameraDescriptionComponent` | FOV, near/far clip planes |
| `PhysicsBodyComponent` | Dynamic vs static body flag |
| `ParticleEmitterComponent` | Game-facing particle emitter descriptor paired with `TransformComponent` |

Renderable scene geometry should use one geometry descriptor at a time:
`StaticMeshComponent`, `QuadMeshComponent`, `DynamicMeshComponent`, or
`WorldTextComponent`.

Descriptor direction: descriptors are supposed to stay as clean authoring data.
They describe what the scene wants, not how the engine currently fulfills it.
GPU handles, cached paths, dirty flags, temporary upload state, generated
runtime data, and backend bookkeeping belong in engine-owned companion
components or feature runtime components. When a descriptor starts needing that
kind of state, split the state out instead of growing the descriptor into a
mixed gameplay/rendering object.

### GPU Components (engine-managed)

| Component | Managed By |
|-----------|-----------|
| `MeshGpuComponent` | `StaticMeshBindingSystem`, `QuadMeshBindingSystem`, `DynamicMeshBindingSystem`, and `WorldTextBindingSystem` |
| `MaterialGpuComponent` | geometry lifecycle systems |
| `RenderGpuComponent` | geometry lifecycle systems + `RenderableCleanupSystem` + `RenderGpuSystem` |
| `CameraGpuComponent` | `CameraLifecycleSystem` + `CameraGpuSystem` + `CameraBindingSystem` for view IDs; renderer for UBO upload |

Particle emitters use a runtime companion component (`ParticleEmitterRuntimeComponent`)
for simulation state, but particles are extracted into `ParticleFrameDataComponent`
instead of flowing through `RenderCommand`.

World text uses the same descriptor/runtime split:
applications author `WorldTextComponent` with a font asset path, and the
renderer feature owns `WorldTextRuntimeComponent`. The runtime companion caches
the last authored text/font/scale/tint values and the resolved `font_id_type`.
`WorldTextBindingSystem` resolves the font, uploads glyph quads as runtime mesh
data, writes the font atlas texture and tint into `MaterialGpuComponent`, and
cleans up the renderable when the text has no visible glyphs.

Scene texture animation uses the same descriptor/runtime split:
applications author `TextureAnimationComponent`, the renderer feature creates
and owns `TextureAnimationRuntimeComponent`, and `TextureAnimationSystem`
advances visual-only elapsed time every controller frame. The runtime companion
caches the last descriptor values so authored changes restart playback. The
resulting UV scale/offset is written into `RenderGpuComponent` and uploaded
through the normal object SSBO path.

### Engine-Exported Frame State

| Component | Purpose |
|-----------|---------|
| `ActiveCameraViewStateComponent` | Read-only current camera view/projection snapshot for non-renderer consumers such as UI projection |

### Controller Order Contract

The shared renderer feature installs controller systems in this order:

1. Geometry and text binding systems
2. `RenderableCleanupSystem`
3. `RenderGpuSystem`
4. `TextureAnimationSystem`
5. `CameraLifecycleSystem`
6. camera control systems
7. `CameraGpuSystem`
8. camera view ID allocation / active-camera export systems
9. particle effect hot reload and particle emitter simulation

This order is intentional:
- lifecycle runs before per-frame sync so newly created GPU companions participate immediately
- texture animation runs after `RenderGpuSystem` so newly ready renderables have
  a valid object slot before animated UV data is queued for upload
- camera control runs before `CameraGpuSystem` so same-frame transform/description changes feed matrix generation
- camera view ID allocation/export runs after matrix generation so rendering and UI see the current frame's active camera state
- particle simulation runs after camera matrix generation so extracted billboards can use the current view

### RenderCommand

The extractor output:
```
RenderCommand {
    mesh_id_type       meshID
    texture_id_type    textureID
    ssbo_id_type       objectSSBOSlot    (per-object transform)
    view_id_type       cameraViewID      (camera UBO)
    bool               doubleSided
    bool               vertexColorOnly
}
```
Opaque commands are sorted by (double-sided, vertex-color-only, meshID, textureID) to minimize state changes. Transparent commands are appended after opaque commands. Cached across frames; only rebuilt when renderable content, visibility, camera version, or active camera view changes.

Render extraction also emits upload command side channels:

```
ObjectUploadCommand {
    ssbo_id_type objectSSBOSlot
    glm::mat4    modelMatrix
    glm::vec4    uvTransform    // xy = scale, zw = offset
    glm::vec4    tint           // rgba multiplier; a controls material opacity
}

CameraUploadCommand {
    view_id_type cameraViewID
    glm::mat4    viewMatrix
    glm::mat4    projMatrix
}
```

Object uploads are generated only for renderables marked dirty by the render
invalidation queue. They carry the full per-object scene data, currently the
model matrix, scene-material UV transform, and material tint. Camera uploads are generated
every extracted frame for the active camera and consumed by the renderer after
the current-frame fence wait.

`vertexColorOnly` is a material/render-command flag for tool and debug meshes
that should render from authored vertex colors without sampling a color texture.
The regular material texture path remains the default for game geometry.

### Scene Texture Animation

`TextureAnimationComponent` is the engine path for animated textures on regular
scene geometry. `enabled` is the off switch; `mode` only selects how an active
animation behaves. It supports two first-pass modes:

- `UvScroll`: continuous UV offset over controller-frame time, suitable for
  water, lava, fog sheets, shimmer, or other looping surface motion.
- `FlipbookAtlas`: row-major atlas frame selection with optional looping,
  suitable for discrete animated tile frames.

Texture animation can run on unscaled frame time or scaled frame time. Unscaled
is the default so existing visual effects keep moving while gameplay timing is
paused. Scaled mode uses frame time multiplied by `TimeContext::timeScale` and
stops while the fixed-step game loop is paused.

This feature deliberately does not swap `MaterialComponent::texturePath` and
does not rewrite mesh vertices every frame. Both approaches would dirty
resource bindings or mesh data instead of updating per-object visual state. The
shader applies `uv = inTexCoord * uvScale + uvOffset` from the object SSBO
before sampling the material texture. Static materials use identity
`uvTransform = {1, 1, 0, 0}` and render unchanged.

For flipbook atlases, `scrollSpeed` and `uvOffset` are applied inside the
selected atlas frame instead of drifting the final atlas UV across neighboring
frames.

Texture animation is a visual controller-space feature. It must not own
gameplay timing, collision, movement, cooldowns, or simulation-authored state.

### Particle Rendering

Particles are an engine rendering feature with asset-facing authoring,
ECS-facing playback descriptors, and a separate Vulkan render stage.

Feature layout:

- `modules/rendering/ecssetup/particles/components/ParticleEmitterComponent.h`: game-facing particle emitter descriptor
- `modules/rendering/ecssetup/particles/components/ParticleTypes.h`: authoring types for curves, shapes, bursts,
  flipbooks, and force module data
- `modules/rendering/ecssetup/particles/components/`: runtime particle state
- `modules/rendering/ecssetup/particles/assets/`: particle effect asset data,
  schema migration, load/save, legacy preset compatibility, and hot-reload
  registry state
- `modules/rendering/ecssetup/particles/systems/`: emitter simulation and
  effect hot reload
- `modules/rendering/ecssetup/particles/extraction/`: per-frame particle draw data
- `modules/rendering/backend/vulkan/rendering/particles/`: billboard and mesh
  particle render stage

Application code currently authors `ParticleEmitterComponent` plus
`TransformComponent`, but `effectPath` is now treated as a particle effect asset
reference rather than only a flat emitter preset. `ParticleEffectAsset` owns
metadata, preview settings, and one or more named emitters. Each emitter now
owns authoring modules in addition to its compatibility runtime descriptor.
Modules have stable instance IDs, module type IDs, display metadata,
schema/version data, and typed parameters. Parameter values include scalars,
integer values, booleans, enums, asset-path strings, float curves, color
gradients, and burst timelines. String parameters may declare a picker role such
as texture or mesh so the editor can discover compatible files without
hardcoding renderer module behavior. Each emitter also owns graph authoring data:
nodes linked to module stable IDs, dependency links, frames, comments, and node
positions. Module definitions declare graph-compatible metadata: input ports,
output ports, required or optional dependencies, execution category, and
execution stage. The current execution stages are `Spawn`, `Initialize`,
`Update`, and `Render`. The stack editor still shows a simple module list, but
the asset now stores enough graph data for node creation, connections,
grouping/comment metadata, validation diagnostics, and later visual graph
editing.

Graph/module authoring compiles into `ParticleCompiledParticleProgram`, a
derived runtime representation stored on each loaded emitter and not serialized
as source data. The current compiler validates graph topology, orders modules by
execution stage, records compiled module metadata, tracks simple optimization
counters such as dead-node elimination, curve baking, static parameter
evaluation, and module fusion, then emits a CPU-descriptor backend by baking the
program into `ParticleEmitterComponent`. Hot reload, legacy `loadParticleEffect`,
and editor live preview prefer this compiled runtime descriptor and fall back to
the compatibility descriptor only if compilation is invalid. This preserves
existing game behavior while keeping ECS playback separate from editor graph
structures and leaving a clean insertion point for a future CPU/GPU particle
program backend.

The engine creates `ParticleEmitterRuntimeComponent`, simulates particles every
controller frame, generates per-emitter particle bounds, applies render-side
frustum/distance culling, computes distance LOD and importance scores, sorts
alpha particles by camera depth, batches by primitive, mesh, texture, and blend
mode, and renders particles in `ParticleRenderStage`. `ParticleBudgetComponent`
is an optional singleton budget surface for global simulated/rendered/spawned
particle limits plus per-frame runtime counters. Hot-reload bookkeeping,
including the last applied effect asset version, lives in
`ParticleEmitterRuntimeComponent`. Tool-preview playback overrides, such as
pause and time scale, also live in runtime state so they do not become authored
particle asset data.

Particle controls currently include:

- local-space or world-space simulation
- sphere, box, disc, cylinder, and ring emit shapes
- billboard and instanced mesh particle primitives
- soft-circle, square, diamond, petal, and streak billboard masks
- alpha and additive blend modes
- base tint, color over lifetime, alpha over lifetime, and size over lifetime
- emission rate, max particles, lifetime range, duration, delay, and intensity
- initial velocity, spread, radial/tangent velocity, drag, spin, random size,
  billboard aspect ratio, and mesh angular velocity
- hue/value variation
- bursts, flipbooks, softness, wind/acceleration/vortex/radial/noise forces
- effect scale, importance, per-emitter spawn caps, global budget weighting,
  distance LOD, render frustum/distance culling, and simulation-while-culled
  policy
- generated particle bounds and per-frame counters for active/rendered/clipped
  particles, culled emitters, collisions, deaths, and event spawns
- CPU-side billboard velocity stretching using existing width/height/rotation
  instance data
- ground-plane particle collision with bounce, damping, kill-on-collision, and
  spawn-on-death/spawn-on-collision event bursts
- material path, mesh softness, and lighting-influence descriptor hooks for
  future renderer/backend integration
- texture path, optional mesh path, effect asset path, and optional selected
  emitter id

Initial particle authoring module categories are:

- `Spawn`
- `Lifetime`
- `Shape`
- `Velocity`
- `Forces`
- `Color`
- `Size`
- `Rotation`
- `Renderer`
- `Bursts`

`ParticleEffectHotReloadSystem` loads effect files into a singleton registry and
copies selected compiled asset-emitter runtime descriptors onto ECS emitters
whose `effectPath` is set and `reloadFromEffect` is true. Existing flat JSON
emitter presets are migrated in memory into one-emitter `ParticleEffectAsset`
values, and missing module or graph authoring data is generated from the legacy
descriptor. Saving through the asset IO path writes the module/graph
effect-asset format plus compatibility descriptor fields, including runtime
policy, material hook, and collision/event fields. Older scalar module data for
alpha peaks, size endpoints, and the first burst is migrated into richer curve,
gradient, and timeline module values.

The legacy particle inspector still edits a selected live ECS emitter descriptor
for low-level debugging. The dedicated `ParticleEffectEditorPanel` is the
asset-authored workflow: it discovers known effect paths from the hot-reload
registry, live emitters, and `resources/particles`, loads `ParticleEffectAsset`,
edits effect/emitter data in memory, saves or duplicates effect files, and
applies the selected asset emitter onto matching live ECS emitters for immediate
preview only. The panel owns inline tool text fields for effect and emitter
names, captures keyboard input while a field is active so the tool camera does
not move during typing, stores preview background plus camera orbit/reset
settings on the asset, and presents emitter modules as a dynamic stack.
Parameter rows are built from the module registry rather than hardcoded per
module type. Numeric min/max pairs are collapsed into range controls; curve,
gradient, and burst timeline parameters expose compact key editing; texture and
mesh string parameters cycle through discovered resources; and editor-local
undo/redo, emitter copy/paste, module copy/paste, and multi-emitter batch
selection are handled inside the panel. Module rows display graph execution
stage tags and the panel shows graph validation status for the selected
emitter's graph. The panel includes compact graph actions for cycling/searching
nodes, adding registered nodes, toggling dependency links, and creating
frames/comments, but it remains a retained-UI module/stack bridge rather than
the final visual graph editor. New particle authoring features should extend
`ParticleEffectAsset`, the graph data, the module registry, and the compiler;
runtime simulation should continue consuming compiled runtime data rather than
editor UI structures.

### Frustum Culling

`FrustumCuller` extracts 6 planes from the view-projection matrix
(Gribb-Hartmann) and tests entity AABBs from `BoundsComponent`. Entities
without bounds are always rendered. Culling can be frozen (debug feature) to
inspect which objects are culled while the camera moves.

Visibility results are cached by content version and frustum. The snapshot also
tracks a camera version so command extraction does not reuse cached commands
across camera changes, even when the visible set happens to stay the same.

### Transform Sync

`RenderGpuSystem` owns world-matrix propagation into `RenderGpuComponent::modelMatrix`.

- Root renderables take a flat fast path keyed by `objectSSBOSlot`
- Hierarchical entities use a cached recursive path with cycle protection
- `gts::transform::markDirty(...)` queues render transform dirtiness through `RenderInvalidationLifecycle`
- `RenderGpuSystem` consumes transform-dirty entities, updates world matrices, and queues snapshot dirtiness
- `RenderDirtyComponent` is the bridge to extraction: `RenderExtractionSnapshotBuilder` consumes queued snapshot-dirty entities and clears the per-renderable dirty flags

This split gives strong single-threaded performance for flat scenes such as large cube benchmarks without removing hierarchy support.

### Frame Graph

The Vulkan backend uses a declarative frame graph:
- Stages declare their image/buffer read-write access patterns at `compile()` time
- `execute()` records command buffers with automatic resource layout transitions
- Resource types: external (swapchain), transient (render targets)

`ForwardRenderer` owns frame-resource lifetime, swapchain/headless output
recreation, frame pacing, extraction upload, and command-buffer submission.
`ForwardFrameGraphBuilder` owns frame-output/depth resource import and stage
assembly for the forward path, so stage constructor wiring stays separate from
the renderer's per-frame orchestration.

### Vulkan Backend Context Boundary

`modules/rendering/backend/vulkan/` is the only layer allowed to include Vulkan
types. Generic rendering contracts, ECS setup, extraction, and scene feature
installers must stay backend-agnostic.

`VulkanGraphics` owns `VulkanContext` and passes a `VulkanBackendContext`
reference through the backend objects that need device, queue, swapchain, or
frame-output access. Descriptor allocation is owned by `RenderResourceManager`
through `DescriptorSetManager` and is passed explicitly to stages and pipelines
that need descriptor layouts or descriptor-set allocation. Backend code should
keep these dependencies explicit rather than introducing process-wide access
helpers.

### Debug Draw

`modules/debugdraw/` is a standalone feature for transient visual diagnostics.
Callers enqueue world-space primitives through helpers in
`DebugDrawPrimitives.h`; `DebugDrawSystem` batches those lines by color into
dynamic meshes. Debug draw uses `MaterialComponent::vertexColorOnly` so axes,
bounds, rays, and frusta are not dependent on sampled debug textures.

Debug draw is intentionally generic. Tool-specific policy, such as drawing the
selected entity bounds or pick ray, lives under `modules/tools/debugdraw/`.

---

## 6. Event Buses

The engine has two event buses with distinct domains. Use the right one for the right layer.

### GtsPlatformEventBus — platform/infrastructure events

Owned by `VulkanGraphics`, passed by reference to `ForwardRenderer`, `WindowManager`, and `OutputWindow`. Dispatched once per frame in `GtsPlatform::beginFrame()`.

- **Emit** from OS/GPU callbacks: `eventBus.emit(MyEvent{...})`
- **Dispatch** on the main thread: `eventBus.dispatch()` — delivers all queued events
- **Subscribe** via `SubscriptionToken`: `token = eventBus.subscribe<MyEvent>([](const MyEvent& e) { ... })`

Dispatch is deferred because GLFW callbacks fire during `glfwPollEvents()`. Queueing ensures delivery always happens on the game thread at a predictable point.

Current platform events: `GtsWindowResizeEvent`, `GtsKeyEvent`, `GtsFrameEndedEvent`.

### ECSWorld — ECS gameplay events

Integrated into `ECSWorld`, accessed via `ctx.world.publish/subscribe` from any system.

- **Publish** (immediate): `ctx.world.publish(MyEvent{...})` — all handlers called before publish() returns
- **Subscribe** via `SubscriptionToken`: `token = ctx.world.subscribe<MyEvent>([](const MyEvent& e) { ... })`

Dispatch is immediate because ECS updates are single-threaded and sequential. Events published during a system's update are received by later systems in the same pass.

**Rule of thumb:** if the event originates from hardware, a driver, or a GPU callback → `GtsPlatformEventBus`. If it originates from game logic during the ECS update loop → `ECSWorld`.

---

## 7. Input System

Raw input is abstracted behind `IInputSource` and resolved to semantic actions via `InputBindingRegistry`:

```
Hardware → GtsPlatform → IInputSource → InputBindingRegistry
                                                  ↓
                        Controller/simulation systems poll via Ecs*Context::input
```

Engine actions use string identifiers such as `engine.pause` and `engine.close`. Applications extend this with their own namespaced action strings.

Input contexts are stack-like named scopes. Engine tools use their own
`engine.tools` context so UI clicking, world-picking, gizmo dragging, and tool
camera input are mediated through the same input abstraction as keyboard
actions instead of reaching into GLFW. Applications may add their own contexts
for game-owned debug controls.

Mouse position, buttons, scroll deltas, and cursor capture flow through
`IInputSource` and `InputBindingRegistry`; engine systems should consume those
values from `EcsControllerContext::input`.

### Simulation And Controller Input

Pressed/released input has two timing domains:

- `isPressed()` / `isReleased()` expose frame-local edges for controller
  systems, UI, tools, and engine commands that run once per rendered frame.
- `isSimulationPressed()` / `isSimulationReleased()` expose action edges latched
  until the next fixed simulation tick consumes them. Simulation systems should
  use these methods for edge-triggered gameplay commands so input cannot be
  missed when render FPS is much higher or lower than the simulation tick rate.

`InputManager` records raw key/button edges generated while polling OS events,
including press-and-release pairs that happen inside one rendered frame.
`InputBindingRegistry` resolves those raw edges through active input contexts
into semantic action edges and decrements simulation edge queues after each
fixed tick via `finishSimulationTick()`. Context stack changes, binding
replacement, and pause transitions clear queued simulation edges so stale
gameplay commands cannot leak across scene/menu boundaries.

The rule is strict: fixed-step gameplay systems must never depend on
frame-local input edges. They may read held state for continuous commands, but
edge-triggered simulation commands must use the simulation-latched input API.
Controller systems may use frame-local edges because they run once for every
input update and are responsible for presentation, UI, tools, and engine-level
commands rather than deterministic gameplay progression.

### Fixed Simulation And Presentation

`GtsGameLoop` uses a fixed-timestep accumulator. Simulation systems run zero or
more fixed ticks per rendered frame, then controller systems run once for the
current rendered frame. `GravitasEngine` copies `GtsGameLoop::alpha()` into
`TimeContext::simulationAlpha` after fixed ticks and before controller systems,
so frame-facing presentation code can interpolate between fixed-step snapshots.

Gameplay correctness therefore comes from fixed simulation plus
simulation-latched input. Visual smoothness at render rates above or below the
simulation tick rate requires presentation systems to interpolate from previous
and current fixed-step snapshots using the accumulator alpha. Presentation
systems may use `ctx.time->unscaledDeltaTime` for visual-only effects, UI, and
tools, but authoritative gameplay movement, timers, cooldowns, combat state,
physics, and scene progression must stay in fixed simulation space.

### Runtime Profiling

`GravitasEngine` prints a rolling frame profile through `ProfileAccumulator`.
When the profile interval elapses, it prints simulation-system and
controller-system breakdowns separately. `ECSWorld` times every registered
`ECSSimulationSystem` during fixed ticks and every registered
`ECSControllerSystem` during rendered-frame controller updates, then resets both
profile buckets after printing.

---

## 8. Retained UI and Engine Tooling

The retained UI model is an engine core service. `UiSurface` is the runtime
boundary for a retained UI universe: it owns a `UiDocument`, explicit layers,
focus state, modal state, mount lifetime, input dispatch, active theme, and
coordinate conversion, animation, and data binding. `UiSystem` is the
render-facing facade and compatibility surface router; existing APIs still
target the default screen surface, while surface-aware APIs can create and
address additional surfaces. `UiWidget` provides reusable composition-owned
controls on top of retained nodes, layout, themes, propagated events,
surface-local navigation, and binding convenience APIs.

Each surface owns a `UiDocument` with a hidden document root plus one or more
ordered layer roots. Existing callers that create nodes without specifying a
surface or parent still attach to the default layer root returned by
`UiSystem::getRoot()`, so current retained UI builders continue to work. New
engine or game systems may create named layers through the default-surface APIs
or through surface-aware overloads such as `UiSystem::createLayer(surface, ...)`,
add nodes under `getLayerRoot(surface, layerId)`, and change layer ordering or
input participation without relying on scene construction order. Render
traversal visits lower-order layers first and hit-testing walks the same layer
roots in reverse order, so layer order remains the explicit stacking primitive
inside a surface.

UI input dispatch is now centralized once per engine frame. After platform input
has been sampled and before simulation/controller systems run,
`RenderingRuntime::dispatchUiInput(...)` builds the frame's `UiInputFrame` from
`InputBindingRegistry` and calls `UiSystem::dispatchInput(frame, frameId)`.
`UiSystem` selects the top ordered input-participating surface containing the
pointer, or a surface that already owns pointer/cancel interaction, converts
screen-normalized coordinates into that surface's local coordinates, and then
delegates to that surface's `UiInputDispatcher`. The dispatcher performs the
retained hit test, creates the frame's hovered/pressed/released/clicked/active/
captured result, resolves owning layers, consults that surface's modal policy,
stores a `UiDispatchResult`, and creates frame `UiEvent` values. Persistent
interaction ownership lives in the surface's `UiFocusManager`, not in the
dispatcher. The focus manager owns pointer hover per pointer id, pointer
capture, active pointer owner, keyboard focus, text-input focus, navigation
focus, focus scopes, and focus restoration. Modal ownership lives in the
surface's `UiModalManager`, which owns the retained modal stack, layer blocking
policy, cancel/back routing, and focus-scope restoration for nested modals.
Retained node `hovered`, `focused`, and `pressed` flags are presentation
reflection of focus-manager state, not authoritative owners.

Retained UI event propagation is engine-owned and surface-local. `UiSystem`
resolves each event's surface, target path, layer, mount, and composition, then
delivers capture, target, and bubble phases to `UiComposition::onEvent(...)`.
`event.consume()` stops propagation; `event.preventDefault()` marks default
behavior as blocked without stopping propagation. Existing retained UI systems
may still read `ctx.ui->dispatchResult()` during migration, but new
composition-owned UI should prefer `onEvent(...)`. `UiSystem::updateInteraction(...)`
remains only as a compatibility API over the dispatcher.

`UiMount` is the retained subtree ownership primitive. `UiSystem::createMount`
creates a container root attached to a layer, node, or parent mount, while
`destroyMount` removes that subtree, destroys child mounts, removes text
bindings below the root, clears retained focus/capture/text/navigation state,
and dismisses modal ownership associated with the subtree. Existing handle
builders still work, but new runtime-owned UI should be attached through mounts.
If compatibility code removes an exact mount root through `removeNode`, the
mount is destroyed instead of leaving stale ownership state.

`UiComposition` is the retained UI authoring primitive. `UiSystem` can
`mountComposition(...)` into a new mount or `attachComposition(...)` to an
existing mount on the default surface, and surface-aware overloads can mount the
same composition type into another surface. The composition lifecycle is
`build`, `update`, `destroy`, and `rebuild`, plus event delivery through a
`UiCompositionContext`. The context exposes the owning `UiSystem`, surface id,
surface-local `UiDocument`, resource provider, mount id, and mount root.
Compositions own cached handles, feature-local UI runtime state, and retained
event behavior; mounts own attachment and lifetime; nodes own rendering data.
Destroying a composition destroys its mount, and destroying a mount, layer,
surface, or document clear also invokes composition cleanup before retained
subtree teardown.

Retained layout is now engine-owned. `UiLayoutSpec` keeps the old absolute and
anchored fields as `UiLayoutMode::Canvas` compatibility, and also supports
container modes for `Stack`, `Grid`, `Dock`, `Overlay`, `Scroll`, `Aspect`, and
`Constraint`. `UiDocument::updateLayout(...)` performs a measure pass followed
by an arrange pass, storing measured size, bounds, content rect, and clip rect
on each node. Layout constraints include preferred/min/max size, grow/shrink,
aspect ratio, alignment, padding, margin, and gaps, with typed layout lengths
for normalized values, parent/surface percentages, content, em, and preliminary
pixel units. Rendering and hit testing consume computed layout; feature
compositions should declare layout intent instead of recomputing pixel offsets
when a container can express the geometry.

Retained styling is now engine-owned. `UiTheme` stores semantic colors,
metrics, typography, skins, panel state skins, and style classes with
state-specific overrides and inheritance. Each surface owns one active theme.
`UiSystem::setTheme(...)`, `UiSystem::theme(...)`, and
`UiSystem::setStyleClass(...)` are the main public compatibility APIs.
`UiDocument::updateLayout(...)` resolves typography during text measurement, and
`UiDocument::rebuildVisualList(...)` resolves style classes into visual
primitive colors, skins, text scale/color, image tint, and opacity. Payload-local
colors and text scales remain valid for existing builders, but new compositions
should request style classes and theme metrics instead of hardcoding
presentation constants.

Reusable retained widgets now sit above primitive nodes and below
compositions. `UiWidgetContext` exposes only the runtime access a widget needs:
`UiSystem`, surface id, document, resource provider, mount id, and composition
root. Widgets own their retained subtree and local interaction state, consume
retained events forwarded by their composition, request layout and style intent,
and expose semantic behavior such as button pressed callbacks. The first engine
widget set includes labels, panels, buttons, images, stacks, spacers,
separators, scroll views, and progress bars. Widgets remain clients of the UI
runtime; they do not own surfaces, mounts, focus, modal policy, rendering, or
composition lifetime.

Navigation is engine-owned and surface-local. Widgets or compatibility builders
register focusable retained handles with a surface's `UiNavigationGraph`,
including role, group, tab index, wrap policy, and optional explicit neighbors.
`RenderingRuntime::dispatchUiInput(...)` maps keyboard bindings into abstract
navigation intent (`Up`, `Down`, `Left`, `Right`, `Next`, `Previous`, and
`Submit`), and the dispatcher asks the selected surface graph to resolve that
intent. Explicit neighbors win first; linear traversal uses tab order with
layout order as fallback; directional traversal uses computed layout bounds.
The graph requests focus changes through `UiFocusManager` and emits retained
navigation events through the dispatcher, but it does not own focus itself.
Navigation respects surface boundaries, modal blocking, focus scopes, groups,
visibility, and enabled state. Pointer interaction remains compatible.

Drag/drop is engine-owned and surface-local. Widgets or compatibility builders
register drag sources and drop targets with a surface's `UiDragDropManager`.
Sources carry an opaque `UiDragPayload` with semantic type/id/label/object data;
targets declare accepted payload types. The dispatcher asks the drag manager to
advance the state machine after hit testing and before navigation, so dragging
can capture the pointer through `UiFocusManager`, suppress click/navigation
competition, resolve compatible targets from the retained parent path, and emit
retained drag events through the normal propagation route. The first
implementation is surface-local; cross-surface/editor-window drag is future
policy above the same source/target model.

Animation is engine-owned and surface-local for retained UI. A surface's
`UiAnimationManager` owns active property timelines for style, payload, and
layout values such as opacity, semantic colors, primitive tint/color, layout
offsets, anchors, fixed size, and scroll/content offset. The manager consumes
the shared `gts::tween` easing/progress primitives rather than duplicating
interpolation math, interrupts existing timelines by `(node, property)`, prunes
destroyed subtrees, and writes current animated values back to retained nodes
before UI command extraction. Widgets and compositions request desired
transitions through `UiSystem::animate(...)`, `animateOpacity(...)`, and
`transitionStyleState(...)`; themes remain presentation data, layout remains
geometry solving, and rendering only draws the current computed frame.

Data binding is engine-owned and surface-local for retained UI synchronization.
A surface's `UiBindingManager` owns one-way relationships from
application-owned state to retained node properties. Applications expose values
through `UiObservable<T>` or custom `UiBindingSource` callbacks; bindings can
format, transform, or compute values and apply them to text, visibility,
enabled state, opacity, progress, primitive colors/tints, style classes, and
layout properties. `RenderingRuntime::renderFrame(...)` updates bindings before
UI animations and command extraction, so bindings establish desired state and
the animation/runtime/layout path presents it. Bindings are cleaned up with
node, mount, layer, surface, and source lifetime. The current model is
one-way only; two-way editing and reflected ECS/property binding are future
runtime layers.

Accessibility semantics are engine-owned and surface-local. A surface's
`UiAccessibilityManager` owns semantic descriptors, builds a semantic tree
separate from the retained render tree, and publishes announcements for focus,
actions, live regions, values, and state changes. Widgets describe roles,
names, relationships, values, ranges, and live-region policy; focus and binding
updates feed the semantic state; node, mount, layer, and surface cleanup prune
stale semantics. Platform screen reader bridges, speech, voice, and automation
tools are future adapters over this engine semantic model.

UI serialization is engine-owned for authored retained UI structure.
`UiSerializedAsset` is a versioned JSON asset model for widget trees, layout,
style/theme references, binding paths, navigation metadata, drag/drop metadata,
animation timing hints, accessibility semantics, optional surface descriptors,
and layer descriptors. `UiSerializationRuntime` validates and instantiates an
asset into an existing mount, and `UiSystem::instantiateUiAsset(...)` exposes
that through the runtime facade. The serializer stores authored intent, not
runtime handles, focus, hover, capture, computed geometry, active animations, or
C++ callbacks. Accessibility relationships are authored by stable serialized
widget id and resolved to retained handles only after loading. Applications
provide binding source resolution and attach behavior through stable serialized
widget ids.

Widget asset definitions are engine-owned for reusable authored UI structure.
`UiWidgetAssetRegistry` stores versioned `UiWidgetAssetDefinition` data with
stable asset ids, parameters, slots, variants, inheritance, dependencies, and a
serialized root widget tree. Serialized widgets may reference an asset by id;
the registry expands those references into ordinary `UiSerializedWidget` trees
before `UiSerializationRuntime` instantiates retained nodes. This keeps asset
reuse, serialization, runtime widgets, and C++ composition behavior separate.

The Visual UI Editor is the first major application built on the UI platform.
It lives in the tools module, implements `EngineToolPanel`, and uses existing
editor shell, theme, widget, property, and workspace infrastructure. It edits
authored `UiWidgetAssetDefinition` data, selects widgets by stable authored ids,
validates through `UiWidgetAssetRegistry`, saves through widget asset
serialization, and previews by instantiating the real asset into a dedicated
preview `UiSurface`. Runtime retained handles are preview implementation
details and must not become editor document state. The editor currently
demonstrates the interaction-prompt widget asset workflow and establishes the
pattern future visual layout/theme/binding/accessibility editors should follow:
fix runtime gaps instead of adding editor-only UI systems.

UI live reload is engine-owned by `UiAssetRuntime`. It tracks authored asset
identity, source paths, runtime versions, validation state, dependencies, and
runtime consumers for serialized UI assets, widget assets, and theme assets.
`UiSystem::instantiateUiAsset(...)` and `UiSystem::instantiateWidgetAsset(...)`
register mounted runtime consumers automatically; mount and surface destruction
unregister them. Reload requests validate before replacement, update the
dependency graph, recursively invalidate dependents, rebuild affected consumers,
and preserve keyboard focus by stable widget id when possible. The Visual UI
Editor now saves widget assets and lets this runtime refresh the preview instead
of owning a private reload path. OS file watching, package/plugin ownership,
theme file serialization, and deeper state transfer are future clients of this
asset runtime.

UI package ownership is engine-owned by `UiPackageRuntime`. Packages own
authored data and plugin metadata; assets still execute through
`UiAssetRuntime`, `UiWidgetAssetRegistry`, and `UiSerializationRuntime`.
Package manifests define stable package id, version, namespace, dependencies,
optional dependencies, asset roots, tags, and future plugin metadata. Package
loading validates required dependencies, version constraints, cycles, duplicate
assets, and override policy before applying effective assets to the asset
runtime. Later packages can replace earlier effective assets only when their
asset descriptor declares `Replace`; unloading an override restores the previous
provider and rebuilds affected consumers through the existing live-reload path.
The Visual UI Editor sample now loads its reusable prompt assets from
`engine.ui` and `game.ui` packages. Runtime code plugin loading, package asset
root scanning, signatures, repository identity, and package browsers are future
layers above this ownership model.

UI extraction is surface-aware. `UiSystem::extractCommandsRef(...)` extracts
visible/render-enabled surfaces in surface order, resolves each surface's
document into a surface-local command buffer, then transforms the vertices into
that surface's normalized output rect before appending them to the submitted UI
command buffer. The current Vulkan UI stage still composites the final combined
buffer as a screen-space overlay; render-target, world-space, and multi-window
surface targets are future renderer integrations above the implemented surface
ownership boundary.

UI extraction now enforces primitive clipping before draw-command generation.
Layout specs include a default-zero child `contentOffset`, so a clipped
container can behave as a scroll view or pannable canvas without moving the
container's own background. Text nodes support measurement, word wrapping,
horizontal/vertical alignment, and max-line limits when authored with nonzero
layout bounds. Image nodes support tint and rotation around their bounds center;
unrotated images use rectangular clipping, while rotated images are emitted as
rotated textured quads after a bounds-vs-clip visibility test. Legacy text nodes
with zero bounds still render from their top-left position to preserve existing
tool and game overlays. The UI primitive set also includes retained line nodes,
which render thick colored screen-space segments for graph-like widgets such as
skill-tree links, retained circle nodes for icon buttons or graph nodes that need
circular hit targets, and retained nine-slice image nodes for scalable panels.

Editor-specific styling lives above the retained UI layer in
`modules/tools/editor/EditorTheme.h`. `EditorTheme` is a data-only token set for
editor typography, spacing, borders, corner radii, shadows, animations, widget
dimensions, semantic colors, panel/background layers, and named icon IDs.
It should migrate toward constructing or adapting a `UiTheme`; today editor
widgets and tool panels still resolve many editor tokens manually into retained
node payloads. `ToolTheme` remains as a compatibility facade over the default
editor theme for existing tool panels.

Reusable editor layout primitives live beside the theme in
`modules/tools/editor/EditorLayout.h`. This layer models editor concepts such as
dock areas, panel state, split views, panel headers/bodies, tab bars, toolbars,
sidebars, footers, collapsed/hidden/restored panel state, and resize handles,
then emits ordinary retained UI nodes. Dock dragging is not implemented yet, but
panels already carry stable ids, areas, size limits, collapsed state, visibility,
and restored sizes so future docking and workspace persistence can extend the
same state model instead of rewriting panel layout.

Reusable editor widgets live in `modules/tools/editor/EditorWidgets.h`. These
are framework-level builders and specs for hierarchy/tree views, property grids,
inspectors, asset pickers, search fields, breadcrumbs, foldouts, context and
popup menus, toolbar and icon buttons, numeric and range controls, enum
dropdowns, tag selectors, color/gradient/curve editors, timelines, graph
canvases, search palettes, validation panels, console output, and diagnostics.
The first pass is retained-UI backed and handle-oriented; command routing,
metadata binding, popup stacking, and richer interaction policy are layered on
top by later editor framework milestones.

Metadata-driven property inspection lives in
`modules/tools/editor/EditorPropertySystem.h`. It converts generic property
descriptors into `EditorPropertySpec` rows consumed by the property grid.
Metadata covers display names, descriptions/tooltips, category/group, default
values, hard and soft numeric limits, step size, visibility rules, read-only
state, asset type, units, and enum options. The conversion layer chooses the
appropriate standard widget kind from the value type, detects modified values
against defaults, applies simple dependency visibility rules, and keeps the
inspector independent from particle, scene, or component-specific data models.

Editor command routing lives in `modules/tools/editor/EditorCommands.h`.
Commands use stable string ids, labels, descriptions, shortcut action names,
enabled/checked state, and optional handlers. The standard command vocabulary
includes undo, redo, copy, paste, duplicate, delete, rename, frame selection,
reset camera, save, and reload. Input actions should resolve through the command
registry before widget-local behavior so shortcuts invoke reusable editor
commands rather than individual controls directly. `EditorUndoStack` provides a
small command-history integration point for editor applications that need local
undo/redo callbacks.

Editor workspace state lives in `modules/tools/editor/EditorWorkspace.h`.
Workspace data records stable workspace ids, display names, selected workspace
tabs, docked panel areas, open/hidden/collapsed panel state, panel sizes,
selected panel tabs, and toolbar command visibility/order. The first persistence
format is a compact quoted text format with load/save helpers; it is deliberately
separate from `EngineToolWorkspaceComponent`, which remains the per-frame scene
viewport publication surface used by the runtime and renderer.

`ParticleEffectEditorPanel` is the first editor application consuming the
framework layer. It still owns particle-specific asset editing, preview
application, module mutation, and live runtime synchronization, but its editor
regions are now described through framework panel state and its module
parameter rows can derive display metadata through the generic property system.
Further migration should continue moving shell/layout/property/command behavior
into `modules/tools/editor` while leaving particle asset semantics under the
particle rendering authoring module.

`UiPanelSkin` is the reusable panel styling contract for retained UI surfaces.
It can describe a solid-color panel, a single image panel, or a nine-slice image
panel, plus tint, source slice fractions, and content padding. `UiPanelStateSkin`
layers normal, hover, pressed, and disabled panel skins with fallback to normal
when a state skin is not supplied. `UiPanelSkinNode` in rendering UI builds a
small retained-node subtree with solid/image/nine-slice visual children and a
padded content container. Menus, inventory screens, windows, VN dialogue boxes,
choice buttons, and future UI surfaces should use this shared skin path instead
of growing feature-local panel rendering helpers.

Nine-slice UI rendering is implemented as a normal retained UI primitive. The
visual resolver splits a node into nine textured quads, preserves corner UVs,
stretches edges/center as needed, and clips each segment before emitting regular
UI draw commands. Slice values are normalized fractions of the source image and
destination bounds; a future texture-metadata pass may add pixel-exact slice
authoring without changing the retained node contract.

### Tween Module

`modules/tween/Tween.h` provides the reusable `gts::tween::Tween<T>` helper and
standard easing modes. It is a small value-interpolation utility rather than an
ECS system: engine or game systems own tweens in their own runtime state, tick
them with the appropriate fixed or controller delta time, and apply the resulting
value to their own descriptors/state. This keeps tweening reusable for UI,
presentation, tools, or gameplay-owned systems without tying it to one renderer
or component type.

### Visual Novel Module

`modules/visualnovel/` is an engine-level VN presentation/runtime module. It is
content-neutral: applications provide scripts, character IDs, image asset paths,
story state, branching labels, and gameplay-specific commands.

The module is split into:

- `runtime/`: `VNStage`, `VNRuntime`, `VNScript`, typed `VNCommand` data, and
  `VNCommandRegistry`
- `ui/`: retained UI construction/sync for dialogue boxes, choices, fullscreen
  backgrounds, dimming, and character sprite images
- `systems/`: `VNSystem`, a controller system that owns one runtime instance,
  routes continue/pointer input, syncs UI, and writes playback state
- `components/`: `VNPlaybackStateComponent`, a lightweight singleton view of
  whether VN playback is active, waiting, or blocking gameplay input, and
  `VNExternalPresentationComponent`, an optional presentation override for
  dialogue-driven VN backgrounds, dimming, and sprites

The VN stage supports multiple background modes: current scene, fullscreen image,
solid color, and none. Current-scene mode leaves the 3D scene visible and renders
VN presentation through retained UI over it. Fullscreen-image and solid-color
modes cover the scene with UI primitives before sprites and dialogue are drawn.
Sprites are independent stage entries with image asset path, normalized position,
size, scale, rotation, alpha, visibility, expression string, and z-order. Stage
animation is powered by `gts::tween::Tween<T>` for position, scale, rotation,
alpha, dimming, and effect envelopes such as shake.

`VNRuntime` executes command streams sequentially and can wait for player input,
animation completion, timers, choices, or an external custom command. Built-in
commands cover say/show/hide/move/fade/scale/rotate/shake/animate/expression/
background/dimming/wait/choice/jump. `VNCommandRegistry` lets applications register
gameplay-specific commands such as starting combat or setting relationship state
without hardcoding any game vocabulary into the engine module.

VN presentation is skinned through `VNPresentationProfile`, not through scripts.
The profile owns dialogue panel, shadow, nameplate, choice-button state skins,
text colors/scales, layout rectangles, and sprite motion presets. Scripts should
continue to describe content and presentation intent such as speaker, text,
choices, sprite identity, background mode, and named animation preset; concrete
panel textures, nine-slice values, padding, and hover/pressed visuals belong in
the active profile.

Dialogue-driven VN presentation can opt into a traditional full-VN scene without
changing dialogue graph data. A game may populate `VNExternalPresentationComponent`
with a fullscreen image background, dimming value, VN sprites, and optional
scene/particle render suppression while a `DialogueRuntimeComponent` is active.
`VNSystem` applies that override only to external dialogue presentation; if the
component is absent or inactive, dialogue continues to overlay the current 3D
scene and render pass visibility is restored to defaults.

`VNSystem` also consumes the generic execution-profile stack. While external
dialogue is active, current-scene presentation pushes the `dialogue_overlay`
profile so gameplay and physics can sleep while camera/render prep continue to
show the live scene. Fullscreen image, solid-color, or scene-suppressed
presentation pushes `fullscreen_dialogue`, which keeps `Ui`, `Dialogue`, `VN`,
`Audio`, `Tools`, and `Always` systems awake while skipping world render build.
When dialogue ends, `VNSystem` pops the named profile it pushed. This makes
fullscreen VN an engine execution mode rather than a VN-specific pause branch.

### Dialogue Module

`modules/dialogue/` is a generic graph-based narrative runtime. It owns dialogue
graph execution, current node tracking, visible-choice evaluation, branching,
generic conditions, generic actions, and start/end state. It deliberately knows
nothing about game concepts such as quests, shops, items, factions, reputation,
or party members.

The data model is:

- `DialogueGraph`: graph id, start node id, and node map
- `DialogueNode`: speaker, text, optional linear `nextNode`, choices, on-enter
  actions, on-exit actions, and an explicit end flag
- `DialogueChoice`: display text, target node, conditions, and actions
- `DialogueCondition` / `DialogueAction`: string id plus string argument map

`DialogueRuntime` executes one active graph. It enters the start node, evaluates
visible choices through `DialogueConditionRegistry`, dispatches actions through
`DialogueActionRegistry`, follows choice target nodes, advances through a node's
linear `nextNode` when no choices are visible, and ends when it reaches an end
node, a missing node when configured to end, a choice or node with no successor,
or an explicit `end()` call. Unknown conditions fail closed and hide the choice.
Unknown actions are ignored by default, or can end dialogue through
`DialogueRuntimeConfig`.

The ECS-facing layer provides:

- `DialogueRuntimeComponent`: singleton runtime storage
- `DialogueGraphRegistryComponent`: optional singleton graph catalog keyed by id
- `DialogueStartRequestComponent`: optional singleton request surface for
  starting either a registered graph id or an inline graph
- `NPCInteractionComponent`: generic character/dialogue graph reference for
  game-owned NPC entities
- `DialogueSystem`: controller system that processes start requests and
  publishes `DialogueStartedEvent`, `DialogueNodeChangedEvent`,
  `DialogueChoicesChangedEvent`, `DialogueActionRequestedEvent`, and
  `DialogueEndedEvent`

The VN module consumes dialogue state only as presentation. When a
`DialogueRuntimeComponent` is active, `VNSystem` mirrors the current dialogue
node into its retained VN UI state and routes choice clicks or continue input
back into `DialogueRuntime`. This bridge does not add rendering code to the
dialogue runtime and does not put quest/shop/item logic into VN presentation.

Engine tooling is a global development runtime owned by `GravitasEngine`.
`EngineToolRuntime` updates once per rendered frame after the active scene's
normal controller systems, but it targets the active scene `ECSWorld` for
inspection and edits. Game scenes no longer install editor systems directly.
Renderer, physics, gameplay, and game UI systems remain scene-owned.

Current tool feature layout:

- `modules/tools/core/`: panel interface, registry, context, and shared state
- `modules/tools/ui/`: tool widget helpers built on retained UI
- `modules/tools/runtime/`: global tool runtime and scene-change state handoff
- `modules/tools/inspectors/`: entity and particle emitter inspector panels
- `modules/tools/assets/`: particle effect editor and particle asset/hot-reload
  status panel
- `modules/tools/scenes/`: generic registered-scene browser panel
- `modules/tools/selection/`: input capture, world picking, selection labels,
  selection highlight, and shared raycast helpers
- `modules/tools/gizmos/`: translation gizmo state, panel, picking, snapping,
  and transform edits
- `modules/tools/debugdraw/`: debug-draw settings panel and tool-driven bounds,
  axes, frustum, and pick-ray drawing

Tooling state is held in singleton ECS components:

- `EngineToolStateComponent`: visibility, active panel, selected entity, status
- `EngineToolInputCaptureComponent`: pointer/UI/world-consumption state
- `EngineGizmoStateComponent`: gizmo mode, hovered/active axis, snap settings
- `DebugDrawSettingsComponent`: debug primitive toggles and sizing

The runtime carries editor-level state across scene changes, such as F6
visibility, active panel, gizmo settings, and debug-draw settings. Scene-entity
references are reset whenever the active scene changes because entity IDs are
owned by the active scene world. Scene-local singleton components are seeded
from the runtime each frame so existing tool systems can operate on the active
scene without owning scene registration.

Workspace and viewport state are published during the runtime prepare step,
before scene load and scene controller systems run. This guarantees camera,
picking, and render extraction systems see the reduced editor viewport
immediately after scene loads instead of spending a frame on full-window
assumptions.
Scene controller contexts keep full output dimensions in `windowPixelWidth` and
`windowPixelHeight` for retained UI extraction, and expose the active renderable
scene viewport separately through `sceneViewportPixel*` and
`sceneViewportAspectRatio`.

Global tool systems are timed back into the active world's controller profile
table, so frame profile output continues to show individual `gts::tools::*`
systems even though those systems are no longer scene-owned.

`SceneManager` owns the registered scene catalog, scene factories, active scene
instance, and active scene id. Tooling receives catalog metadata through
`EcsControllerContext`, and the generic `Scenes` panel lists whatever scenes the
application registered. Selecting a row issues
`GtsCommandBuffer::requestChangeScene(sceneId)`. Engine tooling must not
hardcode game scene names or keep scene-entity references across scene changes.

Selection can come from either inspector panels or world picking. The world
picker uses `ActiveCameraViewStateComponent` plus `BoundsComponent` to raycast
against live entities. `ToolEntityLabelComponent` gives entities human-readable
names/categories and can mark internal tool entities as non-selectable.

The translation gizmo is currently position-only. It draws X/Y/Z axes through
debug draw, follows Unreal-style colors (X red, Y green, Z blue), supports world
or local axes, and writes selected entity `TransformComponent::position` while
marking the transform dirty.

The engine tool camera is active only while the engine tools are visible. It
uses `engine.tool_camera_*` actions inside the `engine.tools` input context.
Game debug cameras are application-owned systems and should not be installed by
the shared renderer feature.

### Engine Font

Engine debug/tool overlays use `resources/fonts/gravitasfont.font.json`, which
points at `resources/fonts/gravitasfont.png` and stores atlas metrics. This
replaced the older generated retro font for engine UI because the upscaled cell
grid gives readable lowercase letters, digits, and symbols without introducing
image-filter noise.

---

## 9. Resource Model

Assets are accessed through `IResourceProvider` (passed in `SceneContext`). Binding systems call into this interface when descriptor components are first processed:

- **Meshes**: loaded from `.obj` via tinyobjloader, uploaded to GPU vertex/index buffers
- **Textures**: decoded via stb_image, uploaded to Vulkan images
- **Fonts**: loaded from `.font.json`; `FontManager` owns glyph metrics and uses `TextureManager` for atlas textures
- **Shaders**: pre-compiled SPIR-V stored in `/shaders/`
- **Engine assets**: minimal shared resources in `/resources/`

GPU resource handles (mesh IDs, texture IDs, font IDs, SSBO slots, camera view IDs) are allocated by lifecycle/binding systems and stored in GPU/runtime components. The steady-state contract is:
- lifecycle systems do near-zero work when no descriptor lifecycle work is pending
- structural GPU companion-component changes are deferred through `world.commands()`
- cleanup of backend resources still happens via removal callbacks on GPU components
- application/game code does not create, remove, or read GPU companion components directly
- camera view IDs are allocated by the camera binding lifecycle, but camera
  matrix UBO writes are renderer-owned and occur after the current frame's fence
  wait

`GraphicsConfig::window.vsync` is authoritative for swapchain present mode:
when true the Vulkan module requests FIFO; when false it uses
`GraphicsConfig::presentModePreference` (Immediate by default).

Runtime graphics changes are engine-owned and travel through engine-facing
types only:

- `RuntimeGraphicsSettings` carries requested render resolution, window mode,
  monitor index, vsync, present-mode preference, and optional max frame rate.
- Game/UI code requests changes through `gts::rendering::requestApplyGraphicsSettings(...)`.
- `IGtsGraphicsModule::applyRuntimeGraphicsSettings` applies the graphics
  portion; the main engine loop applies frame pacing from `maxFrameRate`.

The renderer can separate scene render resolution from output resolution.
Windowed and exclusive fullscreen render directly to the selected output
resolution. Borderless fullscreen keeps the desktop-sized swapchain, renders
scene and particles into an offscreen color target at the requested render
resolution, composites that image into the current frame output, and then
renders UI at output/swapchain resolution.

Viewport restriction is tracked in output pixels by `RenderViewportComponent`.
The Vulkan frame path expands that into a per-frame viewport payload: scene and
particle stages use a matching scene render-target viewport, while the upscale
stage uses the output viewport and samples from the matching source rectangle
when internal scaling is active. This keeps tool chrome from being covered by
scene rendering without stretching the internally scaled scene.

For Vulkan window output, framebuffer resize and out-of-date/suboptimal present
paths request swapchain recreation. Recreation waits for the device to go idle,
refreshes surface capabilities, recreates the swapchain and image views, and
then rebuilds swapchain-dependent frame resources: frame output target,
optional offscreen scene target, depth attachment, frame graph imports/stages,
framebuffers, and frame manager. Long-lived resource managers for meshes,
textures, camera buffers, and SSBO slots are preserved across the rebuild.

Monitor enumeration/selection is exposed through the generic window/graphics
abstraction so game menus can choose which display owns windowed, borderless,
or exclusive fullscreen output without depending on GLFW directly.

---

## 10. Extensibility

### Adding a Simulation System

1. Derive from `ECSSimulationSystem`
2. Implement `update(const EcsSimulationContext& ctx)`
3. Register with `world.addSimulationSystem<MySystem>(EcsSystemGroup::Gameplay)`
   or another broad group from `onLoad()`

### Adding a Controller System

1. Derive from `ECSControllerSystem`
2. Implement `update(const EcsControllerContext& ctx)`
3. Register with `world.addControllerSystem<MySystem>(EcsSystemGroup::Ui)` or
   another broad group from `onLoad()`

Choose the broadest accurate execution group. Use `Always` sparingly for systems
that must run in every runtime mode. Most scene gameplay should use
`Gameplay`; renderer companion/lifecycle work should use `RenderPrep`; camera
presentation should use `Camera`; retained UI sync/interaction should use `Ui`;
dialogue graph bridges should use `Dialogue`; VN presentation should use `VN`;
engine or game debug tooling should use `Tools`.

### Adding a New Component

Define a plain struct. Add it to entities with `world.addComponent<MyComp>(e, val)`. No registration is required unless you need lifecycle callbacks.

### Adding a Render Stage

1. Derive from `GtsRenderStage`
2. Declare resource reads/writes in `compile()`
3. Record Vulkan commands in `execute()`
4. Add to frame graph after construction

### Camera Override

Add a `CameraOverrideComponent` to replace the default camera behavior. Implement a controller system that writes custom view/projection matrices to `CameraGpuComponent`.

Most application code should still author only descriptor/control components.
Direct writes to `CameraGpuComponent` are reserved for explicit custom camera
override systems that own a `CameraOverrideComponent`.

### Adding a Tool Panel

1. Derive from `gts::tools::EngineToolPanel`
2. Build retained UI nodes in `build(...)`
3. Read/write only the state or descriptors owned by that panel's feature
4. Register the panel in `EngineToolShellSystem`

Prefer extending `ToolWidgets` for reusable controls instead of building
one-off retained UI interaction code inside a panel.

New reusable retained UI should prefer a `UiComposition` mounted through
`UiSystem` and should handle retained input through `UiComposition::onEvent(...)`
instead of storing top-level handles in a controller or scene. Legacy tool
panels still use the older panel interface and should migrate one panel at a
time after node/widget event registration and drag/drop are designed.

---

## 11. Key Design Principles

- **ECS-first**: all state is components, all behavior is systems — no monolithic managers
- **Deterministic simulation**: simulation systems run at fixed timestep, isolated from frame timing
- **Simulation-safe input**: fixed-step gameplay reads latched simulation input
  edges, not frame-local controller input edges
- **CPU/GPU separation**: descriptor components are game-logic; GPU components are backend state; binding systems bridge them explicitly
- **Descriptor-only app boundary**: applications and gameplay/UI code operate on descriptors or engine-exported frame state, not GPU companion components
- **Lazy updates**: render commands and GPU state are cached and only rebuilt on change
- **Fence-safe frame data**: per-frame camera data is uploaded by the renderer after the matching frame fence has been waited
- **Data extraction over direct coupling**: the renderer never reads ECS directly; the extractor produces an immutable command list
- **Explicit structural mutation**: hot-path queries stay non-structural; lifecycle mutation is deferred and flushed at controlled points
- **Profile-driven runtime modes**: scenes stay loaded while execution profiles
  decide which broad system groups and render build paths are awake
- **Separation of lifecycle concerns**: geometry lifecycle, camera lifecycle, cleanup, transform sync, and extraction are distinct stages with distinct ownership
- **RAII resource management**: backend resources tied to GPU component lifecycle via removal callbacks
- **Modular assembly**: scenes are built by adding systems and components — the engine imposes no mandatory scene structure
- **Feature-first tooling**: editor/tooling panels live under `modules/tools/`, generic visualization under `modules/debugdraw/`, and runtime particle simulation under rendering particle ECS setup
