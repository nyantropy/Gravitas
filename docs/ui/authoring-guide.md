# Engine UI Authoring Guide

This is the practical handbook for building retained UI on the Gravitas engine
runtime. Runtime ownership details live in [architecture.md](architecture.md).

## Standard Model

New UI is authored in this order:

```text
UiSurface
-> UiLayer
-> UiMount
-> UiComposition
-> UiWidget / UiWidgetAsset
-> retained UiNode
-> renderer
```

The recommended unit of feature UI is a `UiComposition`.

Inside a composition, prefer reusable widgets and layout containers. Use
`UiTheme` style classes, metrics, typography, skins, and tokens for
presentation decisions. Receive user input through retained `UiEvent` values.
Use bindings for one-way data synchronization when the value has an observable
source. Treat retained handles and primitive nodes as implementation details.

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
| `VNInteractionLayout` semantic descriptors | Preferred | Use named overlay/content slots, choice stack intent, and interaction/modal slots. |
| `VNLayoutProfile` coordinate rectangles | Compatibility | Runtime adapts these into `VNInteractionLayout`; do not add more coordinate rectangles. |

## Composition Rules

Compositions may:

- declare structure
- own feature UI state
- bind data
- respond to retained events
- mount child compositions
- emit commands to domain/tool logic
- request focus, navigation, modal, drag/drop, animation, binding, and
  accessibility services

Compositions may not:

- implement hit testing
- own pointer capture
- route modal blocking manually
- calculate ordinary layout geometry
- hardcode presentation values
- mutate domain state directly
- compare raw handles for ordinary controls
- interpret drag/drop payloads outside the owning feature/tool domain

## Layout Rules

The preferred layout model is container-first. A parent declares how children
flow; children declare sizing, alignment, and growth. Do not manually calculate
coordinates for ordinary UI.

- Use Stack for one-dimensional flow: action lists, option lists, form rows,
  setting rows, detail panes, property inspector fields, notification lists.
- Use Grid for regular two-dimensional placement: uniform cells, icon tables,
  fixed card collections, tool palettes.
- Use Dock for shells with reserved edges and fill regions: header/body/footer,
  toolbar/content/status, split panes.
- Use Scroll for clipped content larger than its viewport.
- Use Constraint for size limits and responsive boundaries.
- Use Aspect for fixed-ratio previews, thumbnails, minimaps, or media frames.
- Use Overlay for peers sharing a region: stage layers, badges, modal scrims,
  prompt hosts, floating overlays.
- Use Canvas/anchors only for full-fill roots, edge-pinned compatibility panels,
  projection adapters, primitive visualizations, or legacy assets.

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

Create a widget when:

- the same control appears in more than one composition
- it owns interaction behavior
- it owns navigation metadata
- it owns drag/drop registration
- it owns accessibility semantics
- it hides retained-node implementation detail

Do not create a widget when:

- the code is one composition-specific layout wrapper
- no behavior or reuse exists
- a stock widget already covers the behavior

Every widget should expose `build(...)`, `sync(...)` or explicit setters,
`onEvent(...)` when it consumes events, `root()` for placement/navigation, and
callbacks or commands for semantic output.

## Interaction Frontend Rules

The Interaction Frontend owns stage/background presentation, primary text
panel, choice stack, interaction slot, modal slot, focus, and navigation
integration.

Future interaction features should:

1. Start from the frontend session.
2. Read the published interaction slot or modal slot.
3. Mount a feature composition into the appropriate slot.
4. Emit domain commands from UI events.
5. Return visible status through feature UI state.
6. Leave domain rules in the owning feature.

Use the interaction slot for non-modal views such as transaction views,
collection handoffs, service screens, relationship/profile screens, and
application-specific notifications. Use the modal slot for blocking
confirmation, blocking quantity/value input, and blocking error acknowledgement.

## Anti-Patterns

- Do not manually compute widget spacing.
- Do not create fullscreen retained roots when a shell or slot exists.
- Do not compare raw handles for ordinary button behavior.
- Do not hardcode colors, font sizes, padding, or spacing.
- Do not bypass widgets for controls that already exist.
- Do not bypass layout or themes.
- Do not put domain rules in compositions.

## Migration Guidance

When touching old UI:

1. Keep behavior stable first.
2. Move ownership into a composition if it is handle-bundle driven.
3. Replace raw controls with widgets.
4. Replace manual row math with layout containers.
5. Replace hardcoded presentation with theme tokens.
6. Replace handle comparisons with widget events and semantic commands.
7. Add navigation, modal, drag/drop, binding, and accessibility metadata only
   where the feature needs those behaviors.

Do not perform broad visual rewrites just to satisfy the ideal model. Migrate
at natural feature boundaries.

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
- Are commands emitted to feature/tool logic instead of applying domain
  behavior inside UI?
- Are raw nodes limited to widget internals, tests, tools, adapters, or
  primitive visualization?
