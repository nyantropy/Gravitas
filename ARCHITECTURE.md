# Gravitas Engine Architecture Index

Gravitas is a C++20 modular application engine built around a two-tier ECS
architecture. The engine separates fixed-step simulation from frame-facing
controller work and assembles optional feature modules such as rendering,
physics, tooling, narrative, visual novel presentation, diagnostics, tweening,
and future audio through explicit engine and scene hooks.

This file is the engine architecture entrypoint. Feature details live under
`docs/` so this index can stay readable for agents.

## Core Philosophy

- KISS (Keep It Simple): prefer direct, readable code and explicit ownership.
  Add abstractions only when they solve a concrete problem; do not introduce
  interfaces, factories, or registration layers for hypothetical future needs.
- Group related code by responsibility. Keep small data contracts independent
  of loading, parsing, and runtime implementation dependencies. Put substantial
  implementation in a neighboring source file, not another abstraction layer.
- Optimize for comprehension as well as reuse: descriptive names, straightforward
  control flow, and few jumps between files. A helper is useful when it names a
  meaningful operation or removes real duplication, not merely to shorten a function.
- ECS-first: state lives in components, behavior lives in systems.
- Strict separation between CPU logic and GPU/backend state.
- Descriptor/runtime split: applications write descriptors; engine systems
  manage runtime/GPU companions.
- Registered scenes are factories; only the active scene owns runtime ECS/GPU
  state.
- Lifecycle work is explicit: descriptor changes queue intent, lifecycle
  systems perform structural mutation.
- Runtime modes are profile-driven: execution profiles decide which broad
  system groups and render build paths are awake while the scene remains
  loaded.

## Project Structure

```text
engine/
  core/                  pure ECS, input, scene, command, event, UI runtime, JSON
  modules/
    transform/           local/world transforms and hierarchy
    animation/           keyframe animation
    tween/               reusable tween/easing helpers
    narrative/           headless narrative/dialogue runtimes
    dialogue/            dialogue module surface kept in tree
    diagnostics/         debug draw and diagnostic bridges
    physics/             sphere-collider collision detection
    tools/               in-engine inspection/editing toolchain
    visualnovel/         VN stage/runtime and retained UI frontend
    rendering/           renderer contracts, ECS setup, runtime, Vulkan backend
  resources/             engine-owned fonts, models, textures
  shaders/               GLSL sources and checked-in SPIR-V
  docs/                  feature-owned engine documentation
```

`engine/external/` contains third-party dependencies. Their Markdown files are
vendored documentation and should not be rewritten as first-party engine docs.

## Feature Documentation Map

- [docs/json/architecture.md](docs/json/architecture.md): shared JSON syntax,
  value trees, schema ownership, error handling, and migration contracts.
- [docs/settings/architecture.md](docs/settings/architecture.md): subsystem-owned
  settings, immutable startup configuration, runtime requests, and effective state.
- [docs/ui/architecture.md](docs/ui/architecture.md): retained UI runtime.
- [docs/ui/authoring-guide.md](docs/ui/authoring-guide.md): practical UI
  authoring rules.
- [docs/tooling/architecture.md](docs/tooling/architecture.md): engine tooling
  shell, panes, commands, viewports, and guardrails.
- [docs/tooling/authoring-guide.md](docs/tooling/authoring-guide.md): adding
  panes, properties, and visual QA.
- [docs/tooling/presets.md](docs/tooling/presets.md): launch presets and
  screenshot automation.
- [docs/rendering/architecture.md](docs/rendering/architecture.md): rendering,
  materials, lighting, particles, frame extraction, screenshots.
- [docs/rendering/authoring-guide.md](docs/rendering/authoring-guide.md):
  rendering authoring rules.
- [docs/rendering/roadmap.md](docs/rendering/roadmap.md): future rendering work
  only.
- [docs/rendering/benchmarks.md](docs/rendering/benchmarks.md): deterministic
  rendering benchmark suite, JSON results, counters, baselines, and CI
  regression checks.
- [docs/physics/architecture.md](docs/physics/architecture.md): physics module
  current state.
- [docs/physics/authoring-guide.md](docs/physics/authoring-guide.md): physics
  authoring rules.

## Dependency Rules

- `engine/core/` must not include headers from `engine/modules/`.
- Base feature modules should depend on `gravitas_core` and only the modules
  they explicitly integrate with.
- Rendering module code may define renderer-facing ECS descriptors and
  extraction contracts, but Vulkan-specific types stay under
  `modules/rendering/backend/vulkan/`.
- Transform and hierarchy semantics belong to `modules/transform/`. Rendering
  consumes `WorldTransformComponent`; it must not compute parent-child world
  matrices or mutate scene transforms.
- Base physics must not depend on rendering. Physics visualization belongs in
  diagnostics bridge modules such as `diagnostics/physics/`.
- Tools and debug bridges may depend on multiple modules because integration is
  their purpose.
- Public module headers should stay small and stable. Heavy implementation
  dependencies belong in `.cpp` files or backend-private headers.

