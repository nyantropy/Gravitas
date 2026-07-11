# Gravitas Engine — Architecture

## 1. Overview

Gravitas is a C++20 modular application engine built around a two-tier ECS
(Entity-Component-System) architecture. The engine separates simulation logic
from optional feature modules such as rendering, physics, tooling, and future
audio, and provides a fixed-timestep simulation loop decoupled from the render
frame rate.

**Core philosophy:**
- ECS-first: all application state lives in components, all behavior lives in systems
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
- Transform and hierarchy semantics belong to `modules/transform/`. Rendering
  consumes `WorldTransformComponent`; it must not compute parent-child world
  matrices or mutate scene transforms.
- Base physics must not depend on rendering. Physics visualization belongs in
  diagnostics bridge modules such as `diagnostics/physics/`, which can consume
  debug-draw primitives without making physics depend on rendering.
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
│   │   ├── transform/      # Local/world transforms and parent-child hierarchy
│   │   ├── animation/      # Keyframe animation
│   │   ├── tween/          # Reusable value tween/easing helpers
│   │   ├── narrative/      # Headless narrative runtimes such as dialogue graphs
│   │   ├── diagnostics/    # Debug/diagnostic visualization modules
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
- Deterministic; suitable for physics, animation, and domain logic
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

Do not introduce feature-specific groups such as domain AI, transaction logic,
or a particular effect unless a broad engine-level category stops being expressive
enough. Most application logic belongs in `Gameplay`; engine renderer binding
and GPU sync systems belong in `RenderPrep`; retained UI state adapters,
presenters, coordinators, and composition updates belong in `Ui`; global or
scene debug tooling belongs in `Tools`.

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
implementation. Today, simulation/physics time effectively stops because those
system groups are masked out. Future work should map these policies onto
separate simulation, physics, UI, dialogue, and real-time domains without changing
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
│    └─ TransformSystem (resolve world transforms)       │
│    └─ RenderGpuSystem (sync resolved transforms)       │
│    └─ Camera control / CameraGpuSystem / view IDs      │
│    └─ ParticleEmitterSystem (particle frame data)       │
│    └─ [user systems] (input handling, domain logic)    │
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
- **Mesh lifecycle** resolves static mesh / quad mesh / dynamic mesh / world-text mesh state and owns `MeshGpuComponent`
- **Material lifecycle** resolves material references, legacy material adapters, world-text atlas materials, and per-instance `MaterialGpuState`
- **Render-object lifecycle** decides readiness, owns `RenderGpuComponent` creation, and allocates object SSBO slots
- **Renderable cleanup** tears down stale GPU companions regardless of descriptor source
- **Camera lifecycle** owns `CameraGpuComponent` creation/removal independently of renderable lifecycle
- **Transform lifecycle** resolves `TransformComponent` + `HierarchyComponent`
  into versioned `WorldTransformComponent` data
- **Render invalidation** queues snapshot dirtiness explicitly so steady-state extraction does not scan every renderable

Mesh, material, and render-object lifecycle are intentionally independent.
Changing material parameters or textures must not recreate mesh state; changing
mesh geometry must not recreate material state; object-slot allocation is
centralized in the render-object lifecycle rather than repeated by every
descriptor path. Queued descriptor changes may allow a newly bound renderable
to participate in the same controller update because lifecycle systems run in
mesh, material, then render-object order with deferred command flushes between
systems.

### Material Runtime Architecture

Materials have stable identity independent of ECS entities. A renderable uses a
material; it does not own the material.

`MaterialRuntime` is the CPU-side owner for:

- `MaterialDefinition`: backend-independent material structure, currently
  `LegacyUnlit` or `StandardSurface`
- `MaterialInstance`: shared material parameter values, texture assignment,
  render state, vertex-color-only mode, and an authoritative `version`
- `MaterialGpuState`: a generic cached representation keyed by
  `MaterialInstanceHandle`, storing resolved texture IDs for each material
  role, packed material GPU parameters, feature flags, render state, uploaded
  version, variant key, and an opaque `MaterialGpuHandle`
- built-in fallback materials, including the default material and a
  vertex-color-only material, plus a default standard-surface material

Material handles are small typed values with `id + generation`; `0` is invalid.
They are backend-independent and never depend on ECS entity IDs. Destroyed or
invalid material instance handles resolve to the runtime default material, so
stale references cannot silently address reused material state.

Entities reference material instances through `MaterialReferenceComponent`.
Multiple entities may intentionally reference the same `MaterialInstanceHandle`.
When that shared instance changes, `MaterialBindingSystem` synchronizes one GPU
cache entry for the instance and marks every referencing renderable dirty.
Each GPU cache entry tracks `uploadedVersion`, so one consumer observing a
material update does not clear a shared dirty bit for other consumers.

`MaterialComponent` remains a bounded legacy authoring adapter. It is converted
by `MaterialBindingSystem` into a unique material instance tracked by
`LegacyMaterialRuntimeComponent`; after conversion, the `MaterialInstance` is
the runtime authority. Existing application code can keep authoring
`MaterialComponent` during migration, but new sharing-oriented code should
author or obtain a `MaterialInstanceHandle` and attach `MaterialReferenceComponent`.

World text follows the same rule. `WorldTextBindingSystem` owns text-to-mesh
generation and font resolution. `MaterialBindingSystem` creates or updates the
world-text material instance from the font atlas texture and text tint.

Shared material state currently includes texture assignment by role, base color
factor, metallic factor, perceptual roughness factor, normal scale, ambient
occlusion strength, emissive factor/strength, alpha mode, alpha cutoff,
double-sided state, depth-write state, legacy alpha/additive blend selection,
and vertex-color-only mode. Per-object state remains in `RenderGpuComponent`
and object uploads:
model matrix, UV animation transform, and future object-specific IDs or
overrides. Tint/base color is a shared material value. It is no longer part of
generic object upload data; the current Vulkan backend still writes the scene
shader's temporary object tint field from material frame state until base color
moves into real material descriptor data.

`LegacyUnlit` and `StandardSurface` are explicit shader families. `LegacyUnlit`
samples texture/base color without scene lighting and is the correct default for
world text, debug draw, particles, tool visuals, and UI-like world geometry.
`StandardSurface` is the first lit scene-material family. In Phase 3A it uses a
temporary ambient + Lambert diffuse + Blinn-Phong-style specular model. In
Phase 3B it is a metallic-roughness material family using Cook-Torrance direct
lighting with GGX distribution, Smith geometry, and Schlick Fresnel. Its first
production parameter set is:

```
baseColorFactor           : vec4
metallicFactor            : [0, 1]
roughnessFactor           : [0.04, 1] perceptual roughness
normalScale               : finite float
ambientOcclusionStrength  : [0, 1]
emissiveFactor            : nonnegative vec3
emissiveStrength          : nonnegative float
```

`specularStrength` and `shininess` are removed from the authoritative material
model. `roughness` is perceptual; shaders derive microfacet alpha as
`roughness * roughness`. A dielectric baseline reflectance of `F0 = 0.04` is
mixed toward `baseColor.rgb` as metallic approaches `1`.

`StandardSurface` supports explicit texture roles:

- `BaseColor`: sRGB color texture multiplied by `baseColorFactor` and vertex
  color
- `MetallicRoughness`: linear/data texture using glTF-compatible channel
  packing: `R = ambient occlusion`, `G = roughness`, `B = metallic`
