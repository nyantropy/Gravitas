# Gravitas Engine — Architecture

## 1. Overview

Gravitas is a C++20 Vulkan game engine built around a two-tier ECS (Entity-Component-System) architecture. The engine separates simulation logic from rendering concerns through a data extraction pipeline, and provides a fixed-timestep simulation loop decoupled from the render frame rate.

**Core philosophy:**
- ECS-first: all game state lives in components, all behavior lives in systems
- Strict separation between CPU logic and GPU state
- Modular subsystems assembled at scene construction time
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
│   │   ├── debugdraw/      # Feature-local line/bounds/frustum debug primitives
│   │   ├── physics/        # Physics world, PhysicsSystem, body components
│   │   ├── tools/          # Optional in-engine inspection/editing toolchain
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
- Context provides: `world`, `resources`, `input`, `time`, `physics`, `ui`, `extractor`, `engineCommands`, window dimensions
- All context pointers are valid for the duration of the `update()` call only — do not cache them
- Can issue engine commands (scene transitions, pause/resume) via `ctx.engineCommands`
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
    GtsCommandBuffer*           engineCommands    (scene/pause requests)
    RenderCommandExtractor*     extractor
    UiSystem*                   ui
    IGtsPhysicsModule*          physics
    float windowAspectRatio, windowPixelWidth/Height
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
- **Geometry lifecycle** queues and resolves static mesh / procedural mesh companion state
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

### Descriptor Components (application-facing)

| Component | Purpose |
|-----------|---------|
| `TransformComponent` | Position, rotation, scale |
| `StaticMeshComponent` | Asset path to mesh |
| `MaterialComponent` | Texture path, tint, alpha, culling, and optional vertex-color-only rendering |
| `BoundsComponent` | Local AABB used for frustum culling |
| `CameraDescriptionComponent` | FOV, near/far clip planes |
| `PhysicsBodyComponent` | Dynamic vs static body flag |
| `ParticleEmitterComponent` | Game-facing particle emitter descriptor paired with `TransformComponent` |

### GPU Components (engine-managed)

| Component | Managed By |
|-----------|-----------|
| `MeshGpuComponent` | `StaticMeshBindingSystem` |
| `MaterialGpuComponent` | geometry lifecycle systems |
| `RenderGpuComponent` | geometry lifecycle systems + `RenderableCleanupSystem` + `RenderGpuSystem` |
| `CameraGpuComponent` | `CameraLifecycleSystem` + `CameraGpuSystem` + `CameraBindingSystem` for view IDs; renderer for UBO upload |

Particle emitters use a runtime companion component (`ParticleEmitterRuntimeComponent`)
for simulation state, but particles are extracted into `ParticleFrameDataComponent`
instead of flowing through `RenderCommand`.

### Engine-Exported Frame State

| Component | Purpose |
|-----------|---------|
| `ActiveCameraViewStateComponent` | Read-only current camera view/projection snapshot for non-renderer consumers such as UI projection |

### Controller Order Contract

The shared renderer feature installs controller systems in this order:

1. Geometry and text binding systems
2. `RenderableCleanupSystem`
3. `RenderGpuSystem`
4. `CameraLifecycleSystem`
5. camera control systems
6. `CameraGpuSystem`
7. debug free camera control
8. camera view ID allocation / active-camera export systems
9. particle effect hot reload and particle emitter simulation

This order is intentional:
- lifecycle runs before per-frame sync so newly created GPU companions participate immediately
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
    float              alpha
    bool               doubleSided
    bool               vertexColorOnly
}
```
Sorted by (double-sided, vertex-color-only, meshID, textureID) to minimize state changes. Cached across frames; only rebuilt when renderable content, visibility, camera version, or active camera view changes.

Render extraction also emits upload command side channels:

```
ObjectUploadCommand {
    ssbo_id_type objectSSBOSlot
    glm::mat4    modelMatrix
}

