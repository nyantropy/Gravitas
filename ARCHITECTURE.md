# Gravitas Engine Architecture

Gravitas is a C++20 engine with fixed-step ECS simulation, per-frame controllers,
retained UI and a Vulkan renderer. This page is the entrypoint for agents: read
these cross-module contracts, then the owning feature page before changing behavior.

Paths on this page are relative to the **engine repository root**. In a
DungeonCrawler checkout that root is `engine/`, so production source paths such
as `engine/core/` refer to `engine/engine/core/` from the game root.

## Source and dependency ownership

| Directory | Owns |
| --- | --- |
| `engine/core/` | ECS, scene/resource lifetime, commands/events, input mechanisms, JSON, time, math, paths and tween values |
| `engine/modules/` | Transform, animation, model/assets, rendering, physics, UI, narrative, diagnostics and tools |
| `engine/runtime/` | `GravitasEngine`, startup configuration, platform/loop, engine controls and coordinated execution policy |
| `applications/` | Validation scenes and explicit benchmark applications |
| `tests/`, `cmake/` | Regression tests and build/dependency enforcement |
| `resources/`, `shaders/` | Engine assets, GLSL and checked-in SPIR-V |
| `external/` | Third-party dependencies; their docs remain vendor-owned |

Dependencies point **runtime → modules/core**, and **modules → core or explicit
module contracts**. Core must not include/link modules or runtime; modules must
not include/link runtime. A capability-local `runtime/` folder stays in its module.
Leaf targets own sources and exported include directories. Do not export a parent
source root or link an aggregate merely to obtain a header. CMake checks target
links, source/includes, object-library references and public include surfaces.
See [module ownership](docs/modules/ownership.md).

The complete `gravitas_runtime` / `gravitas_engine` / `GravitasEngine` facade
requires rendering, Vulkan, physics, debug drawing and tools. Reduced configurations
build available leaf capabilities and tests; they do not provide a partial facade.
Runtime tool visibility is independent of compile-time availability. See
[runtime configuration](docs/modules/runtime-configuration.md).

Prefer direct code and explicit ownership. Keep data contracts small and substantial
implementation in neighboring source files. Follow [CODING_STYLE.md](CODING_STYLE.md).

## ECS, time and execution

`ECSWorld` owns entities, archetype components, singleton state, structural commands,
event subscriptions, simulation/controller lists and the execution-selection stack.

- `ECSSimulationSystem` uses fixed-step `EcsSimulationContext::dt` for authoritative
  state, physics and simulation animation.
- `ECSControllerSystem` runs per rendered frame for input, presentation, UI, tooling,
  rendering lifecycle and engine commands. Controller `unscaledDeltaTime` is the
  supplied frame delta; `deltaTime` currently contains the fixed simulation step.
