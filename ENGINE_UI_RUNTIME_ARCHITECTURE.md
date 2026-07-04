# Engine UI Runtime Architecture

This document describes the current Gravitas engine UI runtime. It is
engine-level only: it documents reusable runtime primitives, lifecycle,
ownership, and extension points. Application-specific feature names, content,
rules, and migration status belong in application documentation.

For practical authoring rules, use
[ENGINE_UI_AUTHORING_GUIDE.md](ENGINE_UI_AUTHORING_GUIDE.md).

For the engine-level v1.0 validation, use
[ENGINE_UI_RUNTIME_V1_AUDIT.md](ENGINE_UI_RUNTIME_V1_AUDIT.md).

## Runtime Hierarchy

The standard runtime hierarchy is:

```text
UiSurface
-> UiLayer
-> UiMount
-> UiComposition
-> UiWidget / UiWidgetAsset
-> Retained UiNode
-> Renderer
```

Each layer has one clear responsibility. Higher layers express feature
structure and behavior. Lower layers own retained primitives and rendering
extraction.

## UiSurface

`UiSurface` is the boundary for a retained UI universe.

A surface owns:

- a `UiDocument`
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

Use multiple surfaces for distinct output domains: editor previews, split
screen, render-target UI, tool surfaces, or projection surfaces. Do not create a
new surface merely to organize a panel.

## UiLayer

`UiLayer` owns coarse surface-local ordering and input participation.

Render traversal visits lower-order layers first. Hit testing walks input-enabled
layers in reverse order. Layer visibility and input participation are broad
strata controls; ordinary rows, panels, tabs, and cards should live inside
mounts and layout containers.

Layers never own domain vocabulary or feature rules.

## UiMount

`UiMount` owns retained subtree lifetime and attachment.

Mount destruction removes child mounts and prunes runtime state associated with
the subtree, including:

- focus and pointer ownership
- modal ownership
- navigation entries
- drag/drop registrations
- animations
- data bindings
- accessibility semantics
- asset consumer records

Feature coordinators should keep `UiMountId` or `UiCompositionId` values rather
than arbitrary retained handles.

## UiComposition

`UiComposition` is the retained UI authoring primitive.

A composition owns:

- feature UI structure
- local UI state
- child widget instances
- child composition mounting
- data bindings
- retained event handling
- command translation to application/domain systems

The composition lifecycle is:

```text
build -> update* -> onEvent* -> destroy
```

Compositions do not own surfaces, renderer resources, modal policy, pointer
capture, hit testing, focus traversal, or domain rules.

## Retained Events

`UiInputDispatcher` creates retained `UiEvent` values from frame input. `UiSystem`
resolves each event's surface, target path, layer, mount, and composition, then
delivers capture, target, and bubble phases.

`event.consume()` stops propagation.

`event.preventDefault()` blocks default behavior without stopping propagation.

New feature UI should handle interaction through `UiComposition::onEvent(...)`
and widgets. `dispatchResult()` remains a compatibility and tooling readback
path.

## Input, Focus, Navigation, And Modals

`UiInputDispatcher` owns per-frame pointer and navigation event creation.

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

These systems are surface-local. They never interpret application data.

## Layout Engine

The layout engine owns retained geometry through `UiLayoutSpec`,
measurement, arrangement, constraints, and layout containers.

Preferred containers:

- Stack for one-dimensional flow.
- Grid for regular two-dimensional placement.
- Dock for edge reservations and fill regions.
- Scroll for clipped overflowing content.
- Constraint for min/max/preferred bounds.
- Aspect for fixed-ratio regions.
- Overlay for layered peers in the same region.

Canvas and anchors remain compatibility and low-level paths for full-fill roots,
edge pinning, projection adapters, primitive visualizations, and legacy content.
They are not the preferred model for ordinary UI.

Hidden children do not participate in layout measurement or arrangement unless a
specialized container explicitly documents otherwise.

## Theme Runtime

`UiTheme` owns structured presentation:

- semantic colors
- metrics
- typography
- skins
- state styles
- style classes

Retained nodes request style through `UiStyle`. `UiDocument` resolves computed
presentation before layout, animation, and render extraction.

Compatibility payload values still exist for old retained nodes and primitive
visualizations. New widgets and compositions should request style classes,
typography, metrics, and skins.

## Widget Framework

`UiWidget` provides reusable composition-owned controls over retained nodes.

Widgets may:

- create retained node subtrees.
- request layout and style intent.
- register navigation metadata.
- register drag sources and drop targets.
- publish accessibility semantics.
- consume retained events.
- expose semantic callbacks or command objects.

Widgets do not own mounts, surfaces, feature business rules, modal policy,
renderer resources, or application state.

