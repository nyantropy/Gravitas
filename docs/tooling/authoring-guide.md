# Engine Tooling Authoring Guide

Use this guide when adding or changing Gravitas engine tools. Architecture
details live in [architecture.md](architecture.md). Preset and screenshot
workflow details live in [presets.md](presets.md).

## Adding A Tool Pane

1. Add a pane id and descriptor metadata.
2. Implement a `ToolPane` that owns its widgets and local interaction state.
3. Use existing tooling widgets where possible.
4. Emit typed `ToolCommand`s.
5. Apply commands in `EngineToolShellSystem` or a feature session.

The shell should not gain pane-specific layout logic. Adding a new pane should
mean registering a descriptor and mounting the pane through the existing dock
layout.

## Adding Tool Properties

1. Read feature/session state in the shell system or feature adapter.
2. Build explicit `ToolPropertySection` and `ToolPropertyDescriptor` values.
3. Give editable descriptors a stable id and command.
4. Let `ToolPropertyInspector` render rows and emit edit commands.
5. Apply edits in `EngineToolShellSystem` and/or the owning session.
6. Mark dirty, recompile, save, reload, and refresh previews from the owning
   session/system, not from the inspector.

Do not introduce runtime reflection or serialization-driven property editing.
Material, entity, camera, physics, animation, and particle tools can all
produce property descriptors without sharing domain logic with the inspector.

## Tool Pane Rules

Panes may:

- own pane-local widgets
- own pane-local transient UI state
- handle retained events for their own controls
- emit typed commands
- render view-model data provided by the shell system/session

Panes may not:

- directly mutate ECS or scene state
- directly mutate renderer state
- directly load/save particle assets
- know about sibling panes
- own preview rendering
- request scene changes directly

## Reusable Tool Widgets

Extend `ToolPaneWidgets` only for repeated editor patterns such as selectable
rows, inspector sections, toolbar rows, pagers, or property rows. One-off pane
layout wrappers should remain local to the pane.

Reusable tooling widgets are clients of retained UI widgets and themes; they
are not a replacement UI framework.

## Viewport Rules

The central world viewport is the active runtime scene viewport. It is
published through `RenderViewportComponent::sceneViewport`.

Particle preview is a separate preview render path. Keep preview measurement,
preview world sync, and `EditorPreviewRenderComponent` publication in
`EngineToolShellSystem` or owning preview systems, not panes.

## Visual QA Rules

Use deterministic presets for visual work:

```sh
build/src/DungeonCrawler \
  --tooling-preset=engine/docs/tooling/tooling_presets/editor_eval_particles.json \
  --screenshot-directory=screenshots/tooling/editor_eval_particles
```

Capture before and after screenshots with clear labels. Agents with image
inspection support should open the generated PNGs directly and compare them.

For quick temporary captures, use `/tmp/<run-name>` as the screenshot
directory. Keep committed/generated screenshots out of normal code changes
unless the user explicitly asks for them.

## Validation Commands

Run from the repository root:

```sh
cmake --build build
git diff --check
git -C engine diff --check
```

For docs-only changes, `git diff --check` is usually sufficient unless the
docs change command paths or runtime behavior claims.