CameraUploadCommand {
    view_id_type cameraViewID
    glm::mat4    viewMatrix
    glm::mat4    projMatrix
}
```

Object uploads are generated only for renderables marked dirty by the render
invalidation queue. Camera uploads are generated every extracted frame for the
active camera and consumed by the renderer after the current-frame fence wait.

`vertexColorOnly` is a material/render-command flag for tool and debug meshes
that should render from authored vertex colors without sampling a color texture.
The regular material texture path remains the default for game geometry.

### Particle Rendering

Particles are an engine rendering feature with ECS-facing authoring and a
separate Vulkan render stage.

Feature layout:

- `modules/rendering/ecssetup/particles/components/`: authoring descriptors,
  runtime particles, curves, shapes, bursts, flipbooks, and force module data
- `modules/rendering/ecssetup/particles/assets/`: JSON particle effect
  load/save and hot-reload registry
- `modules/rendering/ecssetup/particles/systems/`: emitter simulation and
  effect hot reload
- `modules/rendering/ecssetup/particles/extraction/`: per-frame particle draw data
- `modules/rendering/backend/vulkan/rendering/particles/`: billboard particle
  render stage

Application code authors `ParticleEmitterComponent` plus `TransformComponent`.
The engine creates `ParticleEmitterRuntimeComponent`, simulates particles every
controller frame, sorts alpha particles by camera depth, batches by texture and
blend mode, and renders billboards in `ParticleRenderStage`.

Particle controls currently include:

- local-space or world-space simulation
- sphere, box, disc, cylinder, and ring emit shapes
- alpha and additive blend modes
- base tint, color over lifetime, alpha over lifetime, and size over lifetime
- emission rate, max particles, lifetime range, duration, delay, and intensity
- initial velocity, spread, radial/tangent velocity, drag, spin, and random size
- hue/value variation
- bursts, flipbooks, softness, wind/acceleration/vortex/radial/noise forces
- texture path and JSON effect path

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
procedural meshes. Debug draw uses `MaterialComponent::vertexColorOnly` so axes,
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

Input contexts are stack-like named scopes. The engine tools and debug camera use
their own contexts (`engine.tools`, `engine.debug_camera`) so UI clicking,
world-picking, gizmo dragging, and free-camera input are mediated through the
same input abstraction as keyboard actions instead of reaching into GLFW.

Mouse position, buttons, scroll deltas, and cursor capture flow through
`IInputSource` and `InputBindingRegistry`; engine systems should consume those
values from `EcsControllerContext::input`.

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
layout bounds. Legacy text nodes with zero bounds still render from their
top-left position to preserve existing tool and game overlays. The UI primitive
set also includes retained line nodes, which render thick colored screen-space
segments for graph-like widgets such as skill-tree links, and retained circle
nodes for icon buttons or graph nodes that need circular hit targets.

Tooling is optional and installed per scene through `GtsScene::installToolingFeature()`.
It is deliberately separate from `installRendererFeature()` so shipping scenes
can opt out.

Current tool feature layout:

- `modules/tools/core/`: panel interface, registry, context, and shared state
- `modules/tools/ui/`: tool widget helpers built on retained UI
- `modules/tools/inspectors/`: entity and particle emitter inspector panels
- `modules/tools/assets/`: particle asset/hot-reload status panel
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

Selection can come from either inspector panels or world picking. The world
picker uses `ActiveCameraViewStateComponent` plus `BoundsComponent` to raycast
against live entities. `ToolEntityLabelComponent` gives entities human-readable
names/categories and can mark internal tool entities as non-selectable.

The translation gizmo is currently position-only. It draws X/Y/Z axes through
debug draw, follows Unreal-style colors (X red, Y green, Z blue), supports world
or local axes, and writes selected entity `TransformComponent::position` while
marking the transform dirty.

The debug free camera is renderer-installed and tool-friendly. It uses
`engine.debug_camera` input bindings, right-mouse look, and context push/pop
through the input registry.

### Engine Font

Engine debug/tool overlays use `resources/fonts/gravitasfont.png` with
`GravitasFontAtlas`. This replaced the older generated retro font for engine UI
because the upscaled cell grid gives readable lowercase letters, digits, and
symbols without introducing image-filter noise.

---

## 9. Resource Model

Assets are accessed through `IResourceProvider` (passed in `SceneContext`). Binding systems call into this interface when descriptor components are first processed:

- **Meshes**: loaded from `.obj` via tinyobjloader, uploaded to GPU vertex/index buffers
- **Textures**: decoded via stb_image, uploaded to Vulkan images
- **Fonts**: loaded for the UI system's text rendering
- **Shaders**: pre-compiled SPIR-V stored in `/shaders/`
- **Engine assets**: minimal shared resources in `/resources/`

GPU resource handles (mesh IDs, texture IDs, SSBO slots, camera view IDs) are allocated by lifecycle/binding systems and stored in GPU components. The steady-state contract is:
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

Application code should still author only descriptor/control components. Direct writes to `CameraGpuComponent` are reserved for engine-owned override paths.

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
