# Engine Tooling Architecture

This document describes the stable Gravitas engine tooling architecture. It is
the primary onboarding reference for contributors and AI agents working on
engine tools.

The tooling UI is built on the retained UI runtime:

```text
UiSurface -> UiLayer -> UiMount -> UiComposition -> UiWidget -> retained UiNode
```

Tooling uses that runtime through a pane-based shell. It must not return to the
legacy `EngineToolPanel` registry as the main editor shell.

## Overview

Engine tooling is a global development runtime owned by the engine, not by
individual scenes. It updates against the active scene ECS world so tools can
inspect and edit the currently running scene without requiring applications to
install editor systems.

The active editor surface is:

- a retained `EngineToolShellComposition`
- a descriptor-driven `ToolDockLayout`
- self-contained `ToolPane` implementations
- reusable tooling widgets for rows, lists, pagers, toolbar rows, inspectors,
  and property inspectors
- an `EngineToolShellSystem` that applies commands and integrates with ECS,
  rendering, asset IO, and input capture

The first-class workspaces are:

- World: scene browser and live runtime world viewport.
- Particles: particle asset/effect hierarchy, live runtime world viewport,
  separate particle preview viewport, property inspector, diagnostics, and
  timeline placeholder.

Workspace switching filters pane descriptors. It does not change engine mode.

## Ownership

### ToolShellComposition

`ToolShellComposition` owns editor shell structure only:

- global chrome
- workspace tabs
- pane mounting
- dock layout usage
- command collection

It does not know how to edit particles, mutate ECS, perform asset IO, render
previews, or apply tool commands.

### ToolDockLayout

`ToolDockLayout` owns descriptor-driven pane placement.

Input:

- pane descriptors
- active workspace
- shell/workspace bounds

Output:

- resolved pane id
- visibility
- dock area
- rectangle
- order

The shell mounts panes from resolved layouts. Adding a new pane should require
implementing the pane and registering its descriptor, not adding pane-specific
layout code to the shell.

Current dock areas are fixed and intentionally simple:

- MenuBar
- Toolbar
- Left
- Center
- Right
- Bottom
- StatusBar

Freeform docking, drag docking, floating panes, workspace persistence, and
serialized docking trees are future work.

### ToolPane

A `ToolPane` owns one self-contained editor pane:

- retained widgets
- pane-local UI state
- retained event handling
- typed `ToolCommand` emission

Panes do not mutate ECS, scene state, renderer state, or particle assets. They
translate user interaction into commands.

### Reusable Tool Widgets

Reusable tooling controls live under `modules/tools/ui/` and compose retained
runtime widgets. They are not a second UI framework.

Current reusable controls include:

- `ToolSelectableRow`
- `ToolListSection`
- `ToolPager`
- `ToolInspectorSection`
- `ToolToolbarRow`
- `ToolPropertyInspector`

The property inspector is descriptor-driven. Editors produce
`ToolPropertySection` and `ToolPropertyDescriptor` lists explicitly. The
inspector renders those descriptors and emits `ToolCommand`s. It does not know
what an emitter, material, entity, or camera is.

### ParticleEditorSession

`ParticleEditorSession` owns particle editor state:

- selected effect
- selected emitter
- selected module
- dirty state
- playback state
- compile/save/reload helpers
- future undo history

It applies particle asset edits requested by the shell system. It is also the
boundary where future undo/redo should live.

### EngineToolShellSystem

`EngineToolShellSystem` owns engine integration:

- ECS integration
- renderer integration
- viewport publication
- particle preview rendering
- asset IO
- scene commands
- input capture
- applying `ToolCommand`s

It consumes commands from the shell, applies them to engine services or feature
sessions, rebuilds view models, and publishes render/input state for the rest of
the engine.

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

## Tooling Launch Presets

Tooling launch presets configure the engine into a deterministic startup state.
They are intended for development, screenshot capture, visual verification, and
future regression tests.

A preset is not:

- an editor workspace save file
- user preferences
- docking/layout persistence
- arbitrary widget state serialization
- runtime persistence

The startup flow is:

```text
Application launch
  -> parse --tooling-preset=<json>
  -> initialize engine runtime
  -> seed EngineToolRuntime
  -> seed ParticleEditorSession through EngineToolShellSystem
  -> load requested scene as the initial active scene
  -> open requested workspace
  -> open requested particle effect
  -> restore requested emitter/module selection
  -> wait for screenshot timing
  -> request screenshots through the existing renderer screenshot path
  -> optionally exit after the final capture
```

No simulated UI clicks are used. The preset configures runtime state directly.

### Command Line

Primary usage:

```text
DungeonCrawler --tooling-preset=engine/docs/tooling_presets/particles.json
```

Supported overrides:

```text
--screenshot-directory=screenshots/tooling/run_001
--no-auto-exit
```

Avoid adding many individual launch flags. The JSON preset is the primary
configuration mechanism.

### JSON Schema

Current schema:

