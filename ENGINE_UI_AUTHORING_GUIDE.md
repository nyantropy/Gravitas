# Engine UI Authoring Guide

This is the practical handbook for building retained UI on the Gravitas engine
runtime. It describes engine-level authoring rules only. Application-specific
feature names, business rules, content, and migration notes belong in the
application repository documentation.

The final engine-level v1.0 audit is recorded in
[ENGINE_UI_RUNTIME_V1_AUDIT.md](ENGINE_UI_RUNTIME_V1_AUDIT.md).

## Standard Model

New UI is authored in this order:

```text
UiSurface
-> UiLayer
-> UiMount
-> UiComposition
-> UiWidget / UiWidgetAsset
-> Retained UiNode
-> Renderer
```

The recommended unit of feature UI is a `UiComposition`.

Inside a composition, prefer reusable widgets and layout containers. Use
`UiTheme` style classes, metrics, typography, skins, and tokens for presentation
decisions. Receive user input through retained `UiEvent` values. Use bindings
for one-way data synchronization when the value has an observable source. Treat
retained handles and primitive nodes as implementation details.

## Authoring Path Classification

| Authoring style | Classification | Rule |
| --- | --- | --- |
| `UiComposition` mounted through `UiSystem` | Preferred | Every feature screen, panel, overlay, and child view starts here. |
| Reusable C++ widgets | Preferred | Widgets own reusable controls and interaction behavior. |
| Serialized widget assets | Preferred for reusable static structure | Use for reusable authored structure, tool-authored UI, and localized UI assets. |
| `UiTheme` style classes, skins, metrics, typography | Preferred | Colors, spacing, text scale, padding, and state visuals come from the theme. |
| Stack, Grid, Dock, Scroll, Constraint, Aspect layout | Preferred | Use these to express structure and flow. |
| Overlay layout | Preferred only for stacking peers | Use for layered regions, not for manual placement of every child. |
| Canvas and anchors | Compatibility | Use only for low-level bridging, full-fill, edge pinning, and legacy layouts. |
| Absolute/fixed normalized positions | Low-level exception | Allowed for primitive visualization, stage presentation, and projection adapters. |
| Raw `createNode`, `setPayload`, handle bundles | Low-level / legacy | Prefer widgets; use raw nodes only inside widgets, tools, tests, adapters, or primitive visualizations. |
| Direct payload colors/text scales | Legacy | Use theme style classes and widget setters/bindings instead. |
| Reading `dispatchResult()` and comparing handles | Compatibility | New UI handles events in `UiComposition::onEvent(...)` and widgets. |
| Manual spacing, row positions, per-frame layout mutation | Legacy / exception | Use layout containers, theme metrics, and stable layout slots. |
| `VNInteractionLayout` semantic descriptors | Preferred | Use named overlay/content slots, choice stack intent, and interaction/modal slots for VN and interaction frontend UI. |
| `VNLayoutProfile` coordinate rectangles | Compatibility | The runtime adapts these into `VNInteractionLayout`; do not add more coordinate rectangles. |

## Ownership Rules

### UiSurface

Owns a complete UI universe: document, layers, focus, modals, navigation,
drag/drop, animation, binding, accessibility, localization scope, active theme,
and input/render participation.

Use a new surface only for a separate output domain: editor preview, split
screen, tool surface, render target, or world-space projection surface.

Do not create a surface just to organize a feature panel.

### UiLayer

Owns coarse ordering and input blocking at the surface level.

Use layers for broad strata:

- base application chrome
- status overlays
- interaction frontend
- modal overlays
- tool chrome
- editor preview

Do not use layers for ordinary rows, tabs, cards, or child panels. Use layout
containers inside a mount.

### UiMount

Owns retained subtree lifetime and attachment.

Use a mount when a UI subtree needs to be attached, detached, rebuilt, or moved
as a unit. Feature coordinators should store `UiCompositionId` or `UiMountId`,
not random root handles.

Do not manually remove child handles owned by another mount.

### UiComposition

Owns feature UI structure, local UI state, bindings, and event translation.

A composition should:

- build its widget tree in `build(...)`.
- synchronize retained state in `update(...)`.
- forward retained events to widgets in `onEvent(...)`.
- emit feature commands rather than mutate domain rules directly.
- own child widget instances and implementation handles.
- attach child compositions through mounts when a child is a feature-owned view.

