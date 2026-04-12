# Gravitas Engine — Architecture

## 1. Overview

Gravitas is a C++20 Vulkan game engine built around a two-tier ECS (Entity-Component-System) architecture. The engine separates simulation logic from rendering concerns through a data extraction pipeline, and provides a fixed-timestep simulation loop decoupled from the render frame rate.

**Core philosophy:**
- ECS-first: all game state lives in components, all behavior lives in systems
- Strict separation between CPU logic and GPU state
- Modular subsystems assembled at scene construction time
- Descriptor/GPU component split: application writes descriptors, engine manages GPU resources

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
│   │   ├── physics/        # Physics world, PhysicsSystem, body components
│   │   └── rendering/      # Vulkan backend, GPU components, binding systems
│   │       ├── ecssetup/   # Camera and geometry GPU components + binding systems
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

**Component Storage** — each component type gets its own sparse-dense array:
- Sparse layer (paged, 4096 entries/page) for O(1) lookup by entity ID
- Dense layer (contiguous) for cache-friendly iteration
- Swap-remove deletion is O(1)

**ECSWorld** — the central registry. Key operations:

| Operation | Description |
|-----------|-------------|
| `createEntity()` / `destroyEntity(e)` | Lifecycle management |
| `addComponent<C>(e, c)` / `getComponent<C>(e)` | Per-entity component access |
| `forEach<C1, C2, ...>(fn)` | Iterate all entities with a component set |
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
- Context provides: `world`, `resources`, `inputSource`, `actions`, `time`, `physics`, `ui`, `extractor`, `engineCommands`, window dimensions
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
│    └─ Binding systems (alloc GPU resources)            │
│    └─ RenderGpuSystem (sync transforms)                │
│    └─ CameraGpuSystem (compute view/proj matrices)     │
│    └─ [user systems] (input handling, gameplay logic)  │
├────────────────────────────────────────────────────────┤
│  Data Extraction                                       │
│  RenderCommandExtractor::extract()                     │
│    └─ Frustum cull against camera planes               │
│    └─ Emit sorted RenderCommand list                   │
│  UiSystem::extractCommands()                           │
│    └─ Flatten retained UI tree → draw calls            │
├────────────────────────────────────────────────────────┤
│  Vulkan Render                                         │
│  Frame Graph execute()                                 │
│    └─ SceneRenderStage (geometry + materials)          │
│    └─ UiRenderStage (overlay)                          │
│    └─ Swapchain present                                │
└────────────────────────────────────────────────────────┘
```

**EcsControllerContext** is the data hub passed to every controller system each frame:

```
EcsControllerContext {
    ECSWorld&                   world
    IResourceProvider*          resources
    IInputSource*               inputSource       (pause-gated — null when paused)
    InputActionManager*         actions           (always live)
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
    const InputSnapshot*    input           (sampled once per frame — null on first frame)
}
```

---

## 5. Rendering Model

### Descriptor → GPU Component Split

Application code writes **descriptor components** (high-level intent). Engine systems translate these into **GPU components** (backend handles).

```
StaticMeshComponent("path/to/mesh.obj")   ← written by application
        ↓
StaticMeshBindingSystem (controller)      ← allocates Vulkan mesh, assigns meshID
        ↓
MeshGpuComponent { mesh_id_type meshID }  ← written by engine
        ↓