- `Normal`: linear/data tangent-space normal map
- `AmbientOcclusion`: linear/data AO texture sampled from red; when present it
  overrides the metallic-roughness texture's `R` AO channel
- `Emissive`: sRGB color texture multiplied by emissive factor/strength

Missing maps are represented by backend fallback textures so the descriptor
layout is stable: white base color, neutral metallic-roughness
(`AO = 1`, `roughness = 1`, `metallic = 0`), flat normal
(`0.5, 0.5, 1`), white AO, and black emissive. Material texture assignments are
part of shared material state; render commands and object uploads never carry
texture IDs or texture-role details.

Material feature flags describe map presence. The current variant key includes
normal-map presence because it changes the required surface frame. Other map
presence bits are carried as material feature flags and handled through the
stable material descriptor layout with fallback textures. Scalar factors and
texture handles themselves are not variant-key data.

Alpha semantics use `MaterialAlphaMode`:

- `Opaque`: opaque sorting/depth-writing material
- `Mask`: architecturally reserved for alpha cutoff; shader discard support is
  a later rendering phase
- `Blend`: transparent/blended material

The legacy `MaterialBlendMode` is retained only as a Phase 2C compatibility
field inside material runtime/backend compatibility code so the current Vulkan
scene stage can still select alpha/additive pipelines. It is not an
authoritative render-command field.

### Scene Lighting

Lights are scene data. Applications author light descriptors on entities, while
spatial meaning is resolved from `WorldTransformComponent` during extraction.
The renderer receives immutable frame-level light arrays and never searches ECS
for lights during backend submission.

Supported scene light descriptors are:

- `DirectionalLightComponent`: direction-only light. Local `-Z` is the
  direction light rays travel; shaders receive the opposite world-space vector
  as `directionToLight`. The legacy `active` flag remains a compatibility gate
  from the original single-light path, but multiple active directional lights
  may now be selected by priority.
- `PointLightComponent`: local radiance from the entity world position, with
  authored `color`, `intensity`, `range`, `enabled`, and `priority`.
- `SpotLightComponent`: local radiance from the entity world position, pointing
  along local `-Z`, with authored `range`, `innerConeAngle`, and
  `outerConeAngle` in radians.
- `EnvironmentLightComponent`: scene-level image-based lighting descriptor
  with an environment texture path, intensity, world-up rotation, enabled flag,
  and priority. It does not own spatial transform semantics or backend
  resources.

Light extraction publishes bounded backend-independent frame data:

```
LightingFrameData {
    DirectionalLightFrameData directional[2]
    PointLightFrameData       point[32]
    SpotLightFrameData        spot[16]
    uint32_t                  directionalCount
    uint32_t                  pointCount
    uint32_t                  spotCount
    EnvironmentFrameData      environment
    float                     ambientIntensity
    LightingDiagnostics       diagnostics
}
```

Selection is deterministic. Directional candidates are enabled and active, then
ranked by `priority` descending and stable entity ID ascending. Point and spot
lights are enabled, then ranked by `priority` descending, distance to the active
camera ascending, and stable entity ID ascending. Excess lights are dropped
after the fixed capacities above, with diagnostics counting total, selected,
dropped, and sanitized descriptors. Disabled lights are excluded. If no
directional light is selected, direct-light arrays are empty. Environment
descriptors are enabled candidates ranked by `priority` descending and stable
entity ID ascending; the first selected environment becomes the frame-level IBL
state. Multiple environments are not blended yet. If no valid environment is
selected, the backend resolves deterministic neutral/black fallback
environment resources.

Authored values are sanitized during extraction: negative or non-finite
intensity becomes zero, range is clamped to at least `0.01`, light colors clamp
to nonnegative finite values, and spot cones are clamped to valid ordered
angles before cosine thresholds are stored. Spot cone cosines are computed once
on the CPU, not per fragment. Environment intensity clamps to nonnegative finite
values and rotation falls back to zero when authored as non-finite.

Point and spot lights use finite-range inverse-square attenuation with a smooth
cutoff:

```
distanceSquared     = max(distance * distance, 0.01)
normalizedDistance  = saturate(distance / range)
cutoff              = saturate(1 - normalizedDistance^4)^2
attenuation         = cutoff / distanceSquared
```

Spot lights multiply this distance attenuation by cone attenuation:

```
coneCos = dot(directionFromLightToFragment, spotForward)
cone    = smoothstep(outerConeCos, innerConeCos, coneCos)
```

The Vulkan backend stores the generic lighting frame in the existing
camera/frame UBO beside view/projection and camera world position. The current
payload comfortably fits in a UBO, so Phase 3C keeps one frame-level lighting
binding instead of introducing an SSBO or per-light allocations. Light state is
not duplicated per material, per object, or per render command. Future
clustered/tiled lighting should replace selection/upload strategy without
changing material ownership or the direct BRDF.

Environment lighting is also frame-level state. `EnvironmentFrameData` carries
only backend-independent identity and scalar controls:

```
EnvironmentFrameData {
    std::string environmentPath
    float       intensity
    float       rotationRadians
    bool        enabled
}
```

The Vulkan renderer resolves that state through render resource management into
one shared environment descriptor set containing irradiance, prefiltered
specular, and BRDF-LUT bindings. Render commands, object uploads, and material
instances do not carry environment texture IDs. Intensity and rotation changes
update frame state only and do not mutate material versions or material variant
keys.

Phase 3E establishes this ownership and shader integration with deterministic
linear equirectangular environment textures and fallback images. The current
backend does not yet generate true HDR cubemaps, irradiance convolutions, or
GGX-prefiltered cubemap mip chains; that preprocessing is the next backend
resource refinement before production-quality IBL. The descriptor contract is
already shaped so those resources can replace the current texture realization
without changing material, command, or object ownership.

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
`CameraUploadCommand` together with camera world position and extracted
`LightingFrameData`, and `ForwardRenderer` writes the current frame's
camera UBO only after waiting for that frame's fence. This keeps camera/light
frame data in sync with uncapped rendering without racing frames in flight.
Engine consumers that need final camera matrices outside the renderer should
read the read-only `ActiveCameraViewStateComponent` singleton instead of
touching GPU companion components directly.

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

Local transform and hierarchy descriptors live in the transform module. Render
and particle descriptors live with the renderer ECS setup, next to their
runtime companion components and systems.

| Component | Purpose |
|-----------|---------|
| `TransformComponent` | Local position, rotation, scale, and local version |
| `HierarchyComponent` | Parent-child transform relationship |
| `StaticMeshComponent` | Asset path to mesh |
| `QuadMeshComponent` | Width/height for shared generated quad meshes |
| `DynamicMeshComponent` | Runtime-authored vertices/indices plus `geometryVersion` and authored vertex-attribute flags for uploaded mesh updates |
| `MaterialComponent` | Legacy texture path, tint color/opacity, culling, and optional vertex-color-only rendering; converted into a material instance |
| `MaterialReferenceComponent` | Stable `MaterialInstanceHandle` used by renderables that share or explicitly reference materials |
| `TextureAnimationComponent` | Optional per-object scene-material UV scrolling or flipbook atlas animation |
| `WorldTextComponent` | World-space text, font asset path, scale, and tint |
| `BoundsComponent` | Local AABB used for frustum culling |
| `CameraDescriptionComponent` | FOV, near/far clip planes |
| `DirectionalLightComponent` | Directional light authoring data; direction comes from resolved world rotation |
| `PointLightComponent` | Point light authoring data; position comes from resolved world transform |
| `SpotLightComponent` | Spot light authoring data; position and cone direction come from resolved world transform |
| `PhysicsBodyComponent` | Dynamic vs static body flag |
| `ParticleEmitterComponent` | Application-facing particle emitter descriptor paired with `TransformComponent` |