- Use `forEachNonstructural<C...>` for scoped row traversal without immediate
  structural mutation. `forEach<C...>` / `query<C...>().each` and
  `forEachSnapshot<C...>` retain entity-ID snapshot traversal for mutation-compatible
  callers. Queue structural work through `world.commands()`; commands flush after
  each executed system, never on nonstructural traversal exit. See the
  [traversal contract](docs/core/architecture.md#nonstructural-ecs-traversal).
- Systems execute in registration order. Groups **filter**, never sort or create
  phases. The selection is checked before each system, so a profile change can
  affect later systems in the same pass. `Always` is maskable too.

Core owns opaque `EcsSystemGroup` values and `EcsExecutionSelection`.
`engine/runtime/execution/` owns the named catalog, `SceneExecutionProfile`,
pause/VN recipes and their rendering policy. Module installers require supplied
selections/groups; they never construct Gravitas policy. The first configured
default wins, overlays remain intact, and clear restores that default.

Engine pause stops simulation ticks while controllers/rendering continue.
`TimePolicy` is metadata, not a separate clock. Retained UI dispatch/animation
and external tool controllers have their own dispatch outside ECS filtering.
See [execution architecture](docs/execution/architecture.md) for masks, recipes
and installation inputs.

`EcsControllerContext` is foundational. Optional model/UI/physics/rendering
services and viewport metrics arrive through module-owned contracts in typed
`ControllerFrameData`. Copies own payload values but borrow services; neither
payload references nor service pointers may be cached beyond the call. Tool and
scene contexts can have different viewport data for the same world. See
[core architecture](docs/core/architecture.md).

## Input

Runtime owns raw event writing through core `InputWriter`. The platform bridges
input events and updates `InputBindingRegistry`; engine/module installers register
their own defaults. Applications register game actions and then apply saved bindings.
`bindDefaults` preserves actions with existing bindings; it does not remember an
explicitly unbound action as an override.

Active contexts route newest-first, then the global context. Passthrough bindings
observe without reserving input. Exclusive bindings reserve input against lower
contexts, not siblings; distinct exclusive sibling actions claiming the same input
are suppressed and reported by `getRoutingConflicts()`. Paused gameplay bindings
neither evaluate nor reserve input. This registry does not implement UI-handled
fallback or modal capture; engine commands run before UI dispatch.

Controllers use frame-local `isPressed()` / `isReleased()`. Simulation uses
`isSimulationPressed()` / `isSimulationReleased()`, latched until a fixed tick
consumes them. Do not drive fixed-step edge actions from frame-local edges.
Bindings are installed once per engine instance; scene changes clear contexts,
not the registry. See [binding routing](docs/core/architecture.md#binding-routing).

## Scenes and lifetime

`SceneManager` stores factories. `GravitasEngine.h` declares the runtime facade;
its paired `.cpp` owns implementation through a private smart owner. It validates
scene IDs before destructive teardown; unknown requests leave the old scene loaded.
`GravitasEngine` keeps only the active `GtsScene`
alive: a switch waits for graphics idle, unloads/clears the old scene, destroys it,
constructs the new scene and calls `onLoad`. Persistent application state belongs
outside scene instances and enters through factory captures/transition data.

A scene owns its ECS world and exact-type resources through `createSceneResource`,
`findSceneResource` and `requireSceneResource`. Duplicate creation and missing
required resources throw; find returns null. Reset destroys resources after world
clear. Direct scene destruction does not call unload, and raw world clear is not
a scene reset. Physics access belongs to `gts::physics::findScenePhysics` /
`requireScenePhysics`, not the foundational scene API.

World-keyed module caches enroll identity-only, non-throwing teardown callbacks.
They run after clear has removed entities/systems/subscriptions, or after other
world-owned members during destruction. Removal callbacks can still queue dirty
work; external caches must be released at that final boundary. Transform and
rendering implement this for preview/benchmark worlds as well as scenes. See
[scene resources](docs/core/architecture.md#scene-resources-and-optional-features),
[transform lifetime](docs/transform/architecture.md#world-lifetime-contract) and
[rendering lifetime](docs/rendering/architecture.md#world-state-lifetime).

## Events and engine commands

- Platform/infrastructure events use `GtsPlatformEventBus`, dispatched on the
  main thread each frame.
- ECS domain events use `world.publish(...)`, with immediate single-threaded
  subscriber dispatch before the call returns.
- `GtsCommandBuffer` carries lifecycle requests and generic typed extensions.
  Its application surface is enqueue-only; private core-owned consumption is
  linked only by engine consumers. Feature semantics belong to their module contracts.

Screenshots use `ScreenshotCommand.h` and
`gts::rendering::requestScreenshot(commands, directory)` from
`gravitas_rendering_command_contracts`; graphics settings use
`requestApplyGraphicsSettings`. Capture commands drain before rendering; scene
changes and quit drain afterward. A capture requested with quit can capture the
final frame. Preset automation uses the same renderer-owned PNG path. See
[tooling presets](docs/tooling/presets.md) and [settings](docs/settings/architecture.md).
The [command API](docs/core/commands-api.md), [scene API](docs/core/scenes-api.md)
and [runtime API](docs/core/runtime-api.md) define ownership and rejection behavior.

## Assets, models and rendering

```text
OBJ / glTF / GLB → canonical import bundle → static cooking → cooked-v1
                           └──────── source/cooked loading ─────┘
                                             ↓
                                     GtsModelResource
                                             ↓
                                     GtsRealizedModel
                                             +
                                  world material realization
                                             ↓
                                     GtsModelInstance
                                             +
                                  authoritative ECS transform
                                             ↓
                                  generic model extraction
                                             ↓
                                   static / skinned renderer
```

Applications use `GtsModels.h`: request a model, check diagnostics, create an
instance through `modelInstances`, and attach `ModelInstanceComponent`. Handles
retain immutable resources; geometry is shared. Instances own independent
playback, poses, palettes and optional material overrides. World material handles
are invalidated on runtime reset. Overrides select per-slot → model-wide → base
materials and must belong to the same live world scope.

Extraction captures frame-owned dynamic state and references immutable geometry.
Static placement is entity-world × model-node transform. Skinned placement applies
entity-world after palette deformation without reapplying the mesh-node transform.
Model hierarchy is shared definition data; entity placement stays in ECS.
Cooked-v1 is static-only and rejects skeletal/animated input. Direct/generated
mesh APIs remain valid for procedural meshes, text and debug geometry.
See [model architecture](docs/model/architecture.md).

Applications author descriptors; rendering owns GPU companions and Vulkan lifetime.
Transforms/hierarchy belong to the transform module. Install transform writers
before the resolver and consumers after it; physics also resolves before collision
queries. Render synchronization consumes published `WorldTransformComponent` versions.
Material color belongs to material state, not object uploads. Dynamic meshes and
transform/material synchronization use authoritative versions plus dirty queues.
Do not bypass these paths by mutating GPU companion components.

Transform, render-sync and snapshot batches currently execute single-threaded.
GPU caches and frame-safe resources belong to the backend. See
[rendering architecture](docs/rendering/architecture.md) for lifecycle order,
invalidation, material/lighting contracts, extraction and current renderer limits.

## UI, narrative and shared services

UI contexts/extraction acquire texture/font assets through the rendering-owned
`IUiResourceProvider`, separate from backend mesh/camera/object allocation.
Runtime clears retained compositions before unloading their borrowed scene data.
See [UI acquisition and lifetime](docs/rendering/ui-resource-api.md).

`gravitas_ui` owns retained UI and depends on core plus lightweight rendering
resource contracts. Rendering owns `UiSystem`, resource realization and extraction.
Author ordinary UI with compositions, widgets, layouts, themes and retained events;
use engine focus/navigation/modal/drag-drop ownership.

Headless dialogue and VN presentation are separate narrative targets. Game content,
conditions and story persistence remain application-owned. Physics provides sphere
collision detection, not a response solver; diagnostics bridges own visualization.

JSON syntax, checked values and serialization belong to `core/json/`. Feature
loaders own file access, schemas, defaults and diagnostics. Reuse `GtsJsonValue`
accessors; do not add local parsers or escaping helpers. Stable enum names belong
beside the enum in `EnumName` tables.

## Feature documentation

- [Core](docs/core/architecture.md), [JSON](docs/json/architecture.md),
  [tween](docs/core/tween.md), [execution](docs/execution/architecture.md).
- [Module ownership](docs/modules/ownership.md),
  [runtime configuration](docs/modules/runtime-configuration.md),
  [settings](docs/settings/architecture.md).
- [Transform](docs/transform/architecture.md),
  [transform animation](docs/animation/transform-animation.md),
  [skeletal evaluation](docs/animation/skeletal-evaluation.md),
  [playback](docs/animation/skeletal-playback.md), [palettes](docs/animation/skin-palette.md).
- [Model pipeline and phase map](docs/model/architecture.md),
  [shared assets](docs/assets/architecture.md),
  [authoring manifests](docs/assets/authoring-manifest-api.md),
  [cooked formats and CLI](docs/cooked-asset-pipeline.md).
- [Rendering](docs/rendering/architecture.md), [material API](docs/rendering/material-api.md),
  [camera API](docs/rendering/camera-api.md), [diagnostics](docs/rendering/diagnostics-api.md),
  [rendering authoring](docs/rendering/authoring-guide.md),
  [Vulkan skinning](docs/rendering/skinned-geometry.md).
- [UI](docs/ui/architecture.md), [UI authoring](docs/ui/authoring-guide.md),
  [narrative](docs/narrative/architecture.md).
- [Physics](docs/physics/architecture.md),
  [physics authoring](docs/physics/authoring-guide.md),
  [diagnostics](docs/diagnostics/architecture.md).
- [Tooling](docs/tooling/architecture.md), [tool authoring](docs/tooling/authoring-guide.md),
  [presets and captures](docs/tooling/presets.md).
- [CPU scaling benchmarks](docs/performance-benchmarks.md),
  [rendering benchmarks](docs/rendering/benchmarks.md).

See [README.md](README.md) for build/test commands and [docs/README.md](docs/README.md)
for navigation. Architecture pages specify implemented contracts and explicit current
limitations, not completed-task reports or speculative plans. Update the owning
feature document in the same change as behavior; keep this entrypoint focused on
cross-module rules.
