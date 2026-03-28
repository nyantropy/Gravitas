# Gravitas Engine — Architecture Reference

Written for a developer or AI agent with no prior context. All content reflects the actual current state of the codebase.

---

## 1. Engine Overview

Gravitas is a C++20 Vulkan game engine built around two parallel systems: an **ECS** (Entity Component System) for game logic and world-space rendering, and a **UiTree** for retained screen-space UI elements. The renderer uses a two-stage frame graph (`SceneRenderStage` → `UiRenderStage`), driven by typed command extractors that read from the ECS and the UI tree respectively. The game loop runs simulation at a fixed timestep; controller systems and rendering run every frame regardless of pause state. GLFW handles windowing and input. GLM, stb_image, and tinyobjloader are bundled; GLFW is fetched by CMake at configure time.

---

## 2. Project Structure

```
Gravitas/
├── applications/           # Four standalone demo applications
│   ├── GravitasTetris/     # Tetris — reference application pattern
│   ├── DungeonCrawler/     # First-person dungeon prototype
│   ├── GtsScene1/          # Basic scene / scratch pad
│   └── GtsScene2/          # Frustum culling test scene
├── engine/
│   ├── GravitasEngine.hpp  # Engine entry point — owns everything
│   ├── GtsPlatform.h       # OS subsystems (graphics, input, window)
│   ├── core/               # gravitas_core — ECS, scene, input, UI tree, time, RNG
│   │   ├── ecs/            # ECS framework + extractors + description components
│   │   ├── scene/          # GtsScene base, SceneContext, SceneManager
│   │   ├── input/          # IInputSource, InputActionManager, GtsKey
│   │   ├── options/        # GraphicsConstants, GlmConfig, Types
│   │   ├── command/        # GtsCommandBuffer
│   │   ├── rng/            # GtsRNG
│   │   └── time/           # TimeContext, Timer
│   └── modules/
│       ├── rendering/      # gravitas_rendering + Vulkan backend
│       │   ├── core/       # UiTree, font loader, windowing, interfaces
│       │   ├── ecssetup/   # Binding systems, GPU components, GPU systems
│       │   └── backend/vulkan/  # gravitas_vulkan_setup + gravitas_vulkan_backend
│       ├── camera/         # CameraDescriptionComponent, CameraGpuSystem, CameraBindingSystem
│       ├── transform/      # TransformComponent
│       ├── hierarchy/      # HierarchyComponent, HierarchyHelper
│       └── animation/      # AnimationComponent
├── external/               # Bundled: GLM, stb, tinyobjloader
├── resources/              # Game assets: .obj models, .png textures, bitmap fonts
└── shaders/                # GLSL source + compiled SPIR-V blobs
```

---

## 3. CMake Targets

Applications link only `GravitasEngine` (an alias for `gravitas_engine`) and receive all engine headers and libraries transitively.

| Target | Type | Description | Key dependencies |
|--------|------|-------------|-----------------|
| `gravitas_vulkan_setup` | STATIC | Vulkan instance, physical/logical device, swapchain, GLFW surface | Vulkan SDK, GLFW (FetchContent) |
| `gravitas_vulkan_backend` | STATIC | Render passes, pipelines, frame graph, resource managers (mesh, texture, SSBO, camera UBO) | `gravitas_vulkan_setup`, `gravitas_core` |
| `gravitas_rendering` | STATIC | UiTree, binding systems, GPU systems, windowing, font loader | `gravitas_vulkan_backend`, `gravitas_core` |
| `gravitas_modules` | INTERFACE | Camera, transform, hierarchy, animation (all header-only) | `gravitas_core`, `gravitas_rendering` |
| `gravitas_core` | STATIC | ECS framework, scene management, input, description components, options, RNG | — (no Vulkan, no GLFW) |
| `gravitas_engine` | INTERFACE | Umbrella — aggregates all of the above | all above |

`gravitas_core` is intentionally Vulkan-free. Any header that includes `<vulkan/vulkan.h>` must live in `gravitas_rendering` or `gravitas_vulkan_backend`.

---

## 4. Render Pipeline