RenderCommandExtractor reads both         ← emits RenderCommand
```

Removal callbacks on descriptor components automatically free GPU resources when entities are destroyed.

### Descriptor Components (application-facing)

| Component | Purpose |
|-----------|---------|
| `TransformComponent` | Position, rotation, scale |
| `StaticMeshComponent` | Asset path to mesh |
| `MaterialComponent` | Texture path and surface parameters |
| `BoundsComponent` | Local AABB used for frustum culling |
| `CameraDescriptionComponent` | FOV, near/far clip planes |
| `PhysicsBodyComponent` | Dynamic vs static body flag |

### GPU Components (engine-managed)

| Component | Managed By |
|-----------|-----------|
| `MeshGpuComponent` | `StaticMeshBindingSystem` |
| `MaterialGpuComponent` | `MaterialBindingSystem` |
| `RenderGpuComponent` | `RenderGpuSystem` |
| `CameraGpuComponent` | `CameraGpuSystem` + `CameraBindingSystem` |

### RenderCommand

The extractor output:
```
RenderCommand {
    mesh_id_type       meshID
    texture_id_type    textureID
    ssbo_id_type       objectSSBOSlot    (per-object transform)
    view_id_type       cameraViewID      (camera UBO)
    glm::mat4          modelMatrix
    float              alpha
    bool               doubleSided
}
```
Sorted by (double-sided, meshID, textureID) to minimize state changes. Cached across frames; only rebuilt when dirty.

### Frustum Culling

`FrustumCuller` extracts 6 planes from the view-projection matrix (Gribb-Hartmann) and tests entity AABBs from `BoundsComponent`. Entities without bounds are always rendered. Culling can be frozen (debug feature) to inspect which objects are culled while the camera moves.

### Frame Graph

The Vulkan backend uses a declarative frame graph:
- Stages declare their image/buffer read-write access patterns at `compile()` time
- `execute()` records command buffers with automatic resource layout transitions
- Resource types: external (swapchain), transient (render targets)

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

Raw input is abstracted behind `IInputSource` and mapped to semantic actions via `InputActionManager`:

```
Hardware → GtsPlatform → IInputSource → InputActionManager<GtsAction>
                                                  ↓
                              Controller systems poll via EcsControllerContext::actions
```

`GtsAction` defines engine-level semantics (Pause, ZoomIn, CloseApp, etc.). Applications extend this mapping for game-specific actions.

---

## 8. Resource Model

Assets are accessed through `IResourceProvider` (passed in `SceneContext`). Binding systems call into this interface when descriptor components are first processed:

- **Meshes**: loaded from `.obj` via tinyobjloader, uploaded to GPU vertex/index buffers
- **Textures**: decoded via stb_image, uploaded to Vulkan images
- **Fonts**: loaded for the UI system's text rendering
- **Shaders**: pre-compiled SPIR-V stored in `/shaders/`
- **Engine assets**: minimal shared resources in `/resources/`

GPU resource handles (mesh IDs, texture IDs, SSBO slots) are allocated by binding systems and stored in GPU components. Resources are freed via removal callbacks when their owner entity is destroyed.

---

## 9. Extensibility

### Adding a Simulation System

1. Derive from `ECSSimulationSystem`
2. Implement `update(const EcsSimulationContext& ctx)`
3. Register with `world.addSimulationSystem<MySystem>()` from `onLoad()`

### Adding a Controller System

1. Derive from `ECSControllerSystem`
2. Implement `update(const EcsControllerContext& ctx)`
3. Register with `world.addControllerSystem<MySystem>()` from `onLoad()`

### Adding a New Component

Define a plain struct. Add it to entities with `world.addComponent<MyComp>(e, val)`. No registration required unless you need removal callbacks.

### Adding a Render Stage

1. Derive from `GtsRenderStage`
2. Declare resource reads/writes in `compile()`
3. Record Vulkan commands in `execute()`
4. Add to frame graph after construction

### Camera Override

Add a `CameraOverrideComponent` to replace the default camera behavior. Implement a controller system that writes custom view/projection matrices to `CameraGpuComponent`.

---

## 10. Key Design Principles

- **ECS-first**: all state is components, all behavior is systems — no monolithic managers
- **Deterministic simulation**: simulation systems run at fixed timestep, isolated from frame timing
- **CPU/GPU separation**: descriptor components are game-logic; GPU components are backend state; binding systems bridge them
- **Lazy updates**: render commands and GPU state are cached and only rebuilt on change
- **Data extraction over direct coupling**: the renderer never reads ECS directly; the extractor produces an immutable command list
- **RAII resource management**: GPU resources tied to component lifecycle via removal callbacks
- **Modular assembly**: scenes are built by adding systems and components — the engine imposes no mandatory scene structure