## Drag And Drop

`UiDragDropManager` owns direct manipulation lifecycle:

```text
armed -> dragging -> drop | cancel
```

It owns pointer capture, target resolution, drag event delivery, and typed opaque
payload transport. The runtime does not interpret payload meaning; application
or tool code owns that meaning.

## Animation Runtime

`UiAnimationManager` owns temporal retained presentation.

Animations target retained node properties, support interruption and
cancellation, and are pruned when nodes, mounts, layers, or surfaces are
destroyed. Animation applies before render extraction.

The animation runtime owns UI timeline mechanics, not widget behavior, focus,
modal policy, or domain state.

## Data Binding

`UiBindingManager` owns one-way synchronization from application/tool state to
retained UI properties.

Bindings observe explicit sources, format or compute values, and update retained
node properties before animation and render extraction. Bindings are cleaned by
source, node, mount, layer, and surface lifetime.

Bindings do not own application state and do not replace command/event flow.

## Accessibility Runtime

`UiAccessibilityManager` owns the surface-local semantic tree and announcement
stream.

Widgets and compositions describe roles, names, relationships, values, ranges,
and live regions. Focus, binding, mount, layer, and surface lifetime update or
prune semantic state automatically.

Platform screen reader bridges are adapters over this semantic model.

## Serialization, Widget Assets, Assets, And Packages

UI Serialization owns the durable data representation for retained UI
structure. Versioned assets can describe widget trees, layout intent,
style/theme references, bindings by application path, navigation metadata,
drag/drop metadata, animation timing hints, accessibility semantics, optional
surface descriptors, and layer descriptors.

`UiWidgetAssetRegistry` owns reusable authored widget definitions above
serialization. C++ widgets and compositions still own runtime behavior.

`UiAssetRuntime` tracks asset consumption and reload participation.

`UiPackageRuntime` groups authored UI assets and dependencies without changing
the composition model.

## Localization Runtime

`UiLocalizationRuntime` resolves localized strings from package/resource data.
Localized text may be consumed by serialized assets, widget assets, widgets, and
compositions. Refresh and workflow improvements are evolutionary work over the
same runtime ownership.

## Visual UI Editor

The visual editor is a tooling client of the same retained UI model. It authors
serialized UI/widget assets, previews them through normal runtime loading, and
uses runtime services for layout, theme, focus, navigation, drag/drop, binding,
accessibility, packages, and localization.

Editor UX can evolve without adding a separate UI architecture.

## VN And Interaction Frontend

The visual novel module is an engine-level presentation/runtime module.
Applications provide scripts, speaker identifiers, image asset paths, branching
labels, and application-specific commands.

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

`VNInteractionLayout` is the preferred semantic layout model. It describes named
overlay/content slots, content insets, choice stack intent, interaction slot,
modal slot, and sprite capacity.

`VNLayoutProfile` coordinate rectangles remain compatibility seeds. The runtime
derives `VNInteractionLayout` from those rectangles before building UI.

The Interaction Frontend is an engine presentation shell. Dialogue/text
presentation is one mode inside that shell. Applications may mount non-modal
child compositions into the published interaction slot and blocking child
compositions into the published modal slot. Application code owns business
logic, command effects, and domain meaning.

## Cleanup Rules

Destroying retained UI through node, widget, composition, mount, layer, or
surface paths must consistently remove:

- data bindings
- animations
- navigation entries
- focus state
- pointer capture
- drag/drop registrations
- accessibility semantics
- modal ownership
- asset consumer registrations

The runtime cleanup rule is ownership-based. A destroyed owner cannot leave
stale state in another UI subsystem.

## Compatibility Policy

Compatibility paths remain available so older UI and low-level tooling continue
to work. Compatibility paths must not become preferred authoring patterns.

Preferred:

- surfaces
- layers
- mounts
- compositions
- widgets/widget assets
- layout containers
- themes
- retained events
- navigation graph
- modal manager
- drag/drop manager
- data binding
- accessibility semantics
- serialization/assets/packages/localization

Compatibility:

- raw retained nodes outside widget/tool/adapter internals.
- canvas and anchors for ordinary UI.
- direct payload styling for ordinary controls.
- `dispatchResult()` polling for feature interaction.
- coordinate-driven VN layout authoring.

## Runtime Stability

The UI runtime is stable enough for future work to focus on:

- additional widgets.
- richer authored assets.
- editor workflow improvements.
- platform bridges.
- presentation extensions.
- localization workflow.
- performance profiling and optimization.

Reopening foundational runtime architecture should require evidence of a real
ownership, lifecycle, interaction, layout, presentation, or cleanup gap that
cannot be solved through existing primitives.