Renderable scene geometry should use one geometry descriptor at a time:
`StaticMeshComponent`, `QuadMeshComponent`, `DynamicMeshComponent`, or
`WorldTextComponent`.

Descriptor direction: descriptors are supposed to stay as clean authoring data.
They describe what the scene wants, not how the engine currently fulfills it.
GPU handles, cached paths, dirty flags, temporary upload state, generated
runtime data, and backend bookkeeping belong in engine-owned companion
components or feature runtime components. When a descriptor starts needing that
kind of state, split the state out instead of growing the descriptor into a
mixed domain/rendering object.

### Geometry Surface Contract

Renderable scene geometry uses one backend-independent `Vertex` contract:

```
Vertex {
    glm::vec3 pos
    glm::vec3 normal
    glm::vec4 tangent   // xyz = tangent direction, w = bitangent handedness
    glm::vec4 color
    glm::vec2 texCoord
}
```

Geometry defines the surface frame; materials define how light interacts with
that surface. Mesh assets and generated meshes own vertex attributes. Materials
must not synthesize geometry data, and render extraction must not inspect vertex
arrays to decide command behavior.

The renderer uses counter-clockwise front faces. Current generated quads and
world text lie in the local XY plane with +Z normals, +X tangents, and tangent
handedness `+1`. Tangents store only the tangent vector and handedness; the
bitangent is reconstructed from normal, tangent, and handedness in shaders that
need it. UVs use the engine's existing top-left atlas conventions; OBJ loading
continues to flip imported V coordinates to match the renderer's texture-space
expectations.

Missing or invalid imported/generated attributes have deterministic behavior:

- missing normals are generated for indexed triangle meshes when possible,
  using area-weighted face-normal accumulation over the already split vertex
  stream; duplicated vertices preserve authored hard edges
- missing tangents are generated when positions, normals, UVs, and triangle
  topology are available; the current implementation is a first-pass tangent
  basis and is not claimed to be MikkTSpace-compatible
- tangents are Gram-Schmidt orthogonalized against the vertex normal and store
  handedness in `tangent.w`
- degenerate triangles or degenerate UVs produce finite fallback normals or
  tangents instead of NaNs
- missing colors default to white and missing UVs default to zero

`MeshGeometryMetadata` records the processed vertex attribute mask, vertex and
index counts, and whether normals or tangents were generated. Procedural mesh
uploads declare the attributes authored by the producer. Existing
`DynamicMeshComponent` producers default to the legacy unlit contract
(`Position | Color | UV0`), while `WorldTextBindingSystem` and shared generated
quads provide full standard attributes. Debug/tool meshes may stay
semantically unlit; the standard vertex layout still carries fallback
normal/tangent data so backend vertex input remains uniform.

OBJ import deduplicates vertices by the complete `(position index, normal
index, UV index)` tuple, not by position alone. This preserves hard edges and
split UVs authored in OBJ files. Missing normals/UVs are valid and trigger the
generation/default policies above; malformed positive indices are import
errors.

Preliminary material/mesh compatibility:

- `LegacyUnlit`: requires position; UV is required only for textured materials;
  color is optional and defaults to white
- `LegacyUnlit` with `vertexColorOnly`: requires position and meaningful color
  for useful output, but remains unlit
- `StandardSurface`: requires position and normal for lit rendering
- `StandardSurface` with normal mapping: requires position, normal, tangent,
  and UV0

Phase 3A derives the normal matrix in the scene vertex shader from the model
matrix (`mat3(transpose(inverse(model)))`) rather than expanding object upload
data immediately. If profiling later shows this is a bottleneck, the renderer
can add a versioned per-object normal-matrix upload without changing geometry
ownership.

If a `StandardSurface` batch reaches the backend without normal-capable mesh
metadata, the scene stage emits a diagnostic and falls back to unlit shading for
that incompatible batch. If a normal-mapped `StandardSurface` reaches the
backend with normals but without tangents or UV0, the scene stage keeps the
material lit, disables normal-map sampling for that batch, and emits a bounded
diagnostic. It never consumes undefined tangent data.

### GPU Components (engine-managed)

| Component | Managed By |
|-----------|-----------|
| `MeshGpuComponent` | mesh binding systems: `StaticMeshBindingSystem`, `QuadMeshBindingSystem`, `DynamicMeshBindingSystem`, and `WorldTextBindingSystem` |
| `RenderGpuComponent` | `RenderObjectLifecycleSystem` + `RenderableCleanupSystem` + `RenderGpuSystem` |
| `CameraGpuComponent` | `CameraLifecycleSystem` + `CameraGpuSystem` + `CameraBindingSystem` for view IDs; renderer for UBO upload |

`MaterialGpuState` is not an ECS component. It is stored in `MaterialRuntime`
per `MaterialInstanceHandle` and reused by all renderables that reference that
material instance.

Particle emitters use a runtime companion component (`ParticleEmitterRuntimeComponent`)
for simulation state, but particles are extracted into `ParticleFrameDataComponent`
instead of flowing through `RenderCommand`.

World text uses the same descriptor/runtime split:
applications author `WorldTextComponent` with a font asset path, and the
renderer feature owns `WorldTextRuntimeComponent`. The runtime companion caches
the last authored text/font/scale/tint values and the resolved `font_id_type`.
`WorldTextBindingSystem` resolves the font and uploads glyph quads as runtime
mesh data. `MaterialBindingSystem` mirrors the font atlas texture and tint into
a material instance referenced by `MaterialReferenceComponent`. Cleanup removes
stale mesh, material reference, and render-object companions through the shared
renderable cleanup path when the text has no visible glyphs.

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
11. camera view ID allocation / active-camera export systems
12. particle effect hot reload and particle emitter simulation

This order is intentional:
- transform resolution runs before render GPU sync so rendering consumes
  authoritative `WorldTransformComponent` matrices
- mesh lifecycle runs before material lifecycle so world-text font resolution can feed material binding
- material lifecycle runs before render-object lifecycle so readiness is decided from resolved companions
- render-object lifecycle owns object-slot allocation before per-frame sync
- cleanup runs after readiness so descriptor removal tears down stale companions through one path
- texture animation runs after `RenderGpuSystem` so newly ready renderables have
  a valid object slot before animated UV data is queued for upload
- camera control runs before `CameraGpuSystem` so same-frame transform/description changes feed matrix generation
- camera view ID allocation/export runs after matrix generation so rendering and UI see the current frame's active camera state
- particle simulation runs after camera matrix generation so extracted billboards can use the current view