```json
{
  "tools": {
    "visible": true,
    "workspace": "particles",
    "visualEvaluation": true,
    "debugDraw": false,
    "gizmos": false,
    "scene": "dungeon_test",
    "particleEffect": "resources/particles/ambient_brazier.json",
    "selectedEmitter": 0,
    "selectedModule": 0
  },
  "screenshots": {
    "enabled": true,
    "afterSeconds": 2.0,
    "intervalSeconds": 1.0,
    "count": 3,
    "directory": "screenshots/tooling/particles",
    "exitAfterCapture": true
  }
}
```

Fields:

- `tools.visible`: seeds `EngineToolStateComponent::visible`.
- `tools.workspace`: `world` or `particles`.
- `tools.visualEvaluation`: enables a clean screenshot-oriented startup state.
  It clears tool selection and disables debug draw/gizmos unless explicit
  overrides are also supplied.
- `tools.debugDraw`: enables or disables tool debug draw primitives at startup.
  Visual evaluation presets should set this to `false`.
- `tools.gizmos`: enables or disables transform gizmos at startup. Visual
  evaluation presets should set this to `false`.
- `tools.scene`: scene id registered by the application.
- `tools.particleEffect`: particle asset path opened by `ParticleEditorSession`.
- `tools.selectedEmitter`: selected emitter index, clamped by the session.
- `tools.selectedModule`: selected module index, clamped by the session.
- `screenshots.enabled`: enables automated screenshot scheduling.
- `screenshots.afterSeconds`: delay before first capture.
- `screenshots.intervalSeconds`: delay between captures.
- `screenshots.count`: number of captures to request.
- `screenshots.directory`: output directory. Relative paths resolve from the
  project root.
- `screenshots.exitAfterCapture`: exits after the final requested capture.

Example presets live in `engine/docs/tooling_presets/`:

- `empty_editor.json`
- `world_viewer.json`
- `particles.json`
- `particles_inspector.json`
- `editor_eval_world.json`
- `editor_eval_particles.json`

### Visual Evaluation Presets

Editor art-direction and UI-polish work should use the `editor_eval_*` presets.
These presets intentionally separate editor UI evaluation from gameplay scene
evaluation.

The evaluation environment should provide:

- no gameplay HUD
- no visual-novel overlays
- no debug primitives
- no gizmos unless explicitly enabled
- no particle debug overlays
- no blue camera/frustum tint
- stable camera and scene content
- neutral lighting and materials
- deterministic screenshot timing

The registered `editor_visual_eval` scene exists only as a neutral backdrop for
tool screenshots. It should stay boring: if a future UI pass needs richer
content, add representative but stable editor-evaluation props rather than
pointing visual tests back at gameplay scenes such as `dungeon_test`.

### Screenshot Automation

Preset screenshots reuse the existing renderer screenshot infrastructure. The
engine schedules screenshot requests; the renderer captures the frame and writes
PNG files using the same path as manual screenshots.

The preset only controls timing, count, destination directory, and optional
exit. It does not implement image comparison or golden-file validation. Those
belong to future visual regression tooling.

## Viewport Model

The editor has two different viewport concepts.

The central world viewport is the live runtime scene viewport. The tool runtime
publishes it through `RenderViewportComponent::sceneViewport`, and render
systems use that viewport to constrain scene rendering so tool chrome is not
covered.

The particle preview viewport is separate. `ParticlePreviewViewportPane` owns
only the retained preview image node and chrome. `EngineToolShellSystem`
measures that node after composition update, asks `ParticlePreviewWorld` to
build preview render data, and publishes `EditorPreviewRenderComponent` for the
renderer.

The particle preview must not be merged into the central world viewport.

## Important Constraints

- `EditorMode` must remain `Runtime` while tools are visible so the selected
  runtime scene keeps rendering.
- Particle preview rendering is system-owned, not pane-owned.
- `dispatchResult()` is compatibility glue for viewport/world input capture
  only. Ordinary UI interaction should use retained events and widgets.
- The legacy `EngineToolPanel` registry must not return as the main runtime
  shell.
- Panes must not know about sibling panes.
- Panes must not own ECS mutation, scene commands, renderer integration, asset
  IO, preview rendering, or particle session logic.
- Do not add docking trees, drag docking, floating panes, or workspace
  persistence until the fixed-pane editor workflow is stable.
- Do not introduce runtime reflection or serialization-driven property editing
  for tools. Property editors should be explicit descriptor builders.

## Adding A Tool Pane

1. Add a pane id and descriptor metadata.
2. Implement a `ToolPane` that owns its widgets and local interaction state.
3. Use existing tooling widgets where possible.
4. Emit typed `ToolCommand`s.
5. Apply commands in `EngineToolShellSystem` or a feature session.

The shell should not gain pane-specific layout logic.

## Adding Tool Properties

1. Read feature/session state in the shell system or feature adapter.
2. Build explicit `ToolPropertySection` and `ToolPropertyDescriptor` values.
3. Give editable descriptors a stable id and command.
4. Let `ToolPropertyInspector` render rows and emit edit commands.
5. Apply edits in `EngineToolShellSystem` and/or the owning session.
6. Mark dirty, recompile, save, reload, and refresh previews from the owning
   session/system, not from the inspector.

This keeps future editors compositional: Material, Entity, Camera, Physics, and
Animation tools can produce property descriptors without sharing domain logic
with the inspector UI.