## ECS Model

`ECSWorld` owns entities, archetype-based component storage, singleton
components, command buffers, event subscriptions, simulation systems,
controller systems, and the active execution-profile stack.

Hot-path queries use `forEach<C...>(fn)`. Mutation-safe traversal uses
`forEachSnapshot<C...>(fn)`. Structural changes from systems should go through
`world.commands()` and be flushed at controlled points.

System types:

- `ECSSimulationSystem`: fixed-timestep update, deterministic simulation,
  physics, animation, and domain logic.
- `ECSControllerSystem`: once per rendered frame, presentation, UI, tooling,
  renderer lifecycle, input sampling, and engine command requests.

Controller context pointers are valid only for the current update call. Do not
cache them across frames.

## Execution Profiles

Every registered system has a broad `EcsSystemGroup`:

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

`SceneExecutionProfile` combines enabled groups, render frame build mode, and
time policy. Built-in profiles include gameplay, dialogue overlay, fullscreen
dialogue, and pause menu.

`TimePolicy` is part of the profile contract, but separate gameplay, physics,
UI, dialogue, and real-time clocks are not fully implemented yet. Today, time
effectively stops for masked system groups.

## Input Model

`GtsPlatform` owns raw input and the binding registry, bridges platform events,
and updates action states each frame. It does not choose default controls.
`GravitasEngine` installs its own controls during startup. Installing an engine
module invokes its `registerInputBindings` hook. `RenderingRuntime` installs UI
and default camera bindings; `EngineToolRuntime` installs tool bindings only when
tooling is enabled. There is no central enumeration of individual bindings:

- `input/EngineControlBindings.hpp`: engine lifecycle and diagnostic controls.
- `core/ui/input/UiDefaultBindings.hpp`: retained UI controls.
- `modules/rendering/ecssetup/camera/input/CameraDefaultBindings.hpp`: default camera controls.
- `modules/tools/input/ToolDefaultBindings.hpp`: tooling and editor camera controls.

Registration order does not determine action routing. Active contexts are
processed newest-first, followed by the global context. Each context evaluates
matching bindings together. Passthrough bindings observe input without reserving
it; non-passthrough bindings reserve it against lower contexts, not siblings.
Distinct exclusive actions claiming the same physical input in one context are
suppressed and reported by `getRoutingConflicts()` for the latest update. The
conflicted input remains reserved against lower contexts; shared observers still
run. Multiple bindings for the same action do not conflict with one another.
Paused gameplay bindings neither evaluate nor reserve input.

Reservations cover matching held/pressed/released physical input, independently
of whether the binding's activation mode fires that frame. They express input
ownership, not successful handling by a UI or gameplay consumer. UI-first
handled-event fallback and modal capture are not implemented by this registry;
engine commands still run before UI dispatch. Legacy `checkConflict()` remains
a first-match editing query, not the runtime routing diagnostic.

Feature installers use `bindDefaults`, which adds defaults only for actions
without existing bindings. All alternatives for a new action are installed
together; existing rebindings are preserved. Explicitly unbound actions are not
tracked as overrides by this API. Applications continue to register their own
gameplay bindings and apply saved settings after engine startup.

Module input registration runs once per engine instance, not per scene or editor
visibility change. Bindings remain in the platform-owned registry for that
engine's lifetime; scene transitions clear contexts without reinstalling defaults.
Modules are unregistered before their runtime objects are destroyed. Runtime
module unloading/reloading and binding ownership tokens are not supported yet.

Raw input flows through `IInputSource` into `InputBindingRegistry`, then systems
poll semantic action strings through their ECS contexts.

Pressed/released input has two timing domains:

- `isPressed()` and `isReleased()` are frame-local edges for controller systems,
  UI, tools, and engine commands.
- `isSimulationPressed()` and `isSimulationReleased()` are latched until the
  next fixed simulation tick consumes them.

Fixed-step simulation systems must never depend on frame-local input edges.

## Scene Lifetime

`SceneManager` stores a registered scene catalog and factory functions. It does
not keep inactive scene instances alive.

`GravitasEngine` owns one active `GtsScene` at a time. Scene changes wait for
graphics idle, unload and clear the old scene, destroy its instance, construct
the next scene from its factory, and call `onLoad(...)`.

Scene ECS worlds, physics worlds, retained scene UI, camera view IDs, render
object slots, and procedural mesh resources are scene-runtime state. Long-lived
application state that should survive a scene change belongs outside the scene
instance and is passed into scene factories by the application.

## Event Buses

The engine has two event buses:

- `GtsPlatformEventBus`: platform/infrastructure events from OS/GPU callbacks;
  dispatched once per frame on the main thread.
- `ECSWorld` domain events: immediate single-threaded dispatch from systems
  through `ctx.world.publish(...)`.

Rule of thumb: hardware, driver, or GPU callback events use the platform bus.
Domain logic events during ECS updates use `ECSWorld`.

## Commands And Screenshots

Engine commands travel through `GtsCommandBuffer`. Scene/controller code may
request scene changes, graphics settings application, pause/resume, and
screenshots.