A composition should not:

- calculate button positions or row offsets.
- hardcode colors, font sizes, padding, or spacing.
- compare raw handles for ordinary button behavior.
- bypass widgets for controls that already exist.
- own domain rules such as pricing, eligibility, validation, or persistence.
- build competing fullscreen roots when a shell or slot exists.

### Widget

Owns a reusable retained subtree and control behavior.

Widgets:

- create retained nodes as an implementation detail.
- register navigation, drag sources, drop targets, semantics, and state visuals.
- consume events that belong to the control.
- expose semantic callbacks or command-oriented results.

Widgets do not own mounts, surfaces, feature state, domain rules, or renderer
resources.

### Retained Node

Retained nodes are low-level primitives. New feature code should rarely create
them directly. Use them inside widgets, widget assets, renderer/tool adapters,
primitive visualizations, and compatibility code.

## Layout Rules

The preferred layout model is container-first. A parent declares how children
flow; children declare sizing, alignment, and growth. Do not manually calculate
coordinates for ordinary UI.

### Stack

Purpose: one-dimensional flow.

Use for:

- vertical action lists
- option lists
- form rows
- setting rows
- detail panes
- property inspector fields
- notification lists

Do not use manual y offsets for rows that can be a stack.

### Grid

Purpose: regular two-dimensional placement.

Use for:

- uniform item cells
- icon tables
- fixed card collections
- tool palettes
- shortcut selectors

Do not use Grid for irregular masonry, arbitrary graph links, or projected
world-space labels.

### Dock

Purpose: reserve edges and fill remaining space.

Use for:

- window header/body/footer shells
- toolbar/content/status layouts
- split left/right/fill panes
- action footer rows

Do not use Dock for arbitrary absolute coordinates.

### Scroll

Purpose: clipped content larger than its viewport.

Use for:

- long text
- long lists
- inspectors
- logs
- asset lists

Scroll views own clipping and scroll offset. Child content should still use
Stack, Grid, Dock, or another layout container.

### Constraint

Purpose: size limits and responsive boundaries.

Use for:

- max-width panels
- min-size forms
- centered dialogs
- bounded tool panes

Do not use constraints to replace the parent layout model.

### Aspect

Purpose: preserve a fixed aspect ratio.

Use for:

- previews
- thumbnails
- minimaps or maps
- media frames

Do not manually recompute height from width when Aspect can express it.

### Overlay

Purpose: stack peers that share a region.

Use for:

- stage layers
- badges
- modal scrims
- prompt hosts
- floating overlays

Do not use Overlay as a replacement for Stack/Grid/Dock when children are
ordinary flow content.

### Canvas / Anchors

Purpose: compatibility and low-level placement.

Use for:

- full-surface roots.
- edge-pinned compatibility panels.
- projection adapters.
- primitive visualizations.
- legacy assets that cannot yet be migrated.

Do not use Canvas for new ordinary windows, forms, lists, or panels.

## Canonical Patterns

### Dialogue / Text Interaction

Composition:

- frontend shell owns stage, text panel, interaction slot, and modal slot.
- text panel content is dock/stack based: header, body, footer.
- choices are a vertical stack of buttons.

Layout:

- use `VNInteractionLayout` for semantic panel slots, content insets, choice
  stack intent, interaction slot, and modal slot.
- legacy `VNLayoutProfile` rectangles are compatibility seeds only.

### Menu

Composition:

- menu composition mounted into an appropriate layer.
- optional scrim panel.
- centered window.
- vertical action stack.
- secondary pages use the same shell with a scrollable body.

Layout:

- action lists are stacks.
- setting rows are dock or grid rows inside a scroll view.
- no manual row offsets.

### Settings

Composition:

- window shell.
- tab/section selector stack.
- scroll view for current section.
- each row is a setting widget: label + control + optional status.

Theme:

- row gap and section gap are metrics.
- labels and values use typography tokens.

### Collection / Grid View

Composition:

- standalone collection UI is a feature composition.
- embedded collection UI uses the same grid/list widget inside a parent shell.
- detail panel is dock/stack/scroll content.

Interaction:

- drag/drop registration belongs to the grid/list widget.
- move legality belongs to domain logic.
- status messages return through feature UI state.

