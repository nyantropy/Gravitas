# Gravitas Engine Architecture Index

Gravitas is a C++20 modular application engine built around a two-tier ECS
architecture. The engine separates fixed-step simulation from frame-facing
controller work and assembles optional feature modules such as rendering,
physics, tooling, narrative (dialogue and visual novel presentation), diagnostics, tweening,
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
    tween/               shared easing/interpolation and caller-owned value transitions
  modules/
    assets/              shared image, geometry, direct mesh, material and cooked-container infrastructure
    model/               model import/domain/processing/cooking/loading/realization/world/runtime/extraction
    transform/           local/world transforms and hierarchy
    animation/           transform animation ECS feature, CPU skeletal evaluation and playback occurrences
    narrative/           narrative feature ownership
      dialogue/          headless graphs/progression and ECS requests/events
      visualnovel/       VN stage/runtime, interaction and retained UI frontend
    diagnostics/         debug draw and diagnostic bridges
    physics/             sphere-collider collision detection
    tools/               in-engine inspection/editing toolchain
    rendering/           renderer contracts, ECS setup, runtime, Vulkan backend
  resources/             engine-owned fonts, models, textures
  shaders/               GLSL sources and checked-in SPIR-V
  docs/                  feature-owned engine documentation
```

`engine/external/` contains third-party dependencies. Their Markdown files are
vendored documentation and should not be rewritten as first-party engine docs.

## Feature Documentation Map

- [docs/transform/architecture.md](docs/transform/architecture.md): authored and
  resolved transforms, hierarchy, publication, target ownership and deferred lifecycle questions.
- [docs/core/tween.md](docs/core/tween.md): shared value transitions consumed by UI and VN.
- [docs/diagnostics/architecture.md](docs/diagnostics/architecture.md): generic debug drawing,
  physics visualization and tool-owned display policy.

- [docs/narrative/architecture.md](docs/narrative/architecture.md): headless dialogue,
  VN presentation, separate runtime ownership and optional target dependencies.

- [docs/model/model-extraction.md](docs/model/model-extraction.md): the authoritative
  resource → realization → instance → generic static/skinned frame extraction path.
  No persistent model presentation mirror; direct mesh consumers remain supported.

- [docs/model/runtime-instances.md](docs/model/runtime-instances.md): model instance ownership,
  world creation, independent skeletal occurrences, lifetime-safe pose references and material reset.

- [docs/model/architecture.md](docs/model/architecture.md): the complete canonical cooking/runtime model pipeline,
  module ownership, lifetime and dependency boundaries.
- [docs/assets/architecture.md](docs/assets/architecture.md): reusable non-model asset infrastructure.
- [docs/model/canonical-cooking.md](docs/model/canonical-cooking.md): one canonical
  OBJ/glTF interpretation feeding static cooked-v1 and runtime, with capability rejection.

- [docs/model/model-domain.md](docs/model/model-domain.md): canonical CPU model
  assets, semantic vertex streams, validation, and the model-importer boundary.
- [docs/model/model-runtime.md](docs/model/model-runtime.md): engine-owned shared
  CPU model resources, capability-aware source/cooked requests, prepared/canonical
  definition views, stable references and scoped clip lookup.
- [docs/model/model-realization.md](docs/model/model-realization.md): shared CPU static/skinned
  geometry realization, occurrence associations and prepared cooked reuse.
- [docs/model/model-material-realization.md](docs/model/model-material-realization.md):
  world-scoped model material handles, canonical images and cooked material references.
- [docs/model/import-bundle.md](docs/model/import-bundle.md): associated canonical
  import products, definition ownership, enumeration and result validation.
- [docs/model/skeleton-domain.md](docs/model/skeleton-domain.md): reusable CPU
  evaluation hierarchies, default transforms, validation, and exact compatibility.
- [docs/model/animation-clip-domain.md](docs/model/animation-clip-domain.md): CPU
  skeletal clip data, typed TRS keys, cubic derivatives, and compatibility validation.
- [docs/animation/skeletal-evaluation.md](docs/animation/skeletal-evaluation.md):
  CPU track sampling, local poses, and parent-first reference-space evaluation.
- [docs/animation/skeletal-playback.md](docs/animation/skeletal-playback.md):
  explicit-delta playback and independent mutable skeleton occurrences.
- [docs/animation/skin-palette.md](docs/animation/skin-palette.md): CPU pose/binding
  validation and skin-local deformation matrix palettes.
- [docs/animation/transform-animation.md](docs/animation/transform-animation.md):
  procedural entity transforms, scene installation, and fixed-step playback.
- [docs/model/skin-binding-domain.md](docs/model/skin-binding-domain.md): CPU
  skin-local remaps, inverse binds, and exact target-skeleton validation.
- [docs/model/obj-importer.md](docs/model/obj-importer.md): standalone canonical
  OBJ strategy, source interpretation, diagnostics, and canonical consumer integration.
- [docs/model/gltf-importer.md](docs/model/gltf-importer.md): canonical glTF/GLB
  strategy, shared source decoding, scene policy, and explicit unsupported features.
- [docs/model/static-geometry.md](docs/model/static-geometry.md): source-neutral
  static geometry preparation, primitive ranges, defaults, generation, and metadata.
- [docs/model/skinned-geometry.md](docs/model/skinned-geometry.md): CPU four-influence
  skinned profile, binding-context validation and deterministic influence reduction.
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
- [docs/rendering/skinned-geometry.md](docs/rendering/skinned-geometry.md): explicit
  Vulkan skinned vertex ABI, per-frame palette SSBOs and shader/resource path.
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
- `gravitas_transform` owns transform include directories and compiled implementation,
  and links only `gravitas_core`. Consumers link the target rather than exporting
  transform directories themselves. Shared tween primitives live in `core/tween/`.
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

Subsystems with external world-keyed state may register an identity-only,
non-throwing teardown callback. These run after world clearing or, on destruction,
after world-owned members are destroyed. Transform uses one such registration
to release dirty queues and publication callbacks together; ordinary callers
do not manage transform registries. See the [transform lifetime contract](docs/transform/architecture.md).

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

Source model interpretation has one canonical entry per format: OBJ uses
`GtsObjModelImporter`, and glTF/GLB uses `GtsGltfModelImporter`. Both return
`GtsModelImportBundle`. That domain is the fork between source-neutral cooking
and runtime source loading; there is no legacy cooker-specific model importer.

```text
canonical import → canonical model ──→ cooking → static cooked-v1
                         │                             │
                         └─────────── loading ─────────┘
                                        ↓
                                GtsModelResource
                                        ↓
                                GtsRealizedModel
                                        +
                           world realized materials
                                        ↓
                                GtsModelInstance
                                        +
                             authoritative ECS transform
                                        ↓
                           generic model render extraction
                                        ↓
                              static / skinned renderer