Invalidation ownership follows the lifecycle split. Mesh lifecycle marks mesh
representation changes. Material lifecycle marks material representation
changes. `RenderGpuSystem` marks consumed world-transform changes after
comparing `WorldTransformComponent::version` with
`RenderGpuComponent::uploadedWorldTransformVersion`. Texture animation marks
object-data changes. Readiness and companion add/remove events queue command
topology changes through the render snapshot invalidation queue. Parameter-only
material changes update the material GPU cache and material frame state without
recreating mesh or object-slot state. Material topology changes such as alpha
mode, double-sided state, vertex-color-only mode, shader family, or temporary
legacy additive blend state update the `MaterialVariantKey`, render queue, and
command ordering. Version checks mean "a consumer has not observed a newer
authoritative value"; dirty markers mean "derived extraction data must be
rebuilt."

### RenderCommand

The extractor output:
```
RenderCommand {
    mesh_id_type           meshID
    MaterialInstanceHandle material
    MaterialGpuHandle      materialGpu
    MaterialVariantKey     variantKey
    RenderQueue            renderQueue
    ssbo_id_type           objectSSBOSlot    (per-object transform/UV)
    view_id_type           cameraViewID      (camera UBO)
    uint64_t               sortKey
}
```

`RenderCommand` is backend-agnostic and carries identity, not material
parameters. It does not contain texture IDs, base color/tint, raw blend flags,
Vulkan descriptor sets, pipelines, or ECS entity IDs. Extraction identifies
what to render; `MaterialRuntime` and material frame state define how it
renders; object upload data defines where it renders.

Material frame data is emitted beside the render command list:

```
MaterialFrameState {
    MaterialInstanceHandle material
    MaterialGpuHandle      materialGpu
    MaterialVariantKey     variantKey
    RenderQueue            renderQueue
    MaterialTextureIds     textures             (base color, MR, normal, AO, emissive)
    MaterialFeatureFlags   featureFlags         (map presence / sampling features)
    MaterialGpuParameters  parameters           (base color, surface, emissive)
    MaterialRenderState    renderState
    uint64_t               uploadedVersion
}
```

The Vulkan backend resolves `MaterialGpuHandle` through this frame material
state into backend-owned texture descriptors and the current pipeline choice.
The generic extraction path does not know which material texture roles are
bound. Alpha-masked commands are represented distinctly as
`RenderQueue::AlphaMasked`, but they currently share the opaque Vulkan path
until alpha-cutoff shader support lands.

Render queues are explicit:

- `Opaque`: submitted first and sorted to minimize state changes
- `AlphaMasked`: submitted after opaque and before transparent; currently an
  opaque-path backend fallback
- `Transparent`: submitted last and sorted back-to-front by resolved camera
  depth, with variant/material/mesh ordering as a stable secondary key

Opaque and alpha-masked command ordering groups by render queue, material
variant key, material GPU handle, mesh ID, and object slot. Transparent command
ordering prioritizes camera depth so conventional alpha blending remains
back-to-front. Camera or transform changes that affect transparent depth
invalidate command ordering without requiring material or mesh recreation.

Render extraction also emits upload command side channels:

```
ObjectUploadCommand {
    ssbo_id_type objectSSBOSlot
    glm::mat4    modelMatrix
    glm::vec4    uvTransform    // xy = scale, zw = offset
}

CameraUploadCommand {
    view_id_type cameraViewID
    glm::mat4    viewMatrix
    glm::mat4    projMatrix
    glm::vec3    cameraWorldPosition
    LightingFrameData lighting
}
```

Object uploads are generated only for renderables marked dirty by the render
invalidation queue. They carry the full per-object scene data, currently the
model matrix and scene-material UV transform. Camera uploads are generated every
extracted frame for the active camera and consumed by the renderer after the
current-frame fence wait. The camera UBO is frame-level data: scene shaders use
it for view/projection, camera world position for specular lighting, and the
extracted bounded directional/point/spot lighting arrays plus environment
parameters.

`vertexColorOnly` is a material variant flag for tool and debug meshes that
should render from authored vertex colors without sampling a color texture. The
regular material texture path remains the default for application-authored
geometry.

### Metallic-Roughness Scene Pipeline

Scene shading is world-space:

- the vertex shader transforms positions to world space
- the vertex shader derives `transpose(inverse(mat3(modelMatrix)))` for
  world-space normals
- the fragment shader reads camera world position and `LightingFrameData`
  arrays from the frame/camera UBO
- `StandardSurface` materials evaluate directional, point, and spot radiance
  through one shared Cook-Torrance metallic-roughness BRDF
- `StandardSurface` optionally samples base-color, metallic-roughness, normal,
  AO, and emissive texture roles from shared material state
- `LegacyUnlit` materials ignore light data and remain explicitly unlit

The direct-light BRDF is:

```
specular = (D * G * F) / max(4 * NdotV * NdotL, epsilon)
diffuse  = ((1 - F) * (1 - metallic)) * baseColor / PI
Lo      += (diffuse + specular) * lightRadiance * NdotL
```

Where `D` is GGX/Trowbridge-Reitz, `G` is Smith masking-shadowing using a
Schlick-GGX approximation, and `F` is Schlick Fresnel. `F0` is `vec3(0.04)` for
dielectrics and is mixed toward `baseColor.rgb` for metals. Roughness is
perceptual and clamped to a documented minimum of `0.04` before the shader
derives the microfacet alpha value. Light types differ only in how they produce
`directionToLight` and `lightRadiance`; they do not have separate surface
shading models.

Image-based lighting is evaluated as indirect lighting, separate from the
direct-light loops:

```
F       = fresnelSchlickRoughness(NdotV, F0, roughness)
kd      = (1 - F) * (1 - metallic)
diffuse = irradiance(worldNormal) * baseColor * kd
spec    = prefilteredEnvironment(reflect(-V, N), roughness)
          * (F * brdfLut.x + brdfLut.y)
ibl     = diffuse * AO + spec
```

Environment intensity scales both diffuse irradiance and prefiltered specular
response. Environment rotation is applied around world up by rotating the
sampling directions, not by mutating source textures. Ambient occlusion affects
indirect diffuse IBL only in the current policy; it does not darken direct
Cook-Torrance lighting and does not occlude environment specular yet. Emissive
contribution is added independently in linear space:

```
emissive = emissiveFactor * emissiveStrength * sampledEmissive
```

Emissive materials glow visually but do not illuminate nearby geometry in this
phase.

When no environment is selected, the backend binds valid fallback resources: a
neutral low-intensity irradiance texture, a black specular environment texture,
and the shared BRDF LUT. This removes missing-descriptor cases while keeping
PBR material variants stable across environment enable/disable changes.
Phase 3E is lighting-only: it does not render the selected environment as a sky
background. A future sky pass should consume the same selected environment
state, respect intensity and rotation, and remain independent from material
draw commands.

`MaterialVariantKey` includes shader family and topology-relevant flags such as
alpha mode, double-sided state, temporary legacy blend mode, depth-write state,
vertex-color-only mode, and normal-map presence. It does not include ordinary
material parameters or texture identities: base color, metallic, roughness,
normal scale, AO strength, emissive factor, or the concrete texture handles.
Changing those parameters updates material GPU/frame data without recreating
meshes, object slots, or command topology. Changing shader family or enabling
or disabling normal mapping updates the variant key and command ordering.

The Vulkan scene stage selects lit or unlit pipeline variants from
`MaterialFrameState::shaderFamily` and the batch compatibility result. The
unlit scene path uses the legacy fragment shader, while `StandardSurface` uses
the PBR fragment shader. Shader-family identity selects the shader
implementation before draw execution; there is no per-fragment shader-family
branch.

