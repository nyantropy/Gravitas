# Engine UI Runtime Architecture

This document describes the current Gravitas retained UI runtime. It is
engine-level only: application-specific feature names, content, rules, and
migration status belong in application documentation.

For practical authoring rules, use
[authoring-guide.md](authoring-guide.md).

## Runtime Hierarchy

The standard runtime hierarchy is:

```text
UiSurface
-> UiLayer
-> UiMount
-> UiComposition
-> UiWidget / UiWidgetAsset
-> retained UiNode
-> renderer
```

Higher layers express structure and behavior. Lower layers own retained
primitives and rendering extraction.

## Runtime Ownership

`UiSurface` is the boundary for a retained UI universe. It owns:

- `UiDocument`
- ordered `UiLayer` roots
- focus state
- modal state
- mount lifetime
- input dispatch
- active theme
- navigation graph
- drag/drop state
- animation timelines
- data bindings
- accessibility semantics
- localization scope
- coordinate conversion
- input and render participation

Use multiple surfaces for distinct output domains such as editor previews,
split screen, render-target UI, tool surfaces, or future projection surfaces.
Do not create a new surface merely to organize a panel.

`UiLayer` owns coarse surface-local ordering and input participation. Render
traversal visits lower-order layers first. Hit testing walks input-enabled
layers in reverse order. Ordinary rows, panels, tabs, and cards belong inside
mounts and layout containers, not separate layers.

`UiMount` owns retained subtree lifetime and attachment. Mount destruction
removes child mounts and prunes runtime state associated with the subtree:
focus/capture, modal ownership, navigation entries, drag/drop registrations,
animations, data bindings, accessibility semantics, and asset consumer records.

`UiComposition` is the retained UI authoring primitive. A composition owns
feature UI structure, local UI state, child widget instances, child composition
mounting, data bindings, retained event handling, and command translation to
application/tool systems. It does not own surfaces, renderer resources, modal
policy, pointer capture, hit testing, focus traversal, or domain rules.

`UiWidget` provides reusable composition-owned controls over retained nodes.
Widgets may create retained subtrees, request layout/style intent, register
navigation and drag/drop metadata, publish accessibility semantics, consume
retained events, and expose semantic callbacks. Widgets do not own mounts,
surfaces, feature business rules, modal policy, renderer resources, or
application state.

## Input, Focus, Navigation, And Modals

`UiInputDispatcher` creates retained `UiEvent` values from frame input.
`UiSystem` resolves each event's surface, target path, layer, mount, and
composition, then delivers capture, target, and bubble phases.

`event.consume()` stops propagation. `event.preventDefault()` blocks default
behavior without stopping propagation.

`UiFocusManager` owns persistent interaction state:

- pointer hover per pointer id
- pointer capture
- active pointer owner
- keyboard focus
- text-input focus
- navigation focus
- focus scopes
- focus restoration

`UiNavigationGraph` owns non-pointer traversal metadata and resolves abstract
move/submit/cancel requests into focus changes and activation events.

`UiModalManager` owns modal stack policy, layer blocking, cancel routing, and
focus-scope restoration.

These systems are surface-local and never interpret application data.

## Layout

The layout engine owns retained geometry through `UiLayoutSpec`, measurement,
arrangement, constraints, and layout containers.

Preferred containers:

- Stack for one-dimensional flow.
- Grid for regular two-dimensional placement.
- Dock for edge reservations and fill regions.
- Scroll for clipped overflowing content.
- Constraint for min/max/preferred bounds.
- Aspect for fixed-ratio regions.
- Overlay for layered peers in the same region.

Canvas and anchors remain compatibility and low-level paths for full-fill roots,
edge pinning, projection adapters, primitive visualizations, and coordinate
authored content.
They are not the preferred model for ordinary UI.

Hidden children do not participate in layout measurement or arrangement unless
a specialized container explicitly documents otherwise.

## Theme Runtime

`UiTheme` owns semantic colors, metrics, typography, skins, state styles, and
style classes. Retained nodes request style through `UiStyle`. `UiDocument`
resolves computed presentation before layout, animation, and render extraction.

