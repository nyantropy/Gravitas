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
    assets/              canonical model/skeleton domains, importers, static/skinned geometry processing
    transform/           local/world transforms and hierarchy
    animation/           transform animation ECS feature, CPU skeletal evaluation and playback occurrences
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

- [docs/assets/model-domain.md](docs/assets/model-domain.md): canonical CPU model
  assets, semantic vertex streams, validation, and the model-importer boundary.
- [docs/assets/model-runtime.md](docs/assets/model-runtime.md): engine-owned shared
  CPU model resources, capability-aware source/cooked requests, prepared/canonical
  definition views, stable references and scoped clip lookup.
- [docs/assets/import-bundle.md](docs/assets/import-bundle.md): associated canonical
  import products, definition ownership, enumeration and result validation.
- [docs/assets/skeleton-domain.md](docs/assets/skeleton-domain.md): reusable CPU
  evaluation hierarchies, default transforms, validation, and exact compatibility.
- [docs/assets/animation-clip-domain.md](docs/assets/animation-clip-domain.md): CPU
  skeletal clip data, typed TRS keys, cubic derivatives, and compatibility validation.
- [docs/animation/skeletal-evaluation.md](docs/animation/skeletal-evaluation.md):
  CPU track sampling, local poses, and parent-first reference-space evaluation.
- [docs/animation/skeletal-playback.md](docs/animation/skeletal-playback.md):
  explicit-delta playback and independent mutable skeleton occurrences.
- [docs/animation/skin-palette.md](docs/animation/skin-palette.md): CPU pose/binding
  validation and skin-local deformation matrix palettes.
- [docs/animation/transform-animation.md](docs/animation/transform-animation.md):
  procedural entity transforms, scene installation, and fixed-step playback.
- [docs/assets/skin-binding-domain.md](docs/assets/skin-binding-domain.md): CPU
  skin-local remaps, inverse binds, and exact target-skeleton validation.
- [docs/assets/obj-importer.md](docs/assets/obj-importer.md): standalone canonical
  OBJ strategy, source interpretation, diagnostics, and canonical consumer integration.
- [docs/assets/gltf-importer.md](docs/assets/gltf-importer.md): canonical glTF/GLB
  strategy, shared source decoding, scene policy, and explicit unsupported features.
- [docs/assets/static-geometry.md](docs/assets/static-geometry.md): source-neutral
  static geometry preparation, primitive ranges, defaults, generation, and metadata.
- [docs/assets/skinned-geometry.md](docs/assets/skinned-geometry.md): CPU four-influence
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

`modules/assets/` groups the canonical domain, importer contracts/strategies, and
source-neutral processing. `gravitas_assets` owns the CPU domain and depends on
core and the CPU skin/skeleton/animation domains; parser and static-profile processing dependencies stay in separate targets
within the same module. Core has no dependency on the assets module.

`modules/assets/skeleton/` owns the separate CPU-only `gravitas_skeleton_assets`
target. `GtsSkeletonAsset` stores stable local node IDs, display names,
parent-before-child forests, and quaternion-TRS or exact affine default local
transforms. Helpers may be evaluation nodes. Validation rejects malformed data
without repair; exact compatibility compares ordered IDs, parents, and stored
transforms, ignoring display names. `GtsSkeletonCompatibility` captures those
fields in a self-contained value. Asset and descriptor validation share one
implementation, and all compatibility APIs use one exact descriptor comparison.
Compatibility is neither asset identity nor a runtime pose association.
This is immutable asset data, not a runtime
pose or mesh skin binding. Model-local skeleton uses now reference definitions;
canonical glTF skin import now produces definitions/bindings and model occurrences;
no skinned cooking/runtime integration exists.

`modules/assets/skin/` owns `GtsSkinBinding` in the CPU-only `gravitas_skin_assets`
target, depending on skeleton assets. Each local slot pairs a skeleton-node
index with an authored inverse bind. A self-contained `GtsSkeletonCompatibility`
value retains the expected exact structural contract without owning a skeleton. Structural and skeleton-context
validation are separate; duplicate mappings and singular affine inverse binds
are allowed. The skin target has no model dependency or runtime pose.

`modules/assets/animation/` owns the independent CPU-only `gravitas_animation_assets`
target, depending only on skeleton assets and core math. `GtsAnimationClipAsset`
stores an exact compatibility expectation, duration, and per-property TRS tracks
with Step/Linear/CubicSpline keys. Rotation values are quaternions; cubic rotation
tangents are XYZW component derivatives. Structural/contextual validation rejects
malformed timing, values, duplicate targets, and matrix-node TRS animation without
repair. The clip owns no skeleton definition or runtime state. Canonical glTF now
imports skeletal TRS clips and enumerates them as bundle values.

`modules/animation/skeletal/` owns the separate CPU `gravitas_skeletal_animation`
target, consuming animation/skeleton assets without model, skin, ECS or rendering
dependencies. `evaluateGtsDefaultPose` and `evaluateGtsAnimationPose` produce
`GtsSkeletonPose` local TRS/matrix values, skeleton-reference-space matrices and
a self-contained exact skeleton compatibility contract for downstream validation.
Explicit clip-domain times use STEP, LINEAR vectors/shortest-path SLERP, or
time-scaled Hermite (XYZW then normalization for cubic rotation). Missing tracks
retain defaults; exact matrix nodes stay fixed. Parent-first composition uses
`parent * local` with `T * R * S`. Validation/arithmetic failures expose no pose.
This pose target implements no skin matrices, world placement or playback controller.