Current color-space behavior:

- regular color textures are uploaded as `VK_FORMAT_R8G8B8A8_SRGB`, so color
  samples are decoded before lighting math
- material texture roles carry explicit `TextureColorSpace` intent:
  `BaseColor` and `Emissive` are `SRgb`; `MetallicRoughness`, `Normal`, and
  `AmbientOcclusion` are `Linear`
- Vulkan realizes `TextureColorSpace::SRgb` as `VK_FORMAT_R8G8B8A8_SRGB` and
  `TextureColorSpace::Linear` as `VK_FORMAT_R8G8B8A8_UNORM`
- texture cache keys include color-space intent, so the same file path can be
  realized as a color texture and as data texture without accidental sRGB decode
- swapchain/headless frame output prefers sRGB color formats, so shader outputs
  are encoded by the framebuffer path
- lighting math is authored in linear space; metallic-roughness, normal, AO,
  and future mask textures remain linear/data textures

Normal mapping is tangent-space. The vertex shader transforms normals with the
inverse-transpose normal matrix, transforms tangents through the model basis,
orthogonalizes the world tangent against the world normal, and preserves tangent
handedness in `.w` with determinant-sign correction for negative scale. The
fragment shader reconstructs the bitangent as
`cross(worldNormal, worldTangent) * tangent.w`, decodes normal-map samples from
`[0, 1]` to `[-1, 1]`, scales tangent-space X/Y by `normalScale`, normalizes,
and converts the mapped normal into world space.

### Scene Texture Animation

`TextureAnimationComponent` is the engine path for animated textures on regular
scene geometry. `enabled` is the off switch; `mode` only selects how an active
animation behaves. It supports two first-pass modes:

- `UvScroll`: continuous UV offset over controller-frame time, suitable for
  water, lava, fog sheets, shimmer, or other looping surface motion.
- `FlipbookAtlas`: row-major atlas frame selection with optional looping,
  suitable for discrete animated tile frames.

Texture animation can run on unscaled frame time or scaled frame time. Unscaled
is the default so existing visual effects keep moving while simulation timing is
paused. Scaled mode uses frame time multiplied by `TimeContext::timeScale` and
stops while the fixed-step loop is paused.

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
simulation timing, collision, movement, cooldowns, or simulation-authored state.

### Particle Rendering

Particles are an engine rendering feature with asset-facing authoring,
ECS-facing playback descriptors, and a separate Vulkan render stage.

Feature layout:

- `modules/rendering/ecssetup/particles/components/ParticleEmitterComponent.h`: application-facing particle emitter descriptor
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
existing application behavior while keeping ECS playback separate from editor graph
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

Particle tooling now uses the pane-based engine tool shell instead of a legacy
particle panel. `ParticleEditorSession` owns the selected effect path, loaded
`ParticleEffectAsset`, selected emitter/module, dirty flag, playback state, and
compile/save helpers. UI panes emit typed tool commands for asset open, save,
reload, preview playback, emitter/module selection, and enable toggles; the
tool shell system applies those commands, performs asset IO, syncs
`ParticlePreviewWorld`, and publishes `EditorPreviewRenderComponent` data for
the separate particle preview viewport. The central scene viewport remains the
runtime scene viewport. New particle authoring features should extend
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

`TransformSystem` owns world-matrix propagation into `WorldTransformComponent`.

- `TransformComponent` is authoritative local-space data and carries a local version
- `HierarchyComponent` lives under the transform module and owns parent-child relationships
- `gts::transform::markDirty(...)` queues transform dirtiness without touching rendering state
- `TransformSystem` and the shared transform resolver traverse hierarchy, resolve scene-space
  matrices, and increment `WorldTransformComponent::version`
- `attachToParent(...)` rejects ancestor cycles and preserves local transform by default
- `attachToParentPreserveWorld(...)` is the explicit API for reparenting while preserving
  world-space position/orientation/scale where the transform can be decomposed
- Removing a parent hierarchy or destroying a parent promotes children to roots; entity
  lifetime remains owned by application/gameplay code, and children never keep stale parent
  references
- `RenderGpuSystem` consumes `WorldTransformComponent` by comparing
  `RenderGpuComponent::uploadedWorldTransformVersion` to the world-transform version
- When rendering consumes a new world transform, it copies the model matrix into
  GPU companion state and queues render snapshot dirtiness
- `RenderDirtyComponent` remains the bridge to extraction for render-owned mesh,
  material, object-data, and consumed-transform updates

This split keeps scene meaning in transform/hierarchy systems and leaves
rendering responsible for representation and upload state.

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

`modules/diagnostics/debugdraw/` is a standalone feature for transient visual
diagnostics. Callers enqueue world-space primitives through helpers in
`DebugDrawPrimitives.h`; `DebugDrawSystem` batches those lines by color into
dynamic meshes. Debug draw uses `MaterialComponent::vertexColorOnly` so axes,
bounds, rays, and frusta are not dependent on sampled debug textures.

Debug draw is intentionally generic. Tool-specific policy, such as drawing the
selected entity bounds or pick ray, lives under `modules/tools/debugdraw/`.
Physics-specific diagnostic policy lives under `modules/diagnostics/physics/`
and emits shared debug-draw primitives instead of owning a separate line
rendering path.

---

## 6. Event Buses

The engine has two event buses with distinct domains. Use the right one for the right layer.

### GtsPlatformEventBus — platform/infrastructure events

Owned by `VulkanGraphics`, passed by reference to `ForwardRenderer`, `WindowManager`, and `OutputWindow`. Dispatched once per frame in `GtsPlatform::beginFrame()`.

- **Emit** from OS/GPU callbacks: `eventBus.emit(MyEvent{...})`
- **Dispatch** on the main thread: `eventBus.dispatch()` — delivers all queued events
- **Subscribe** via `SubscriptionToken`: `token = eventBus.subscribe<MyEvent>([](const MyEvent& e) { ... })`

Dispatch is deferred because GLFW callbacks fire during `glfwPollEvents()`.
Queueing ensures delivery always happens on the main thread at a predictable
point.

Current platform events: `GtsWindowResizeEvent`, `GtsKeyEvent`, `GtsFrameEndedEvent`.

### ECSWorld — ECS Domain Events

Integrated into `ECSWorld`, accessed via `ctx.world.publish/subscribe` from any system.

- **Publish** (immediate): `ctx.world.publish(MyEvent{...})` — all handlers called before publish() returns
- **Subscribe** via `SubscriptionToken`: `token = ctx.world.subscribe<MyEvent>([](const MyEvent& e) { ... })`

Dispatch is immediate because ECS updates are single-threaded and sequential. Events published during a system's update are received by later systems in the same pass.

**Rule of thumb:** if the event originates from hardware, a driver, or a GPU callback → `GtsPlatformEventBus`. If it originates from domain logic during the ECS update loop → `ECSWorld`.

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
for application-owned debug controls.

Mouse position, buttons, scroll deltas, and cursor capture flow through
`IInputSource` and `InputBindingRegistry`; engine systems should consume those
values from `EcsControllerContext::input`.

### Simulation And Controller Input

Pressed/released input has two timing domains:

- `isPressed()` / `isReleased()` expose frame-local edges for controller
  systems, UI, tools, and engine commands that run once per rendered frame.