```
GravitasEngine::render()
    │
    ├─ RenderCommandExtractor::extract(GtsExtractorContext)   → vector<RenderCommand>
    └─ UiCommandExtractor::extract(viewportAspect)            → UiCommandBuffer
            │
            ▼
    IGtsGraphicsModule::renderFrame(dt, renderList, uiBuffer, stats)
            │
            ▼
    ForwardRenderer::renderFrame()
            │
            ▼
    GtsFrameGraph::execute()
            │
            ├─ SceneRenderStage   — depth-tested 3D geometry
            │       loadOp = CLEAR, depth attachment, CCW front faces, back-face culling
            │       one draw call per RenderCommand, push constant = objectIndex + alpha
            │
            └─ UiRenderStage      — screen-space overlay, no depth
                    loadOp = LOAD (composites on top), no depth attachment
                    one draw call per UiDrawCommand, alpha blending
                    GtsDebugOverlay appended here from GtsFrameStats*
```

`GtsFrameGraph` owns the render stages, manages Vulkan pipeline barriers between them, and provides a per-frame data blackboard (producers call `graph.setData<T>()`, consumers call `graph.getData<T>()`). `ForwardRenderer` owns the frame graph, the depth attachment, `RenderResourceManager`, and `FrameManager`. `GtsFrameStats::triangleCount` always reflects the **previous** frame — it is read after `frameGraph.execute()` completes (intentional).

---

## 5. Component Pattern — World-Space Rendering

All world-space renderables follow the same three-layer pattern:

```
Description component  (game code writes — asset paths and values only)
        │
        ▼  read by
Binding system  (ECSControllerSystem — runs every frame)
        │  writes to
        ▼
GPU components  (engine-internal — game code never reads or writes these)
        │
        ▼  read by
RenderCommandExtractor  →  SceneRenderStage
```

### Renderable types

| Description components | Binding system | GPU components created | Notes |
|----------------------|----------------|----------------------|-------|
| `StaticMeshComponent` + `MaterialComponent` | `StaticMeshBindingSystem` | `MeshGpuComponent` + `MaterialGpuComponent` + `RenderGpuComponent` | Mesh loaded from .obj via `requestMesh()` |
| `ProceduralMeshComponent` + `MaterialComponent` | `ProceduralMeshBindingSystem` | Same as above | Flat quad generated at runtime; `doubleSided` always true |
| `WorldTextComponent` | `WorldTextBindingSystem` | Same as above | Font atlas is the texture; mesh rebuilt when `dirty = true` |

### Rules
- Game code **never** calls `requestMesh`, `requestTexture`, `requestObjectSlot`, or `uploadProceduralMesh`. Only binding systems do.
- All binding systems are registered by `GtsScene::installRendererFeature()`.
- `RenderCommandExtractor` picks up any entity with `RenderGpuComponent` + `MeshGpuComponent` + `MaterialGpuComponent` automatically — no changes to the extractor or render stages are needed when adding a new type.
- `RenderGpuComponent::objectSSBOSlot` starts at `RENDERABLE_SLOT_UNALLOCATED` (`numeric_limits<ssbo_id_type>::max()`). The extractor skips entities with the sentinel value.
- `RenderGpuComponent::readyToRender` starts `false`. `RenderGpuSystem` sets it `true` after computing the first model matrix. The extractor skips entities where it is still `false` (prevents one-frame origin glitch).
- `RenderGpuComponent::dirty = true` must be set by any system that modifies `TransformComponent` on a renderable. Exception: entities with a valid `HierarchyComponent::parent` always recompute.

### Adding a new world-space renderable type
1. Create a description component (paths/values only, no GPU handles) in `engine/core/ecs/desc/`
2. Create a binding system in `engine/modules/rendering/ecssetup/systems/`
3. Register it in `GtsScene::installRendererFeature()`
4. Done — the extractor picks it up automatically

---

## 6. UI System

Screen-space UI elements are **not** ECS entities. They are managed by `UiTree`, a standalone retained data structure owned by `GravitasEngine`.

