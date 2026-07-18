# Engine Tooling Architecture

This document describes the current Gravitas engine tooling architecture. The
tooling UI is built on the retained UI runtime:

```text
UiSurface -> UiLayer -> UiMount -> UiComposition -> UiWidget -> retained UiNode
```

Tooling uses that runtime through a pane-based shell. The previous
`EngineToolPanel` registry must not return as the main editor shell.

## Overview

Engine tooling is a global development runtime owned by the engine, not by
individual scenes. It updates against the active scene ECS world so tools can
inspect and edit the currently running scene without applications installing
editor systems.

The active editor surface is:

- a retained `EngineToolShellComposition`
- descriptor-driven `ToolDockLayout`
- self-contained `ToolPane` implementations
- reusable tooling widgets for rows, lists, pagers, toolbar rows, inspectors,
  and property inspectors
- rounded rect payloads for pane borders, shadows, and large surface separation
- named tool surface roles for panes, raised panels, overlays, controls, rows,
  chrome, and section headers
- `EngineToolShellSystem`, which applies commands and delegates input capture
  and preview rendering to tool-owned helper systems

The first-class workspaces are:

- World: scene browser and live runtime scene viewport.
- Particles: particle asset/effect hierarchy, isolated particle preview
  viewport, property inspector, and diagnostics.
- Assets: manifest browser, selected asset model preview, property inspector,
  and diagnostics.

Workspace switching filters pane descriptors. It does not change engine mode.

## Feature Layout

- `modules/tools/core/`: shared tool state, query helpers, workspace enums, and
  non-UI formatting/path helpers.
- `modules/tools/ui/`: pane descriptors, pane widgets, shell composition,
  shell system, tool theme tokens, property inspectors, and grouped pane
  implementations under `ui/panes/`.
- `modules/tools/particles/`: particle editor session state, particle asset
  IO-facing editing helpers, and the isolated particle preview world.
- `modules/tools/runtime/`: global tool runtime, scene-change state handoff,
  and preview coordination for tool-owned render targets.
- `modules/tools/workspace/`: per-frame workspace layout and scene viewport
  publication.
- `modules/tools/assets/`: asset manifests, asset browser session state, asset
  discovery, validation, and asset preview world integration.
- `modules/tools/selection/`: input capture state/updating, world picking,
  selection labels, selection highlight, and shared raycast helpers.
- `modules/tools/gizmos/`: translation gizmo state, picking, snapping, and
  transform edits.
- `modules/tools/debugdraw/`: tool-driven bounds, axes, frustum, and pick-ray
  drawing.

## Ownership

`ToolShellComposition` owns editor shell structure only: global chrome,
workspace tabs, pane mounting, dock layout usage, and command collection. It
does not edit particles, mutate ECS, perform asset IO, render previews, or
apply tool commands.

`ToolDockLayout` owns descriptor-driven pane placement. Its input is pane
descriptors, active workspace, and shell/workspace bounds. Its output is
resolved pane id, visibility, dock area, rectangle, and order.

Current dock areas are fixed:

- `MenuBar`
- `Toolbar`
- `Left`
- `Center`
- `Right`
- `Bottom`
- `StatusBar`

Freeform docking, drag docking, floating panes, workspace persistence, and
serialized docking trees are future work.

`ToolPane` owns one self-contained editor pane: retained widgets, pane-local UI
state, retained event handling, and typed `ToolCommand` emission. Panes do not
mutate ECS, scene state, renderer state, or particle assets.

Reusable tooling controls live under `modules/tools/ui/` and compose retained
runtime widgets. They are not a second UI framework. Current reusable controls
include `ToolSelectableRow`, `ToolListSection`, `ToolPager`,
`ToolInspectorSection`, `ToolToolbarRow`, and `ToolPropertyInspector`.

`EngineToolShellSystem` owns shell orchestration: ECS integration, viewport
publication, asset discovery, view-model building, scene commands, and applying
`ToolCommand`s. It delegates pointer/input capture to
`EngineToolInputCaptureSystem` and tool preview publication to
`EngineToolPreviewCoordinator`.

## Command Flow

Tool interaction follows one direction:

```text
Pane
  -> ToolCommand
  -> ToolShellComposition command queue
  -> EngineToolShellSystem
  -> feature session or engine service
  -> updated view model
  -> panes
```

For property editing:

```text
Feature/session state
  -> explicit property builder
  -> ToolPropertySection list
  -> ToolPropertyInspector
  -> EditProperty ToolCommand
  -> EngineToolShellSystem
  -> ParticleEditorSession or future feature session
```

No pane should bypass this flow by directly mutating assets, ECS components, or
renderer data.

## Particle Editor Session

`ParticleEditorSession` owns particle editor state:

- selected effect
- selected emitter
- selected module
- dirty state
- playback state
- compile/save/reload helpers
- future undo/redo insertion point

`EngineToolShellSystem` builds particle view models and property descriptors,
then applies emitted commands back through the session.

The particle preview is separate from the central world viewport.
`ParticlePreviewViewportPane` owns preview chrome and an image handle.
`EngineToolPreviewCoordinator` measures that handle after composition update,
routes `ParticlePreviewWorld` rendering into the preview target, and publishes
`EditorPreviewRenderComponent`.

## Asset Browser Session

`AssetBrowserSession` owns toolchain asset browser state:

- discovered asset manifests
- selected manifest path
- manifest load diagnostics

Asset manifests describe reusable authored assets independently of any game
scene placement. Game code may consume the same manifest type for runtime
spawning, but asset discovery, validation, preview, and future import/cook
commands belong to engine tooling.

`AssetPreviewPane` owns the retained UI image handle for the selected asset.
`EngineToolPreviewCoordinator` measures that handle after composition update,
routes `AssetPreviewWorld` rendering into the preview target, and publishes
`EditorPreviewRenderComponent`. The preview world is engine-tool owned and loads
the manifest model path through the same mesh/material binding systems as
runtime scenes. `unlit_texture_override` manifests use a shared unlit material
with the fallback texture; `cooked_mesh_materials` manifests enable submesh
material bindings from the cooked mesh and use the standard lit material path.

## Input Capture

`EngineToolInputCaptureSystem` converts retained UI dispatch state, mouse
motion, scroll input, action bindings, and the current render viewport into the
singleton `EngineToolInputCaptureComponent`. World-view cameras, particle
previews, asset previews, picking, and gizmos should consume that component
instead of reading raw input independently.

## Tooling Launch Presets

Tooling launch presets configure the engine into a deterministic startup state
for development, screenshot capture, visual verification, and future regression
tests. Presets are documented in [presets.md](presets.md).

Preset startup configures runtime state directly. It does not simulate UI
clicks.

## Viewport Model

The editor has two viewport concepts.

The central world viewport belongs to the World workspace. The tool runtime
publishes it through `RenderViewportComponent::sceneViewport`, and render
systems use that viewport for runtime scene rendering while the scene viewer is
active.

Particle and asset preview viewports are separate from the central world
viewport. They are driven by `EditorPreviewRenderComponent` and must not be
merged into the central world viewport. Particle and asset workspaces may keep
the runtime scene alive for engine continuity, but they do not expose a live
scene viewport pane.

## Current Limitations

These are known limits, not accidental regressions:

- Docking is fixed, descriptor-driven, and non-resizable.
- No floating windows or layout persistence.
- Curve/timeline authoring is not exposed until it has real editing behavior.
- No graph editing, curve editing, gradient editing, burst editing, or module
  add/remove/reorder.
- Property editing is limited to the first particle fields and stepper controls.
- Module parameters are read-only.
- List sections use fixed row counts rather than virtualization or scrolling.
- Some pane internals still use normalized rectangles because the current
  widget helper layer is lightweight.
- Theme usage is improved but not complete; some low-level payload styling
  still exists inside tooling widgets.

## Guardrails

Do not:

- revive `EngineToolPanel` as the main editor shell
- add pane-specific placement switches back into `EngineToolShellComposition`
- make panes mutate ECS, scene state, renderer state, or particle assets
- merge the particle preview into the world viewport
- switch to an editor mode that stops live runtime scene rendering
- add freeform docking before the fixed pane editor is stable
- introduce reflection or serialization-driven property editing
- simulate UI clicks for preset startup
- use gameplay scenes as the default editor visual QA surface

Do:

- build new editor features as pane-local retained widgets plus typed commands
- extend `ToolPaneWidgets` only for repeated editor patterns
- build explicit `ToolPropertyDescriptor` lists for inspector controls
- apply edits through `EngineToolShellSystem` and owning sessions
- keep `EditorMode::Runtime`
- use `editor_eval_*` presets before and after visual changes
- keep generated screenshots clearly labeled
