# Engine Tooling Handoff

Status date: 2026-07-11

This handoff summarizes the Gravitas tooling migration work completed so far
and the current state future contributors should build from.

Primary references:

- `engine/ARCHITECTURE.md`
- `engine/docs/ENGINE_UI_AUTHORING_GUIDE.md`
- `engine/docs/ENGINE_UI_RUNTIME_ARCHITECTURE.md`
- `engine/docs/ENGINE_TOOLING_ARCHITECTURE.md`
- `engine/docs/tooling_presets/README.md`

The temporary migration handoff documents should no longer be treated as the
source of truth. The stable architecture now lives under `engine/docs/`.

## Current State

The tooling UI is now based on the retained engine UI system. The main editor
shell is pane-based, descriptor-driven, and no longer centered around the old
`EngineToolPanel` registry.

The editor currently supports two first-class workspaces:

- World: scene browser, live runtime scene viewport, scene inspector,
  diagnostics, timeline placeholder, status bar.
- Particles: particle asset/effect hierarchy, live runtime scene viewport,
  separate particle preview viewport, generic property inspector, diagnostics,
  timeline placeholder, status bar.

The intended editor mode remains `EditorMode::Runtime`. Tools run over the live
runtime scene instead of switching the engine into a special particle editor
mode that would stop scene rendering.

## Completed Work

### 1. Tooling UI Migration

The legacy giant tool surface was replaced with a retained pane shell.

Important files:

- `engine/engine/modules/tools/ui/EngineToolShellComposition.h`
- `engine/engine/modules/tools/ui/EngineToolPanes.h`
- `engine/engine/modules/tools/ui/ToolPane.h`
- `engine/engine/modules/tools/ui/ToolDockLayout.h`
- `engine/engine/modules/tools/ui/EngineToolShellSystem.hpp`
- `engine/engine/modules/tools/ui/ParticleEditorSession.h`

Current ownership:

- `ToolShellComposition` owns global chrome, workspace tabs, pane mounting,
  dock layout usage, and command collection.
- `ToolDockLayout` resolves `PaneDescriptor` metadata into runtime layout
  slots.
- `ToolPane` implementations own their widgets, local UI state, retained event
  handling, and command emission.
- `EngineToolShellSystem` owns ECS integration, renderer integration, viewport
  publication, preview rendering, asset IO, scene commands, input capture, and
  command application.
- `ParticleEditorSession` owns selected effect/emitter/module, dirty state,
  playback state, compile/save/reload, and future undo history.

Hard rule: panes do not directly mutate ECS state, scene state, renderer state,
or particle assets. Panes emit typed `ToolCommand`s.

### 2. Descriptor-Driven Dock Layout

Pane placement is now descriptor-driven.

`PaneDescriptor` is the single source of truth for:

- pane id
- title
- workspace visibility
- dock area
- order
- preferred/minimum sizing
- leading offset
- collapsible/closable metadata

`ToolDockLayout` supports only the intentionally simple fixed dock areas:

- `MenuBar`
- `Toolbar`
- `Left`
- `Center`
- `Right`
- `Bottom`
- `StatusBar`

There is no freeform docking, drag docking, floating panes, serialized layout,
workspace persistence, or docking tree yet. Do not add those until the fixed
pane workflow is visually and behaviorally stable.

### 3. Reusable Tool Pane Widgets

Common pane internals were extracted into lightweight tooling-layer widgets.

Important files:

- `engine/engine/modules/tools/ui/ToolPaneWidgets.h`
- `engine/engine/modules/tools/ui/ToolTheme.h`
- `engine/engine/modules/tools/ui/EditorTheme.h`

Reusable controls now include:

- `ToolSelectableRow`
- `ToolListSection`
- `ToolPager`
- `ToolInspectorSection`
- `ToolToolbarRow`

Recent visual refinements include:

- hierarchy row depth
- disclosure markers
- type badges
- selected/hover/disabled/warning paint states
- pager hiding when no paging action is available
- toolbar grouping tokens
- viewport overlay tokens
- inspector row tokens

These helpers are not a second UI framework. They compose existing retained UI
widgets into editor-specific patterns.

### 4. Generic Tool Property Inspector

The property inspector is generic and descriptor-driven.

Important files:

- `engine/engine/modules/tools/ui/ToolPropertyTypes.h`
- `engine/engine/modules/tools/ui/ToolPropertyInspector.h`
- `engine/engine/modules/tools/ui/EngineToolShellSystem.hpp`

Data flow:

```text
feature/session state
  -> explicit property builder
  -> ToolPropertySection / ToolPropertyDescriptor list
  -> ToolPropertyInspector
  -> ToolCommand
  -> EngineToolShellSystem
  -> owning session/service
```

Implemented particle properties:

- selected emitter enabled
- selected emitter max particles
- selected emitter emission rate
- selected emitter lifetime minimum
- selected emitter lifetime maximum
- selected module enabled
- selected module parameters displayed read-only

There is no runtime reflection, automatic property registration, or
serialization-driven property editing.

Current editor controls are intentionally simple. Numeric properties use
stepper buttons, not text fields or sliders.

### 5. Particle Editor Session Flow

The particle editor now has a session object for editor state and asset
lifecycle.

`ParticleEditorSession` owns:

- selected effect
- selected emitter
- selected module
- dirty state
- playback state
- compile/save/reload helpers
- future undo/redo insertion point

`EngineToolShellSystem` builds particle view models and property descriptors,
then applies emitted commands back through the session.

The particle preview is separate from the world viewport. `ParticlePreviewViewportPane`
only owns preview chrome and an image handle. `EngineToolShellSystem` measures
that handle after composition update and routes `ParticlePreviewWorld` rendering
into the preview target.

### 6. Deterministic Tool Launch Presets

Tooling startup can now be seeded from JSON.

Important files:

- `engine/engine/modules/tools/runtime/ToolLaunchPreset.h`
- `engine/engine/modules/tools/runtime/EngineToolRuntime.hpp`
- `engine/GravitasEngine.hpp`
- `src/main.cpp`
- `engine/docs/tooling_presets/*.json`

Primary command:

```sh
build/src/DungeonCrawler --tooling-preset=engine/docs/tooling_presets/editor_eval_particles.json
```

Optional overrides:

```sh
--screenshot-directory=<path>
--no-auto-exit
```

Preset capabilities:

- show/hide tools
- select workspace
- load requested scene
- open particle effect
- restore selected emitter/module
- enable visual evaluation mode
- disable debug draw/gizmos
- schedule screenshots
- optionally exit after capture

The preset system is not workspace persistence and does not serialize widget
state, docking state, or arbitrary runtime data.

### 7. Visual Evaluation Environment

A neutral editor evaluation scene exists so UI screenshots do not depend on
gameplay scenes.

Important files:

- `src/scene/EditorVisualEvaluationScene.h`
- `src/scene/EditorVisualEvaluationScene.cpp`
- `src/scene/SceneRegistry.cpp`
- `engine/docs/tooling_presets/editor_eval_particles.json`
- `engine/docs/tooling_presets/editor_eval_world.json`

Scene id:

```text
editor_visual_eval
```

Visual evaluation presets are intended to provide:

- no gameplay HUD
- no VN overlays
- no debug primitives
- no gizmos unless explicitly enabled
- no camera/frustum blue-line clutter
- stable camera
- stable lighting
- neutral scene content
- deterministic screenshot timing

Game HUD visibility was adjusted so tool visibility can suppress gameplay HUD
surfaces during editor evaluation.

### 8. Screenshot Automation And Screenshot Write Path

Tooling presets reuse the existing renderer screenshot mechanism.

Relevant files:

- `engine/GravitasEngine.hpp`
- `engine/core/command/GtsCommandBuffer.h`
- `engine/engine/modules/rendering/backend/vulkan/rendering/ScreenshotManager.cpp`
- `engine/engine/modules/rendering/backend/vulkan/rendering/ScreenshotManager.hpp`

Current screenshot behavior:

- `GravitasEngine` schedules screenshot requests from the preset timing.
- Renderer screenshot requests still go through the normal graphics module path.
- `ScreenshotManager` allocates output paths and captures the swapchain image
  into a staging buffer.
- PNG conversion/write work is moved to async write jobs so disk encoding does
  not block the render thread as much.
- Pending writes are pruned and joined on manager destruction.