```

The registry owns immutable definitions; handles retain their lifetime. The
realization cache owns shared prepared geometry and occurrence associations.
Canonical meshes use static/skinned preparation per occurrence; cooked meshes
retain their prepared bytes without regeneration. World material services resolve
logical associations to `MaterialRuntime` handles and invalidate them on reset.
Instances own only mutable occurrence state: independent skeletal playback, poses
and binding palettes, plus optional material-handle overrides. Material lookup resolves
per-logical-slot override → model-wide fallback → shared base material. Overrides
retain weak lifetime tokens in the same world scope and never mutate shared materials.
Model hierarchy remains shared; world placement stays in ECS.

Rendering discovers generic model-instance components. It references immutable
geometry, resolves live materials and captures frame-owned dynamic state. Static
placement composes entity and model hierarchy transforms. Skinned placement applies
entity transform after palette deformation, without reapplying the mesh-node
transform. GPU caches own shared geometry and frame-safe palette resources. No
persistent model-presentation mirror or game-owned rendering bridge remains.

Assets and model runtime remain renderer-independent. Cooking owns persistence
and uses CPU processing/serialization; it never calls world/runtime realization.
Cooked-v1 remains static-only and rejects skeletal/animated inputs rather than
losing capabilities. Lower-level direct/generated mesh APIs remain supported.

See the [asset/model architecture](docs/assets/architecture.md) for module and
ownership boundaries, [canonical cooking](docs/model/canonical-cooking.md),
[model instances](docs/model/runtime-instances.md), and
[render extraction](docs/model/model-extraction.md) for their contracts.
Canonical skeleton, skin and animation data and CPU evaluation remain independent
layers documented in the feature map above.

JSON syntax belongs to `core/json/GtsJsonParser`, backed by `GtsJsonValue`.
Feature loaders own file access, schema validation, defaults, and typed-data
conversion. Checked primitive access and narrowing belong to `GtsJsonValue`;
loaders use its `find*`/`try*` methods and keep feature defaults and diagnostics
local. Do not add module-local JSON parsers, typed lookup wrappers, or
escaping/writer logic.
Stable enum text belongs beside its enum in `gts::EnumName` tables, using
JSON-independent lookups from `core/types/EnumName.h`.
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