```
Scene calls ctx.ui->addImage() / addText()    →  returns UiHandle
Scene calls ctx.ui->update(handle, desc)      →  updates the element
GravitasEngine calls uiTree->clear()          →  between scene loads

UiCommandExtractor::extract(viewportAspect)
        │  calls
        ▼
UiTree::buildCommandBuffer(viewportAspect)
        │  resolves textures internally via injected IResourceProvider*
        │  lays out glyphs via GlyphLayoutEngine::appendUiText()
        ▼
UiCommandBuffer  →  UiRenderStage
```

### Key points
- `UiHandle` (`uint32_t`) is the opaque identifier returned by `addImage`/`addText`. `UI_INVALID_HANDLE = 0`.
- `UiTree` owns `IResourceProvider*` — injected at construction in `GravitasEngine`. `UiCommandExtractor` has **zero** resource provider dependency.
- Internal storage: separate `vector<UiImageElement>` and `vector<UiTextElement>`, with `unordered_map<UiHandle, size_t>` index maps for O(1) lookup. Removal uses swap-erase.
- `UiTree::clear()` is called before every scene load (`setActiveScene` and `applyCommands`).
- `GtsDebugOverlay` bypasses the tree — it appends directly to `UiCommandBuffer` inside `UiRenderStage` using a `GtsFrameStats*` injected at stage construction. The tree does not manage debug overlay elements.
- Screen-space images: `UiImageDesc` — fields: `texturePath`, `x`, `y`, `width`, `imageAspect`, `tint`, `visible`.
- Screen-space text: `UiTextDesc` — fields: `text`, `font` (`BitmapFont*`), `x`, `y`, `scale`, `color`, `visible`.
- Positions are normalised viewport coordinates: `(0, 0)` = top-left, `(1, 1)` = bottom-right.

---

## 7. Extractor Pattern

```cpp
// Base interface — only RenderCommandExtractor implements this
template<typename TOutput>
class IGtsExtractor {
    virtual TOutput extract(const GtsExtractorContext& ctx) = 0;
};

struct GtsExtractorContext {
    ECSWorld& world;
    float     viewportAspect;
};
```

| Extractor | Output type | ECS dependency | Notes |
|-----------|------------|----------------|-------|
| `RenderCommandExtractor` | `vector<RenderCommand>` | Yes — reads 3 GPU components + camera | Implements `IGtsExtractor`. Frustum cull, stable_partition opaque-first |
| `UiCommandExtractor` | `UiCommandBuffer` | No | Does NOT implement `IGtsExtractor`. Takes only `UiTree*`. Calls `buildCommandBuffer(viewportAspect)` |

`GravitasEngine` calls both extractors once per frame in `render()`, before calling `platform.getGraphics()->renderFrame()`.

---

## 8. Scene Interface

```cpp
class GtsScene {
protected:
    ECSWorld ecsWorld;
public:
    virtual void onLoad(SceneContext&, const GtsSceneTransitionData* = nullptr) = 0;
    virtual void onUpdateSimulation(SceneContext&) = 0;       // fixed timestep only
    virtual void onUpdateControllers(SceneContext& ctx) {}    // every frame
};
```

`installRendererFeature()` must be called once in `onLoad`. It registers the following controller systems in order:

1. `RenderGpuSystem` — computes model matrices from `TransformComponent`, uploads to SSBO
2. `CameraGpuSystem` — computes view/proj matrices, writes `CameraGpuComponent`
3. `StaticMeshBindingSystem` — resolves `.obj` paths to GPU mesh IDs
4. `ProceduralMeshBindingSystem` — uploads runtime quad meshes
5. `WorldTextBindingSystem` — rebuilds glyph mesh when `WorldTextComponent::dirty`
6. `CameraBindingSystem` — allocates camera UBO, uploads matrices
7. `RenderResourceClearSystem` — recycles freed SSBO slots
8. `DefaultCameraControlSystem` — basic camera movement (skipped by `CameraControlOverrideComponent`)

### SceneContext fields

| Field | Type | Description |
|-------|------|-------------|
| `resources` | `IResourceProvider*` | Load meshes, textures, allocate GPU slots |
| `inputSource` | `IInputSource*` | Pause-filtered raw key state |
| `actions` | `InputActionManager<GtsAction>*` | Engine-level semantic actions (always live) |
| `time` | `const TimeContext*` | `deltaTime`, `unscaledDeltaTime`, `frame` |
| `engineCommands` | `GtsCommandBuffer*` | Issue `LoadScene`, `ChangeScene`, `TogglePause`, `Quit` |
| `windowAspectRatio` | `float` | Current viewport width / height |
| `extractor` | `RenderCommandExtractor*` | Query visible/total counts, toggle/freeze frustum |
| `ui` | `UiTree*` | Add, update, remove screen-space UI elements |