### Transaction / Service View

Composition:

- mounted into an interaction frontend slot when it belongs to an ongoing
  interaction shell.
- window or child view, not a separate fullscreen overlay when a shell exists.
- list/grid widget for selectable entities.
- action stack for commands.
- status label bound or updated from feature UI state.

Ownership:

- domain systems own legality, pricing, persistence, and command execution.
- UI owns selection, focus, visible feedback, drag/drop event translation, and
  command emission.

### Record Log

Composition:

- window shell.
- record list stack or virtualized list widget.
- details scroll view.
- optional checklist/status widget.

Theme:

- status colors are semantic tokens.
- row spacing and selected-row styling are style classes.

### Tooltip

Composition:

- lightweight composition or widget asset mounted into an overlay layer.
- constraint layout aligned near target bounds.
- panel with text stack.

Rules:

- tooltips are not domain state.
- tooltip bounds are computed by the tooltip service or widget, not every
  feature composition.

### Popup / Modal

Composition:

- modal composition mounted into a modal layer or slot.
- optional scrim.
- focused content window.
- action buttons in a footer stack/dock.

Rules:

- use `UiModalManager` for blocking ownership.
- cancel/back routes through modal policy.
- focus restoration is runtime-owned.
- non-blocking interaction views are not modal.

### Status Overlay

Composition:

- overlay root mounted on a status layer.
- dock layout for edge regions.
- widgets for counters, bars, prompts, and notifications.
- primitive nodes only for visualizations that do not map to existing widgets.

Ownership:

- overlay observes feature state through adapters, events, or bindings.
- overlay does not own domain state.

### Tool Panel

Composition:

- editor/tool shell owns dockable panels.
- each panel is a composition.
- inspectors are scroll views of property rows.
- asset browsers are searchable collection views.

Rules:

- tools may use lower-level retained nodes for specialized editors.
- common chrome, commands, undo/redo, focus, and modal policy remain runtime
  responsibilities.

## Theme Rules

No feature should invent local color, spacing, typography, padding, or state
visual rules when a theme token can express the same intent.

| Concern | Source |
| --- | --- |
| Color | semantic color token or style class |
| Text scale/font/weight | typography token or style class |
| Spacing/gap | metric token |
| Padding/insets | metric token or panel skin content padding |
| Border radius / nine-slice | skin |
| Hover/pressed/disabled visuals | state style or state skin |
| Progress/selection/error visuals | style states and semantic tokens |

Recommended naming:

```text
Feature.Window
Feature.ActionButton
Feature.BodyText
feature.window.padding
feature.action.gap
```

Use compatibility payload values only when maintaining legacy retained nodes or
building a primitive visualization.

## Widget Rules

Use widgets for any reusable control or semantic UI unit.

Create a widget when:

- the same control appears in more than one composition.
- it owns interaction behavior.
- it owns navigation metadata.
- it owns drag/drop registration.
- it owns accessibility semantics.
- it hides retained-node implementation detail.

Do not create a widget when:

- the code is one composition-specific layout wrapper.
- no behavior or reuse exists.
- a stock widget already covers the behavior.

Every widget should expose:

- `build(...)`.
- `sync(...)` or explicit setters for mutable state.
- `onEvent(...)` when it consumes retained events.
- `root()` when the parent needs placement or navigation metadata.
- `content()` only when the widget intentionally exposes a child slot.
- callbacks or commands for semantic output.

## Composition Rules

Compositions may:

- declare structure.
- own feature UI state.
- bind data.
- respond to retained events.
- mount child compositions.
- emit commands to domain logic.
- request focus, navigation, modal, drag/drop, animation, binding, and
  accessibility services.

Compositions may not:

- implement hit testing.
- own pointer capture.
- route modal blocking manually.
- calculate ordinary layout geometry.
- hardcode presentation values.
- mutate domain state directly.
- compare raw handles for ordinary controls.
- interpret drag/drop payloads outside feature ownership.

## Interaction Rules

| Concern | Owner |
| --- | --- |
| Pointer hit testing | `UiInputDispatcher` |
| Event propagation | `UiSystem` / `UiComposition` |
| Keyboard/text focus | `UiFocusManager` |
| Navigation traversal | `UiNavigationGraph` |
| Modal blocking/cancel | `UiModalManager` |
| Drag lifecycle | `UiDragDropManager` |
| Domain command result | Application domain system |
| Widget activation behavior | Widget |
| Composition command translation | Composition |