- `isSimulationPressed()` / `isSimulationReleased()` expose action edges latched
  until the next fixed simulation tick consumes them. Simulation systems should
  use these methods for edge-triggered simulation commands so input cannot be
  missed when render FPS is much higher or lower than the simulation tick rate.

`InputManager` records raw key/button edges generated while polling OS events,
including press-and-release pairs that happen inside one rendered frame.
`InputBindingRegistry` resolves those raw edges through active input contexts
into semantic action edges and decrements simulation edge queues after each
fixed tick via `finishSimulationTick()`. Context stack changes, binding
replacement, and pause transitions clear queued simulation edges so stale
simulation commands cannot leak across scene/menu boundaries.

The rule is strict: fixed-step simulation systems must never depend on
frame-local input edges. They may read held state for continuous commands, but
edge-triggered simulation commands must use the simulation-latched input API.
Controller systems may use frame-local edges because they run once for every
input update and are responsible for presentation, UI, tools, and engine-level
commands rather than deterministic simulation progression.

### Fixed Simulation And Presentation

`GtsGameLoop` uses a fixed-timestep accumulator. Simulation systems run zero or
more fixed ticks per rendered frame, then controller systems run once for the
current rendered frame. `GravitasEngine` copies `GtsGameLoop::alpha()` into
`TimeContext::simulationAlpha` after fixed ticks and before controller systems,
so frame-facing presentation code can interpolate between fixed-step snapshots.

Simulation correctness therefore comes from fixed simulation plus
simulation-latched input. Visual smoothness at render rates above or below the
simulation tick rate requires presentation systems to interpolate from previous
and current fixed-step snapshots using the accumulator alpha. Presentation
systems may use `ctx.time->unscaledDeltaTime` for visual-only effects, UI, and
tools, but authoritative simulation movement, timers, cooldowns, domain state,
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

Runtime ownership and authoring conventions are intentionally separate. This
section describes the runtime architecture. New UI should follow
[docs/ENGINE_UI_AUTHORING_GUIDE.md](docs/ENGINE_UI_AUTHORING_GUIDE.md), whose standard
authoring hierarchy is:

`UiSurface -> UiLayer -> UiMount -> UiComposition -> UiWidget / UiWidgetAsset -> retained UiNode -> renderer`

The v1.0 validation and compatibility classification are recorded in
[docs/ENGINE_UI_RUNTIME_ARCHITECTURE.md](docs/ENGINE_UI_RUNTIME_ARCHITECTURE.md).
Engine tooling ownership and pane/inspector conventions are documented in
[docs/ENGINE_TOOLING_ARCHITECTURE.md](docs/ENGINE_TOOLING_ARCHITECTURE.md).

Raw retained nodes, canvas/anchor rectangles, direct payload styling, and
`UiSystem::dispatchResult()` remain compatibility and low-level tooling paths,
not the default way to build feature UI.

Each surface owns a `UiDocument` with a hidden document root plus one or more
ordered layer roots. Existing callers that create nodes without specifying a
surface or parent still attach to the default layer root returned by
`UiSystem::getRoot()`, so current retained UI builders continue to work. New
engine or application systems may create named layers through the default-surface APIs
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
when a container can express the geometry. Visibility changes invalidate layout,
and sequential flow containers such as `Stack` and `Dock` exclude hidden
children from allocated gaps and slots.

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
separators, scroll views, and progress bars. Stock widgets expose a consistent
`setVisible(...)` API for their root state, including compound child nodes where
the widget owns them. Widgets remain clients of the UI runtime; they do not own
surfaces, mounts, focus, modal policy, rendering, or composition lifetime.

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

Engine tooling is a major retained UI client, but it is no longer built as a
legacy panel registry or generic editor framework. The active shell is a
`UiComposition` composed of self-contained panes backed by ordinary retained UI
widgets. Future visual layout/theme/binding/accessibility editors should follow
the same runtime model: edit authored asset data, validate through the relevant
asset runtime, preview by instantiating real assets into a dedicated surface or
pane, and keep runtime retained handles as preview implementation details
rather than editor document state.

UI live reload is engine-owned by `UiAssetRuntime`. It tracks authored asset
identity, source paths, runtime versions, validation state, dependencies, and
runtime consumers for serialized UI assets, widget assets, and theme assets.
`UiSystem::instantiateUiAsset(...)` and `UiSystem::instantiateWidgetAsset(...)`
register mounted runtime consumers automatically; mount and surface destruction
unregister them. Reload requests validate before replacement, update the
dependency graph, recursively invalidate dependents, rebuild affected consumers,
and preserve keyboard focus by stable widget id when possible. Authoring tools
that save widget assets should let this runtime refresh previews instead of
owning private reload paths. OS file watching, package/plugin ownership, theme
file serialization, and deeper state transfer are future clients of this asset
runtime.

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
Tooling and sample clients may load reusable assets from package manifests.
Runtime code plugin loading, package asset root scanning, signatures,
repository identity, and package browsers are future layers above this
ownership model.

UI localization core is engine-owned by `UiLocalizationRuntime`, which is a
global UI platform runtime owned by `UiSystem` rather than a surface-local
manager. Phase 21A supports package-aware localization catalogs, active and
default locale state, parent-locale fallback such as `de-AT -> de`,
catalog-declared fallback/source locale participation, package-relative and
absolute namespace key lookup, missing-key diagnostics, catalog validation, and
JSON parse/serialize helpers. `UiPackageRuntime` can register
`UiLocalizationAsset` data from package assets; package unload unregisters the
catalog. Phase 21B extends authored UI with optional serialized localization
references: `UiSerializedWidget::textKey` and semantic `nameKey`,
`descriptionKey`, `hintKey`, and `valueKey`. `UiSerializationRuntime` resolves
those refs through `UiLocalizationRuntime` during instantiation, attaches the
localized text/semantic values to retained nodes, and falls back to the existing
embedded strings when a key is absent or missing. `UiWidgetAssetRegistry`
preserves the same fields through reusable widget assets, inheritance,
variants, overrides, nested expansion, and parameter substitution. Mounted UI
refresh on language switch, localized target tracking, editor locale preview,
language-pack live reload, catalog parameter substitution, plural/select rules,
RTL rendering, and font fallback remain future localization subphases.

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
tool and application overlays. The UI primitive set also includes retained line nodes,
which render thick colored screen-space segments for graph-like widgets such as
skill-tree links, retained circle nodes for icon buttons or graph nodes that need
circular hit targets, and retained nine-slice image nodes for scalable panels.

Tool-specific styling lives above the retained UI layer in
`modules/tools/ui/EditorTheme.h` and `modules/tools/ui/ToolTheme.h`. These
headers provide data-only typography, spacing, dimensions, and color tokens for
tool panes. They should keep moving toward ordinary `UiTheme` styles where the
runtime widget set supports the required behavior.

Engine tooling uses a pane-based retained UI shell. `ToolPane.h` defines pane
descriptors, workspace visibility, typed tool commands, view-model row data,
and the `ToolPane` lifecycle. `EngineToolPanes.h` implements individual panes
for menu chrome, workspace tabs, toolbar, world viewport, scene browser,
particle effect hierarchy, emitter details, particle preview viewport,
property inspector, curve/timeline placeholder, diagnostics, and status.
`EngineToolShellComposition.h` owns only global chrome, fixed dock layout,
workspace tab visibility, pane mounting, and command collection.