Screenshot requests use one renderer-owned path:

```text
engine.screenshot action or GtsCommandBuffer::requestScreenshot
  -> IGtsGraphicsModule::requestScreenshot
  -> ForwardRenderer::requestScreenshot
  -> ScreenshotManager::saveImage
  -> async PNG write job
```

Tooling preset screenshot automation is documented in
[docs/tooling/presets.md](docs/tooling/presets.md). Agents doing visual work
should run a deterministic preset, then inspect the generated PNGs directly.

Queued screenshot requests are consumed in the pre-render command pass, before
the current frame is drawn. Scene changes and quit remain post-render commands,
so a screenshot requested alongside quit still captures the final frame.

## Resource Model

JSON syntax belongs to `core/json/GtsJsonParser`, backed by `GtsJsonValue`.
Feature loaders own file access, schema validation, defaults, and typed-data
conversion. Do not add module-local JSON parsers or escaping/writer logic.
Keep parser dependencies out of data-only headers; see the
[JSON architecture](docs/json/architecture.md) for the contract and limits.

Assets are accessed through `IResourceProvider`. Binding/lifecycle systems load
and upload meshes, textures, fonts, shaders, and engine assets as needed.

GPU resource handles are engine-managed runtime values. Application code should
not create, remove, or read GPU companion components directly.

Renderer ownership rule: material color belongs to material GPU state. Object
GPU state contains placement and per-object presentation data only. The current
scene object buffer stores the model matrix and UV transform; shared material
base color is synchronized through `MaterialRuntime`, `MaterialFrameData`, and
the scene material binding path rather than being copied into every object.
Material versions are authoritative; material queues only schedule work, and
the scene-local material user index limits invalidation to entities that
actually reference a changed material.

Dynamic mesh geometry is also version-authoritative. `DynamicMeshComponent`
owns authored CPU geometry, `markDynamicMeshChanged(...)` increments
`geometryVersion` and schedules work, and `DynamicMeshBindingSystem` processes
only queued changed versions. `MeshGpuComponent` stores the uploaded version,
last attempted version, used byte counts, and GPU allocation capacities. Unchanged
dynamic meshes do no geometry preparation or upload work; capacity-stable
updates reuse the existing procedural mesh allocation. Detailed dynamic mesh
lifetime and failure behavior is documented in
[docs/rendering/architecture.md](docs/rendering/architecture.md).

Render transform synchronization follows the same version-plus-queue model.
`TransformSystem` owns `WorldTransformComponent` publication, the renderer-owned
bridge schedules affected renderables, and `RenderGpuSystem` validates
`WorldTransformComponent::version` against
`RenderGpuComponent::uploadedWorldTransformVersion` before copying model
matrices or requesting object uploads. Static steady state performs no full
renderable scan and emits no transform-driven object uploads.

The transform, render-sync, and snapshot hot paths are now single-threaded
batch pipelines: they collect deterministic work records, partition contiguous
ranges, process without structural ECS mutation, and publish at explicit
barriers. No worker pool or render thread exists yet; the batch path is the
reference implementation a future executor should run.

Rendering performance work should use the benchmark suite documented in
[docs/rendering/benchmarks.md](docs/rendering/benchmarks.md). Smoke benchmarks
run deterministic ECS/extraction workloads in ordinary CI and emit JSON
timings, counters, environment metadata, and invariant failures. GPU runtime
benchmarks run the normal headless Vulkan engine path with backend-owned
timestamp query pools and report real GPU frame, scene, particle, and UI
timings when supported. CPU queue-submit time must not be treated as GPU frame
time.

Runtime graphics changes are engine-owned and travel through engine-facing
types. Applications request changes through
`gts::rendering::requestApplyGraphicsSettings(...)`; the graphics module and
main loop apply window, swapchain, resolution policy, presentation, and frame
pacing changes. `EngineConfig` is an immutable startup snapshot. Runtime
preferences and effective graphics state are queried separately; see the
settings architecture for ownership and extension rules.

## Extensibility Pointers

- Add simulation systems by deriving `ECSSimulationSystem` and registering them
  with the broadest accurate `EcsSystemGroup`.
- Add controller systems by deriving `ECSControllerSystem` and registering them
  with the broadest accurate group.
- Add renderables through descriptors; do not write GPU companions.
- Add UI through `UiComposition` and widgets. See
  [docs/ui/authoring-guide.md](docs/ui/authoring-guide.md).
- Add tool panes through pane descriptors, `ToolPane`, typed commands, and
  `EngineToolShellSystem`. See
  [docs/tooling/authoring-guide.md](docs/tooling/authoring-guide.md).
- Add physics participation through `PhysicsBodyComponent` and
  `SphereColliderComponent`. See
  [docs/physics/authoring-guide.md](docs/physics/authoring-guide.md).

## Documentation Policy

This file must stay an index and cross-cutting contract. Feature-specific
details belong in `engine/docs/<feature>/`. When engine behavior changes,
update the owning feature document in the same change.