New UI should not poll raw dispatch results for buttons. Widgets consume events,
and compositions translate widget callbacks into commands.

## Interaction Frontend Rules

The Interaction Frontend is the canonical shell for interaction-oriented
presentation. It owns:

- stage/background presentation.
- primary text panel.
- choice stack.
- interaction slot for non-modal child views.
- modal slot for blocking overlays.
- focus and navigation integration.

Future interaction features should integrate by:

1. Starting from the frontend session.
2. Reading the published interaction slot or modal slot.
3. Mounting a feature composition into the appropriate slot.
4. Emitting domain commands from UI events.
5. Returning visible status through feature UI state.
6. Leaving domain rules in the owning feature.

Examples of views that belong in the interaction slot:

- transaction views
- collection handoffs
- service screens
- relationship or profile screens
- application-specific notifications

Examples of views that belong in the modal slot:

- blocking confirmation
- blocking quantity/value input
- blocking error acknowledgement

## VN Interaction Layout Rule

`VNInteractionLayout` is the preferred authoring model for VN and
interaction-oriented frontend layout. It describes intent through named slots
and metrics: dialogue panel alignment and insets, panel content padding,
speaker/body/continue slots, choice stack width/alignment/gap/capacity,
interaction slot padding, modal slot padding, and sprite capacity.

New profiles should set `layoutAuthoring` to semantic and fill the
`interactionLayout` fields.

`VNLayoutProfile` remains supported only as a compatibility seed. The runtime
normalizes legacy coordinate rectangles through `resolveVNInteractionLayout(...)`
before building UI, so old content continues to render through the same semantic
path as new content.

Rules:

- Do not add new coordinate rectangles to `VNLayoutProfile`.
- Do not encode alignment, padding, content regions, and overlay slots in one
  rectangle.
- Use theme metrics for reusable spacing and typography decisions.
- Use the Interaction Frontend interaction slot for non-modal child views.
- Use the modal slot only for blocking confirmations or nested modal work.

## Anti-Patterns

Do not manually compute widget spacing.

Use Stack/Grid/Dock/Scroll and theme metrics. Manual offsets hide layout intent
and become inconsistent across surfaces.

Do not create fullscreen retained roots when a shell or slot exists.

Mount into the existing layer, shell, interaction slot, or modal slot.

Do not compare raw handles for ordinary button behavior.

Widgets and retained events already own activation.

Do not hardcode colors, font sizes, padding, or spacing.

Use theme style classes, typography, skins, and metrics.

Do not manually calculate button positions.

Use Stack or Dock for action groups.

Do not bypass widgets for controls that already exist.

Raw retained nodes make behavior, accessibility, navigation, and theme state
easy to omit.

Do not bypass layout.

Anchors and Canvas exist for compatibility and low-level escape hatches, not as
the normal authoring path.

Do not bypass themes.

Presentation consistency depends on semantic style requests.

Do not put domain rules in compositions.

Compositions emit commands; application systems validate and apply them.

## Migration Guidance

When touching old UI:

1. Keep behavior stable first.
2. Move ownership into a composition if it is still handle-bundle driven.
3. Replace raw controls with widgets.
4. Replace manual row math with layout containers.
5. Replace hardcoded presentation with theme tokens.
6. Replace handle comparisons with widget events and semantic commands.
7. Add navigation, modal, drag/drop, binding, and accessibility metadata only
   where the feature needs those behaviors.

Do not perform broad visual rewrites just to satisfy the ideal model. Migrate at
natural feature boundaries.

## Review Checklist

Before merging new UI, verify:

- Is it mounted through `UiSystem`?
- Is the root a composition?
- Are controls widgets or widget assets?
- Is ordinary layout container-driven?
- Are theme tokens used for presentation?
- Are retained events used instead of dispatch polling?
- Is navigation metadata present for focusable controls?
- Are modals actual modals?
- Are commands emitted to feature logic instead of applying domain behavior
  inside UI?
- Are raw nodes limited to widget internals, tests, tools, adapters, or
  primitive visualization?