The tool system applies pane commands and owns engine integration.
`EngineToolShellSystem.hpp` keeps the editor mode at runtime, requests scene
changes through engine commands, performs particle asset IO, updates
`ParticleEditorSession`, syncs `ParticlePreviewWorld`, publishes preview render
data, and mirrors retained UI dispatch into `EngineToolInputCaptureComponent`
for camera, picker, and gizmo systems. Panes must not directly mutate ECS,
scene state, or particle assets.

`ParticleEditorSession` owns particle authoring state: selected effect,
selected emitter/module, dirty flag, playback state, compile/save helpers, and
the future undo/redo boundary. It is deliberately separate from pane widgets so
particle data edits can grow without making the shell composition monolithic.

`UiPanelSkin` is the reusable panel styling contract for retained UI surfaces.
It can describe a solid-color panel, a single image panel, or a nine-slice image
panel, plus tint, source slice fractions, and content padding. `UiPanelStateSkin`
layers normal, hover, pressed, and disabled panel skins with fallback to normal
when a state skin is not supplied. `UiPanelSkinNode` in rendering UI builds a
small retained-node subtree with solid/image/nine-slice visual children and a
padded content container. Menus, windows, VN dialogue boxes, choice buttons,
tool panels, and future UI surfaces should use this shared skin path instead of
growing feature-local panel rendering helpers.

Nine-slice UI rendering is implemented as a normal retained UI primitive. The
visual resolver splits a node into nine textured quads, preserves corner UVs,
stretches edges/center as needed, and clips each segment before emitting regular
UI draw commands. Slice values are normalized fractions of the source image and
destination bounds; a future texture-metadata pass may add pixel-exact slice
authoring without changing the retained node contract.

### Tween Module

`modules/tween/Tween.h` provides the reusable `gts::tween::Tween<T>` helper and
standard easing modes. It is a small value-interpolation utility rather than an
ECS system: engine or application systems own tweens in their own runtime state,
tick them with the appropriate fixed or controller delta time, and apply the
resulting value to their own descriptors/state. This keeps tweening reusable for
UI, presentation, tools, or domain-owned systems without tying it to one
renderer or component type.

### Visual Novel Module

`modules/visualnovel/` is an engine-level VN presentation/runtime module. It is
content-neutral: applications provide scripts, character IDs, image asset paths,
story state, branching labels, and domain-specific commands.

The module is split into:

- `runtime/`: `VNStage`, `VNRuntime`, `VNScript`, typed `VNCommand` data, and
  `VNCommandRegistry`
- `ui/`: retained UI compositions and presentation adapters for dialogue boxes,
  choices, fullscreen backgrounds, dimming, and character sprite images
- `systems/`: `VNSystem`, a controller system that owns one runtime instance,
  routes continue/input events, syncs UI, and writes playback/frontend state
- `components/`: `VNPlaybackStateComponent`, a lightweight singleton view of
  whether VN playback is active, waiting, or blocking application input, and
  `VNExternalPresentationComponent`, an optional presentation override for
  dialogue-driven VN backgrounds, dimming, and sprites, plus
  `VNFrontendStateComponent`, which publishes the VN surface, composition,
  interaction layer/slot, modal layer, and modal-slot mount for application-side
  interaction frontend integrations, and `InteractionFrontendSessionComponent`,
  which identifies the active application interaction mode without putting
  domain rules into VN

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
background/dimming/wait/choice/jump. `VNCommandRegistry` lets applications
register domain-specific commands without hardcoding application vocabulary into
the engine module.

VN presentation is authored through `VNPresentationProfile`, not through
scripts. The profile owns dialogue panel, shadow, nameplate, choice-button state
skins, text colors/scales, semantic interaction layout, compatibility layout
seeds, and sprite motion presets. The runtime frontend converts that profile
into surface-local `UiTheme` style classes, metrics, skins, typography, and
widget layout constraints. Scripts should continue to describe content and
presentation intent such as speaker, text, choices, sprite identity, background
mode, and named animation preset; concrete panel textures, nine-slice values,
padding, and hover/pressed visuals belong in the active profile/theme seed.

`VNInteractionLayout` is the preferred layout authoring model for interaction
frontend UI. It describes named overlay slots, content insets, and choice-stack
intent: dialogue panel slot, nameplate slot, speaker/body text slots, continue
prompt slot, choice stack width/gap/capacity, interaction slot, and sprite
capacity. `VNLayoutProfile` coordinate rectangles remain supported as
compatibility seeds; the runtime derives the same semantic descriptors from
them before building UI. Do not add new VN rectangles for future interaction
features.

The VN frontend is authored as `VNDialogueComposition` over the retained UI
runtime. It builds a stage layer, dialogue panel widgets, speaker/body/continue
labels, a vertical choice stack of button widgets, retained event handling,
navigation metadata, an interaction slot, and a modal slot. Choice activation routes through
`UiComposition::onEvent(...)` and `UiNavigationGraph` submit events instead of
polling `UiDispatchResult` handles. Stage sprite placement remains VN stage
presentation data because sprites are authored in normalized stage coordinates,
not as conventional controls.

The same mounted frontend now acts as the engine's Interaction Frontend shell.
Dialogue is one mode inside that shell. Non-modal application feature views
should mount into the published interaction slot. True blocking overlays should
mount into the published modal slot and use `UiModalManager`.
`InteractionFrontendSessionComponent` lets an application feature keep the VN
stage active after a dialogue graph hands off to another interaction mode, while
the feature continues to own its own business logic.

Dialogue-driven VN presentation can opt into a traditional full-VN scene without
changing dialogue graph data. An application may populate `VNExternalPresentationComponent`
with a fullscreen image background, dimming value, VN sprites, and optional
scene/particle render suppression while a `DialogueRuntimeComponent` is active.
`VNSystem` applies that override to active external dialogue and active
interaction sessions. If the component is absent or inactive, dialogue continues
to overlay the current 3D scene and render pass visibility is restored to
defaults when no interaction session is active.

`VNSystem` also consumes the generic execution-profile stack. While external
dialogue is active, current-scene presentation pushes the `dialogue_overlay`
profile so simulation and physics can sleep while camera/render prep continue to
show the live scene. Fullscreen image, solid-color, or scene-suppressed
presentation pushes `fullscreen_dialogue`, which keeps `Ui`, `Dialogue`, `VN`,
`Audio`, `Tools`, and `Always` systems awake while skipping world render build.
When dialogue ends, `VNSystem` pops the named profile it pushed. This makes
fullscreen VN an engine execution mode rather than a VN-specific pause branch.

### Narrative Dialogue Runtime

`modules/narrative/dialogue/` is a generic graph-based narrative runtime. It
owns dialogue graph execution, current node tracking, visible-choice evaluation,
branching, generic conditions, generic actions, and start/end state. It
deliberately knows nothing about application domain concepts or presentation.

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
- `NPCInteractionComponent`: generic speaker/dialogue graph reference for
  application-owned interactable entities
- `DialogueSystem`: controller system that processes start requests and
  publishes `DialogueStartedEvent`, `DialogueNodeChangedEvent`,
  `DialogueChoicesChangedEvent`, `DialogueActionRequestedEvent`, and
  `DialogueEndedEvent`