Compatibility payload values still exist for old retained nodes and primitive
visualizations. New widgets and compositions should request style classes,
typography, metrics, and skins.

`UiPanelSkin` describes solid, image, or nine-slice panels plus tint, source
slice fractions, and content padding. Nine-slice UI rendering is implemented as
a retained UI primitive; the resolver emits clipped textured quads.

## Drag And Drop

`UiDragDropManager` owns direct manipulation lifecycle:

```text
armed -> dragging -> drop | cancel
```

It owns pointer capture, target resolution, drag event delivery, and typed
opaque payload transport. Runtime code does not interpret payload meaning;
application or tool code owns that meaning.

## Animation And Binding

`UiAnimationManager` owns temporal retained presentation. Animations target
retained node properties, support interruption and cancellation, and are pruned
when nodes, mounts, layers, or surfaces are destroyed.

`UiBindingManager` owns one-way synchronization from application/tool state to
retained UI properties. Bindings observe explicit sources, format or compute
values, and update retained node properties before animation and render
extraction. Bindings do not own application state and do not replace command
flow.

## Accessibility

`UiAccessibilityManager` owns the surface-local semantic tree and announcement
stream. Widgets and compositions describe roles, names, relationships, values,
ranges, and live regions. Platform screen reader bridges are future adapters
over this semantic model.

## Serialization, Assets, Packages, Localization

`UiSerializationRuntime` owns the durable JSON representation for retained UI
structure. It stores authored intent, not runtime handles, hover/capture state,
computed geometry, active animations, or C++ callbacks.

`UiWidgetAssetRegistry` owns reusable authored widget definitions.
`UiAssetRuntime` tracks asset consumption and live-reload participation.
`UiPackageRuntime` groups authored UI assets and dependencies without changing
the composition model.

`UiLocalizationRuntime` resolves localized strings from package/resource data.
Serialized assets, widget assets, widgets, and compositions may consume
localized text.

## Visual UI Editor And Tooling

The visual editor/tooling surface is a client of the same retained UI model. It
authors serialized UI/widget assets, previews them through normal runtime
loading, and uses runtime services for layout, theme, focus, navigation,
drag/drop, binding, accessibility, packages, and localization.

Editor UX can evolve without adding a second UI architecture.

## VN And Interaction Frontend

The visual novel module is an engine-level presentation/runtime module.
Applications provide scripts, speaker identifiers, image asset paths,
branching labels, and application-specific commands.

Engine-owned pieces:

- `VNStage`
- `VNRuntime`
- `VNScript`
- typed `VNCommand` data
- `VNCommandRegistry`
- `VNSystem`
- VN retained UI compositions
- `VNPresentationProfile`
- `VNInteractionLayout`
- compatibility `VNLayoutProfile` adaptation
- frontend interaction and modal slots

`VNInteractionLayout` is the preferred semantic layout model. It describes
named overlay/content slots, content insets, choice stack intent, interaction
slot, modal slot, and sprite capacity.

The Interaction Frontend is an engine presentation shell. Dialogue/text
presentation is one mode inside that shell. Applications may mount non-modal
child compositions into the published interaction slot and blocking child
compositions into the published modal slot. Application code owns business
logic, command effects, and domain meaning.

## Compatibility Policy

Preferred paths:

- surfaces, layers, mounts, compositions
- widgets and widget assets
- layout containers and themes
- retained events
- navigation graph, modal manager, drag/drop manager
- data binding, accessibility semantics
- serialization/assets/packages/localization

Compatibility paths:

- raw retained nodes inside widgets, renderer/tool adapters, tests, and
  primitive visualizations
- canvas and anchors for low-level bridging, fullscreen fill, edge pinning,
  projection adapters, primitive visualizations, and coordinate authored content
- `dispatchResult()` readback for tools, tests, and compatibility code
- `VNLayoutProfile` rectangles normalized into `VNInteractionLayout`

Historical debt that should not be copied:

- handle-bundle UI architecture outside widgets
- per-frame dispatch polling for button behavior
- fullscreen retained roots that bypass existing shells or slots
- domain systems mutating retained UI directly
- new coordinate rectangles for VN or interaction frontend layout
