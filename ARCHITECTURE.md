# Gravitas Engine — Architecture

## 1. Overview

Gravitas is a C++20 Vulkan game engine built around a two-tier ECS (Entity-Component-System) architecture. The engine separates simulation logic from rendering concerns through a data extraction pipeline, and provides a fixed-timestep simulation loop decoupled from the render frame rate.

**Core philosophy:**
- ECS-first: all game state lives in components, all behavior lives in systems
- Strict separation between CPU logic and GPU state
- Modular subsystems assembled at scene construction time
- Registered scenes are factories; only the active scene owns runtime ECS/GPU state
- Descriptor/GPU component split: application writes descriptors, engine manages GPU resources
- Lifecycle work is explicit: descriptor changes queue intent, lifecycle systems perform structural mutation

---

## 2. Project Structure

```
Gravitas/
├── engine/
│   ├── core/               # Pure C++ ECS framework, input, scene, UI, events
│   │   ├── ecs/            # Entity, component storage, system bases, extractors
│   │   ├── input/          # Raw keys, semantic action mapping
│   │   ├── scene/          # SceneContext, SceneManager
│   │   ├── ui/             # Retained-mode UI document model
│   │   └── event/          # Event bus
│   ├── modules/            # Feature modules built on core
│   │   ├── transform/      # TransformComponent (position, rotation, scale)
│   │   ├── hierarchy/      # Parent-child transform hierarchy
│   │   ├── animation/      # Keyframe animation
│   │   ├── tween/          # Reusable value tween/easing helpers
│   │   ├── debugdraw/      # Feature-local line/bounds/frustum debug primitives
│   │   ├── physics/        # Physics world, PhysicsSystem, body components
│   │   ├── tools/          # Optional in-engine inspection/editing toolchain
│   │   ├── visualnovel/    # VN stage, command runtime, and retained UI overlay
│   │   └── rendering/      # Vulkan backend, GPU components, binding systems
│   │       ├── ecssetup/   # Camera, geometry, text, and particle ECS setup
│   │       └── backend/    # Vulkan device, frame graph, render stages, resources
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

Singletons are used for global state: camera, physics world, time context.

---

### System Types

The engine defines two system base classes with distinct contracts:

#### ECSSimulationSystem
```
update(const EcsSimulationContext& ctx)
```
- Runs at **fixed timestep** (default 20 Hz)
- Context provides: `world`, fixed `dt`, read-only `InputSnapshot`
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

---

## 4. Frame Data Flow

```
┌────────────────────────────────────────────────────────┐
│  Fixed-Timestep Loop (1+ ticks per frame)              │
│  ECSSimulationSystem::update(dt)                       │
│    └─ PhysicsSystem, AnimationSystem, [user systems]   │
├────────────────────────────────────────────────────────┤
│  Per-Frame Controller Pass                             │
│  ECSControllerSystem::update(ctx)                      │
│    └─ Lifecycle systems (descriptor → GPU companions)  │
│    └─ Cleanup systems (GPU companion teardown)         │
│    └─ RenderGpuSystem (sync world matrices)            │
│    └─ Camera control / CameraGpuSystem / view IDs      │
│    └─ ParticleEmitterSystem (particle frame data)       │
│    └─ [user systems] (input handling, gameplay logic)  │
├────────────────────────────────────────────────────────┤
│  Data Extraction                                       │
│  RenderPipeline build stages                          │
│    └─ Frustum cull against camera planes               │
│    └─ Emit sorted RenderCommand list + upload commands │
│  UiSystem::extractCommands()                           │
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

### Descriptor → GPU Component Split

Application code writes **descriptor components** (high-level intent). Engine systems translate these into **GPU components** (backend handles) through explicit lifecycle passes.

```
StaticMeshComponent("path/to/mesh.obj")   ← written by application
        ↓
StaticMeshBindingSystem (controller)      ← allocates Vulkan mesh, assigns meshID
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

Particles are an engine rendering feature with ECS-facing authoring and a
separate Vulkan render stage.

Feature layout:

- `core/ecs/desc/ParticleEmitterComponent.h`: game-facing particle emitter descriptor
- `core/ecs/desc/ParticleTypes.h`: authoring types for curves, shapes, bursts,
  flipbooks, and force module data
- `modules/rendering/ecssetup/particles/components/`: runtime particle state
- `modules/rendering/ecssetup/particles/assets/`: JSON particle effect
  load/save and hot-reload registry
- `modules/rendering/ecssetup/particles/systems/`: emitter simulation and
  effect hot reload
- `modules/rendering/ecssetup/particles/extraction/`: per-frame particle draw data
- `modules/rendering/backend/vulkan/rendering/particles/`: billboard and mesh
  particle render stage

Application code authors `ParticleEmitterComponent` plus `TransformComponent`.
The engine creates `ParticleEmitterRuntimeComponent`, simulates particles every
controller frame, sorts alpha particles by camera depth, batches by primitive,
mesh, texture, and blend mode, and renders particles in `ParticleRenderStage`.
Hot-reload bookkeeping, including the last applied effect asset version, lives
in `ParticleEmitterRuntimeComponent`.

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
- texture path, optional mesh path, and JSON effect path

`ParticleEffectHotReloadSystem` loads effect files into a singleton registry and
copies asset values onto emitters whose `effectPath` is set and
`reloadFromEffect` is true. The tools inspector can edit live emitter descriptors
and save them back through the asset IO path.

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

The retained UI model is an engine core service. `UiDocument` stores retained
nodes, `UiSystem` owns per-frame extraction and interaction evaluation, and
tool widgets in `modules/tools/ui/` provide reusable engine-editor controls
on top of that UI system.

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
skill-tree links, and retained circle nodes for icon buttons or graph nodes that
need circular hit targets.

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
  whether VN playback is active, waiting, or blocking gameplay input

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
commands cover say/show/hide/move/fade/scale/rotate/shake/expression/background/
dimming/wait/choice/jump. `VNCommandRegistry` lets applications register
gameplay-specific commands such as starting combat or setting relationship state
without hardcoding any game vocabulary into the engine module.

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
- `modules/tools/assets/`: particle asset/hot-reload status panel
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
- Game/UI code requests changes through `GtsCommandBuffer::requestApplyGraphicsSettings`.
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
3. Register with `world.addSimulationSystem<MySystem>()` from `onLoad()`

### Adding a Controller System

1. Derive from `ECSControllerSystem`
2. Implement `update(const EcsControllerContext& ctx)`
3. Register with `world.addControllerSystem<MySystem>()` from `onLoad()`

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
- **Separation of lifecycle concerns**: geometry lifecycle, camera lifecycle, cleanup, transform sync, and extraction are distinct stages with distinct ownership
- **RAII resource management**: backend resources tied to GPU component lifecycle via removal callbacks
- **Modular assembly**: scenes are built by adding systems and components — the engine imposes no mandatory scene structure
- **Feature-first tooling**: editor/tooling panels live under `modules/tools/`, generic visualization under `modules/debugdraw/`, and runtime particle simulation under rendering particle ECS setup