`modules/animation/skinning/` owns the separate CPU `gravitas_skin_palette` target,
linking skeletal pose evaluation and skin assets. `evaluateGtsSkinPalette` validates
pose and binding against a supplied skeleton, then emits one matrix per skin-local
slot: `pose.modelTransforms[mappedNode] * inverseBind`. One pose can feed multiple
mesh-specific bindings without reevaluation. Finite checks reject overflow without
partial results; singular affine products are valid. There are no model/mesh,
world-placement, ECS, renderer, Vulkan or playback dependencies in this layer.

`GtsModelSkeletonUse` shares an immutable skeleton definition; each table entry
is a distinct occurrence. `GtsModelSkinBinding` pairs a binding value with a
skeleton-use index. Nodes optionally select mesh + binding. Model validation
checks these references, delegates exact compatibility to the skin domain, and
checks optional evaluation-node/model-node correspondence, paired influence
sets, local-slot bounds and nonnegative unit-sum weights
across all sets (absolute tolerance `1e-4`). Unbound streams retain generic
geometry validation. See [model associations](docs/assets/model-domain.md#skeleton-uses-and-skin-associations).

`modules/assets/model/` owns the source-format-independent, renderer-independent
`GtsModelAsset` domain. `modules/assets/importer/` owns the `IGtsModelImporter`
contract and concrete strategies. File-backed model importers return validated
bundles through `GtsModelImportResult`. `GtsModelImportBundle` contains an optional
primary model, shared immutable skeleton definitions, and animation clip values. Model occurrences share
those same objects; duplicate definition entries and unlisted uses are rejected.
Bundle validation composes model/skeleton validation and checks ownership, not
structural identity. Clip validation separately requires at least one listed exact
compatible skeleton; multiple matches are valid and select no occurrence. Static importers retain model-only success calls. A successful
model-less bundle is possible, so `asset() == nullptr` alone no longer means failure.
Runtime realization remains a separate responsibility. `modules/assets/importer/obj/` provides the first
strategy, `GtsObjModelImporter`. It is the only OBJ interpreter. Offline cooking
and permitted development runtime loading both consume its canonical model through
static preparation. `modules/assets/realization/` adapts flat identity-root models
to the existing single-mesh storage/resource contract. `modules/assets/runtime/`
owns CPU mesh loading and cooked/source selection, with the existing strict policy.
The renderer links the importer and preparation targets through these consumers;
TinyOBJ remains private to the importer. glTF/GLB still uses `GltfAssetImporter`
and legacy import DTOs in the cooker. `modules/assets/importer/gltf/` now also
provides the independent canonical `GtsGltfModelImporter`, with no consumer cutover.
It now imports actual skins before scene pruning, retaining required transform
ancestors, source TRS/matrix forms and skin-local slot order. Clear shared-rig
evidence groups skins into definitions/occurrences; inverse binds stay binding-specific.
Skeletal TRS animation channels resolve by intersecting original source-node rig
memberships, then target the resolved compatibility contract. Shared-helper
ambiguity, multi-rig, ordinary-node and morph animation remain explicit failures;
STEP/LINEAR/CUBICSPLINE preserve typed keys and derivatives. See
[glTF skin policies](docs/assets/gltf-importer.md#skins-definitions-and-occurrences).
Its CPU source utilities and stricter GLB framing are shared with the legacy
importer. Cooked v1 serializers and GPU upload are unchanged.
See [OBJ consumer migration](docs/assets/obj-consumers.md) for adaptation limits.
`modules/assets/processing/geometry/static/` consumes canonical meshes for the current
static rendering profile. Its `GtsStaticVertex.h` defines the concrete static layout;
`GtsVertexAttribute` remains canonical semantic data above the format wall.
It prepares CPU `GtsStaticVertex` buffers and primitive ranges
without modifying canonical inputs. This standalone target reuses CPU geometry
algorithms and has no importer, cooker, runtime, or backend dependency.
The separate `geometry/skinned/` profile prepares `GtsSkinnedVertex` geometry with
four effective influences, keeping skin-local slots. It validates mesh/binding
context, selects the strongest four across all sets with deterministic ties, and
renormalizes prepared weights with explicit reduction metadata/warnings. Both
profiles share primitive attribute conversion and existing CPU geometry algorithms.
Shared flags/metadata live in `geometry/GtsGeometryMetadata.h`; the static vertex
has no renderer ownership or dependency. Static/skinned layouts are explicit,
independent structs. The Vulkan backend has explicit static/skinned vertex
descriptions and a separate skinned mesh/palette upload and shader path. Existing
world extraction, MeshResource and DynamicMeshComponent remain static-only.
Skinned backend resources accept externally supplied palettes; they do not sample
clips, apply inverse binds or orchestrate animation.
Neither profile computes poses, skin matrices, world transforms or animated bounds.
Model materials describe CPU appearance and reference model-local image inputs
with explicit UV sets and scalar channels. Shader policy, cooked texture identity,
and runtime material state remain outside the canonical domain.

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