---

## 9. Key Conventions and Gotchas

**GLM configuration** — Always `#include "GlmConfig.h"` instead of including GLM headers directly. This file defines `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, and `GLM_ENABLE_EXPERIMENTAL` before any GLM include. Violating this causes silently wrong depth math.

**UI ortho projection** — `UiRenderStage` uses `glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f)`. Do not change this. The Y-axis orientation is intentional given the GLM settings. There is a comment in `UiRenderStage.h` marking this line.

**Backface culling** — Enabled by default (`VK_CULL_MODE_BACK_BIT` in `VulkanPipelineConfig`). Set `MaterialComponent::doubleSided = true` for planes, quads, and anything viewed from both sides. `ProceduralMeshBindingSystem` sets `doubleSided = true` automatically for all procedural quads.

**Winding order** — Front faces are counter-clockwise (`VK_FRONT_FACE_COUNTER_CLOCKWISE`). Quad index pattern: `0, 2, 1, 1, 2, 3` (TL → BL → TR, TR → BL → BR). Getting this wrong looks identical to a missing backface culling fix but has a different cause.

**Coordinate system** — Y-up, right-handed. In the dungeon crawler: North = −Z, East = +X. Grid-to-world: `worldPos = (gridX + 0.5, eyeHeight, gridZ + 0.5)`.

**ECS pause behaviour** — `ECSSimulationSystem` (game logic) stops running when paused. `ECSControllerSystem` (rendering, input, GPU updates) runs every frame regardless. UI updates and camera movement must be in controller systems if they should continue while paused.

**Frustum culling** — Requires a `BoundsComponent` on the entity. Entities without one are **never culled** regardless of config (safe default). Toggle with `ctx.extractor->setFrustumCullingEnabled()`. Freeze the cull volume with `ctx.extractor->toggleFrustumFreeze()`.

**Triangle count latency** — `GtsFrameStats::triangleCount` shows the previous frame's triangle count. This is intentional — it is read after `frameGraph.execute()` completes and thus reflects completed GPU work, not the current frame's submission.

**gravitas_core is Vulkan-free** — Any header that transitively includes `<vulkan/vulkan.h>` must live in `gravitas_rendering` or `gravitas_vulkan_backend`. Placing such a header in `gravitas_core` breaks the Windows build for non-Vulkan compile paths and violates the layering contract.

**Descriptor set layout**

| Set | Binding | Content |
|-----|---------|---------|
| Set 0 | 0 | Camera UBO (view + projection matrices) |
| Set 1 | 0 | Object SSBO (array of model matrices) |
| Set 2 | 0 | Combined image sampler (texture) |

Object index into the SSBO is pushed via push constants (first 4 bytes) before each draw call.

---

## 10. Application Pattern

All four applications follow the same structure. `GravitasTetris` is the canonical reference.

```cpp
// main.cpp
int main()
{
    EngineConfig config;
    config.graphics.backend = GraphicsBackend::Vulkan;
    // ... window size, title, etc.

    GravitasEngine engine(config);
    engine.registerScene("main", std::make_unique<MyScene>());
    engine.setActiveScene("main");
    engine.start();
}

// MyScene.hpp
class MyScene : public GtsScene
{
public:
    void onLoad(SceneContext& ctx, const GtsSceneTransitionData* = nullptr) override
    {
        // 1. Load resources and create ECS entities
        // 2. Create singletons for global state
        // 3. Register application-specific systems
        // 4. Call installRendererFeature() — LAST, after app systems
        installRendererFeature();
    }

    void onUpdateSimulation(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
    }

    void onUpdateControllers(SceneContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);
    }
};
```

Application-specific files live entirely under `applications/<AppName>/` and are glob-compiled into a single executable. The application links only `GravitasEngine` and gets all engine headers transitively.