The VN module consumes dialogue state only as presentation. When a
`DialogueRuntimeComponent` is active, `VNSystem` mirrors the current dialogue
node into its retained VN UI state and routes choice clicks or continue input
back into `DialogueRuntime`. This bridge does not add rendering code to the
dialogue runtime and does not put application domain rules into VN presentation.

Engine tooling is a global development runtime owned by `GravitasEngine`.
`EngineToolRuntime` updates once per rendered frame after the active scene's
normal controller systems, but it targets the active scene `ECSWorld` for
inspection and edits. Application scenes no longer install editor systems
directly. Renderer, physics, application simulation, and application UI systems
remain scene-owned.

Current tool feature layout:

- `modules/tools/core/`: shared tool state and query helpers
- `modules/tools/ui/`: pane descriptors, pane widgets, shell composition,
  shell system, tool theme tokens, and particle editor session state
- `modules/tools/runtime/`: global tool runtime and scene-change state handoff
- `modules/tools/workspace/`: per-frame workspace layout and scene viewport
  publication
- `modules/tools/assets/`: particle preview world integration
- `modules/tools/selection/`: input capture, world picking, selection labels,
  selection highlight, and shared raycast helpers
- `modules/tools/gizmos/`: translation gizmo state, picking, snapping, and
  transform edits
- `modules/tools/debugdraw/`: tool-driven bounds, axes, frustum, and pick-ray
  drawing

Tooling state is held in singleton ECS components:

- `EngineToolStateComponent`: visibility, runtime editor mode, selected entity,
  selection source, and status
- `EngineToolInputCaptureComponent`: pointer/UI/world-consumption state
- `EngineGizmoStateComponent`: gizmo mode, hovered/active axis, snap settings
- `DebugDrawSettingsComponent`: debug primitive toggles and sizing

The runtime carries editor-level state across scene changes, such as F6
visibility, active workspace, gizmo settings, and debug-draw settings. Scene-entity
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
`EcsControllerContext`, and `SceneBrowserPane` lists whatever scenes the
application registered. Selecting a row emits a tool command that the shell
system applies through
`GtsCommandBuffer::requestChangeScene(sceneId)`. Engine tooling must not
hardcode application scene names or keep scene-entity references across scene changes.

Selection currently comes from world picking. The world picker uses
`ActiveCameraViewStateComponent` plus `BoundsComponent` to raycast against live
entities. `ToolEntityLabelComponent` gives entities human-readable
names/categories and can mark internal tool entities as non-selectable.

The translation gizmo is currently position-only. It draws X/Y/Z axes through
debug draw, follows Unreal-style colors (X red, Y green, Z blue), supports world
or local axes, and writes selected entity `TransformComponent::position` while
marking the transform dirty.

The engine tool camera is active only while the engine tools are visible. It
uses `engine.tool_camera_*` actions inside the `engine.tools` input context.
Debug cameras are application-owned systems and should not be installed by the
shared renderer feature.

### Engine Font

Engine debug/tool overlays use `resources/fonts/gravitasfont.font.json`, which
points at `resources/fonts/gravitasfont.png` and stores atlas metrics. This
replaced the older generated retro font for engine UI because the upscaled cell
grid gives readable lowercase letters, digits, and symbols without introducing
image-filter noise.

---

## 9. Resource Model

Assets are accessed through `IResourceProvider` (passed in `SceneContext`). Binding systems call into this interface when descriptor components are first processed:

- **Meshes**: loaded from `.obj` via tinyobjloader, processed into the standard
  position/normal/tangent/color/UV vertex contract, and uploaded to GPU
  vertex/index buffers
- **Textures**: decoded via stb_image, uploaded to Vulkan images
- **Fonts**: loaded from `.font.json`; `FontManager` owns glyph metrics and uses `TextureManager` for atlas textures
- **Shaders**: pre-compiled SPIR-V stored in `/shaders/`
- **Engine assets**: minimal shared resources in `/resources/`

GPU resource handles (mesh IDs, texture IDs, font IDs, SSBO slots, camera view IDs) are allocated by lifecycle/binding systems and stored in GPU/runtime components. The steady-state contract is:
- lifecycle systems do near-zero work when no descriptor lifecycle work is pending
- structural GPU companion-component changes are deferred through `world.commands()`
- cleanup of backend resources still happens via removal callbacks on GPU components
- application code does not create, remove, or read GPU companion components directly
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
- Application/UI code requests changes through `gts::rendering::requestApplyGraphicsSettings(...)`.
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
abstraction so application settings UI can choose which display owns windowed,
borderless, or exclusive fullscreen output without depending on GLFW directly.

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
that must run in every runtime mode. Most scene simulation should use
`Gameplay`; renderer companion/lifecycle work should use `RenderPrep`; camera
presentation should use `Camera`; retained UI sync/interaction should use `Ui`;
dialogue graph bridges should use `Dialogue`; VN presentation should use `VN`;
engine or application debug tooling should use `Tools`.

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

### Adding a Tool Pane

1. Add a `ToolPaneId` and `PaneDescriptor` metadata entry.
2. Implement a self-contained `ToolPane` that owns its widgets, local UI state,
   retained event handling, and typed command emission.
3. Mount the pane from `EngineToolShellComposition` and assign it a fixed dock
   layout until freeform docking is intentionally designed.
4. Apply commands in `EngineToolShellSystem` or a feature session/model. Panes
   must not directly mutate ECS, scene state, renderer state, or assets.

New reusable retained UI should prefer a `UiComposition` mounted through
`UiSystem` and should handle retained input through `UiComposition::onEvent(...)`
and widgets. Shared controls should become widgets or theme-backed helpers
rather than a new panel registry.

---

## 11. Key Design Principles

- **ECS-first**: all state is components, all behavior is systems — no monolithic managers
- **Deterministic simulation**: simulation systems run at fixed timestep, isolated from frame timing
- **Simulation-safe input**: fixed-step simulation reads latched simulation input
  edges, not frame-local controller input edges
- **CPU/GPU separation**: descriptor components are domain logic; GPU components are backend state; binding systems bridge them explicitly
- **Descriptor-only app boundary**: applications and UI code operate on descriptors or engine-exported frame state, not GPU companion components
- **Lazy updates**: render commands and GPU state are cached and only rebuilt on change
- **Fence-safe frame data**: per-frame camera data is uploaded by the renderer after the matching frame fence has been waited
- **Data extraction over direct coupling**: the renderer never reads ECS directly; the extractor produces an immutable command list
- **Explicit structural mutation**: hot-path queries stay non-structural; lifecycle mutation is deferred and flushed at controlled points
- **Profile-driven runtime modes**: scenes stay loaded while execution profiles
  decide which broad system groups and render build paths are awake
- **Separation of lifecycle concerns**: mesh lifecycle, material lifecycle, render-object lifecycle, camera lifecycle, cleanup, transform sync, and extraction are distinct stages with distinct ownership
- **RAII resource management**: backend resources tied to GPU component lifecycle via removal callbacks
- **Modular assembly**: scenes are built by adding systems and components — the engine imposes no mandatory scene structure
- **Feature-first tooling**: editor/tooling panels live under `modules/tools/`, generic visualization under `modules/diagnostics/debugdraw/`, physics diagnostic producers under `modules/diagnostics/physics/`, and runtime particle simulation under rendering particle ECS setup