Remaining performance caveat: image transition/copy/map still happens on the
render path. If screenshot hitches remain unacceptable, the next optimization
should be frame-delayed readback with a small staging buffer ring.

### 9. First Editor UX Pass

The editor has begun moving away from debug UI.

Current visual improvements:

- darker professional editor palette
- editor-specific bitmap font
- larger and more consistent type tokens
- grouped top toolbar regions
- clearer selected workspace/row states
- hierarchy rows with badges and depth
- viewport header/footer and subtle edge chips
- separate particle preview presentation and preview status strip
- denser property inspector rows
- editable-state rails in the property inspector
- timeline placeholder with curve legend, ticks, playhead, and tabs
- reduced dead pager UI when no page action exists

Important files:

- `engine/resources/fonts/editor_sans.font.json`
- `engine/resources/fonts/editor_sans.png`
- `engine/engine/modules/tools/ui/EditorTheme.h`
- `engine/engine/modules/tools/ui/ToolTheme.h`
- `engine/engine/modules/tools/ui/ToolPaneWidgets.h`
- `engine/engine/modules/tools/ui/ToolPropertyInspector.h`
- `engine/engine/modules/tools/ui/EngineToolPanes.h`
- `engine/engine/modules/tools/ui/EngineToolShellSystem.hpp`

Screenshots are generated through presets and are usually ignored/generated
artifacts. Regenerate before/after screenshots locally when doing visual work.

## Current Limitations

These are known and should not be misread as accidental regressions:

- Docking is fixed, descriptor-driven, and non-resizable.
- No floating windows or layout persistence.
- Timeline/curve area is a visual placeholder.
- No graph editing, curve editing, gradient editing, burst editing, or module
  add/remove/reorder.
- Property editing is limited to the first particle fields and stepper controls.
- Module parameters are read-only.
- List sections use fixed row counts rather than virtualization or scrolling.
- Some pane internals still use normalized rects because the current widget
  helper layer is still lightweight.
- Theme usage is improved but not complete; some low-level payload styling still
  exists inside tooling widgets.
- The editor is better visually than the original debug UI, but still not near
  the provided reference image's polish.

## Validation Commands

Run from the repository root:

```sh
cmake --build build
git diff --check
git -C engine diff --check
```

Visual QA:

```sh
build/src/DungeonCrawler \
  --tooling-preset=engine/docs/tooling_presets/editor_eval_particles.json \
  --screenshot-directory=screenshots/tooling/editor_eval_particles

build/src/DungeonCrawler \
  --tooling-preset=engine/docs/tooling_presets/editor_eval_world.json \
  --screenshot-directory=screenshots/tooling/editor_eval_world
```

For quick temporary captures, use `/tmp/<run-name>` as the screenshot directory.

## Guardrails For Future Agents

Do not:

- revive `EngineToolPanel` as the main editor shell
- add pane-specific placement switches back into `EngineToolShellComposition`
- make panes mutate ECS, scene state, renderer state, or particle assets
- merge the particle preview into the world viewport
- switch to an editor mode that stops live runtime scene rendering
- add freeform docking before the fixed pane editor is stable
- introduce reflection or serialization-driven property editing
- simulate UI clicks for preset startup
- put visual QA back onto gameplay scenes as the default

Do:

- build new editor features as pane-local retained widgets plus typed commands
- extend `ToolPaneWidgets` only for repeated editor patterns
- build explicit `ToolPropertyDescriptor` lists for inspector controls
- apply edits through `EngineToolShellSystem` and owning sessions
- keep `EditorMode::Runtime`
- use `editor_eval_*` presets before and after visual changes
- keep generated screenshots clearly labeled

## Suggested Next Work

Recommended next milestone:

```text
Editor visual polish pass 2: pane proportions, typography, and empty-state cleanup.
```

Reason:

The architecture is now usable enough that the biggest remaining gap is visual
quality and working density, not framework structure. A good next pass should:

- compare fresh `editor_eval_particles` and `editor_eval_world` screenshots
  against the reference image
- reduce debug-like labels and placeholder copy
- improve pane proportions and sidebar rhythm
- tighten typography and row spacing
- make empty diagnostics/details sections collapse or read intentionally
- avoid adding particle features until the current surfaces look stable

After that, implement Particle Parameter Editor v2 with richer controls only if
the inspector layout still holds up visually.
