# Engine UI Runtime Architecture Investigation

## Status

This document records the UI runtime investigation performed against the current
engine and game code. It also records the recommended long-term architecture and
the staged migration plan.

The investigation found that the current UI runtime is useful as a retained
primitive renderer, but it is not a complete UI runtime architecture for a
general-purpose engine. The engine needs first-class concepts for surfaces,
layers, composition, centralized input dispatch, focus, modal ownership, richer
layout, styling, animation, data binding, and semantic accessibility.

The first foundational primitive has been implemented during this investigation:
`UiDocument` now owns explicit ordered layers. Existing builders still attach to
the default layer root, while newer systems can create named layers and attach UI
trees below those layer roots.

The second foundational primitive is also now implemented: `UiInputDispatcher`
performs one retained UI dispatch per engine frame, and retained UI feature
systems read `ctx.ui->dispatchResult()` instead of initiating their own document
hit test.

The third foundational primitive is now implemented: `UiFocusManager` owns
persistent retained UI focus state, while `UiInputDispatcher` is responsible for
dispatching the frame's input and publishing the frame result.

The fourth foundational primitive is now implemented: `UiModalManager` owns the
retained UI modal stack, layer blocking policy, cancel routing, and focus-scope
restoration policy.

The fifth foundational primitive is now implemented: `UiMount` owns retained UI
subtree lifetime and attachment, and `UiSystem` cleans focus, pointer capture,
modal ownership, text bindings, and child mounts when a mount is destroyed.

The sixth foundational primitive is now implemented: `UiComposition` owns
retained UI authoring and feature-local UI runtime state while `UiMount` owns
lifetime.

The seventh foundational primitive is now implemented: retained `UiEvent`
propagation routes dispatcher-created events through capture, target, and bubble
phases to `UiComposition::onEvent(...)`.

The eighth foundational primitive is now implemented: `UiSurface` owns a
surface-local UI universe: coordinate conversion, document, layers, focus,
modal state, mounts, dispatcher, and input/render participation.

The ninth foundational primitive is now implemented: the retained layout engine
owns surface-local UI geometry through measurement, arrangement, typed units,
constraints, and reusable layout containers while preserving anchored layout as
the compatibility canvas path.

The tenth foundational primitive is now implemented: `UiTheme` owns structured
presentation data for semantic colors, metrics, typography, skins, state
styles, and style classes. Each `UiSurface` owns its active theme, and
`UiDocument` resolves computed presentation during layout measurement and visual
list rebuild.

The eleventh foundational primitive is now implemented: the Widget Framework
provides reusable composition-owned controls that build retained node subtrees,
consume retained events, request layout and theme intent, and expose semantic
callbacks without owning mounts, surfaces, focus, modal policy, or rendering.

The twelfth foundational primitive is now implemented: `UiNavigationGraph`
owns surface-local non-pointer traversal between focusable UI controls. Widgets
declare navigation metadata, the graph resolves move/submit requests, the focus
manager owns the resulting focus, and retained events deliver navigation
activation to compositions and widgets.

The thirteenth foundational primitive is now implemented: `UiDragDropManager`
owns surface-local direct manipulation. Widgets and compatibility builders
register drag sources and drop targets with typed opaque payloads; the drag
runtime owns armed/dragging/drop/cancel lifecycle, pointer capture, target
resolution, and retained drag event delivery.

The fourteenth foundational primitive is now implemented: `UiAnimationManager`
owns surface-local temporal presentation. The UI runtime consumes the existing
engine tween easing primitives, owns retained node property timelines,
interrupts/cancels animations by node/property, prunes destroyed subtrees, and
applies animated style, payload, and layout values before rendering extracts the
current frame.

The fifteenth foundational primitive is now implemented: `UiBindingManager`
owns surface-local one-way synchronization relationships between
application-owned state and retained UI properties. Bindings observe explicit
sources, support formatting and computed values, update retained node
presentation/layout/state targets before animation and rendering, and are
cleaned up by source, node, mount, layer, and surface lifetime.

The sixteenth foundational primitive is now implemented:
`UiAccessibilityManager` owns the surface-local semantic tree and announcement
stream. Widgets describe roles, names, relationships, values, ranges, and live
regions; focus, binding, mount, layer, and surface lifetime update or prune
semantic state automatically. Platform screen reader bridges remain future
adapters over this engine-owned semantic model.

The seventeenth foundational primitive is now implemented: UI Serialization
owns the durable data representation for retained UI structure. Versioned JSON
assets can describe widget trees, layout intent, style/theme references,
bindings by application path, navigation metadata, drag/drop metadata,
animation timing hints, accessibility semantics, optional surface descriptors,
and layer descriptors. The loader instantiates that data into an existing
mount while C++ remains responsible for behavior and binding source resolution.

The eighteenth foundational primitive is now implemented: Widget Asset
Definitions own reusable authored UI building blocks. `UiWidgetAssetRegistry`
stores versioned widget assets, resolves base assets and variants, substitutes
parameters, fills named slots, expands nested asset references into ordinary
serialized widget trees, and then lets the serialization runtime instantiate
the resolved graph. Assets remain authored data; runtime widgets and
compositions still own behavior.

## 1. Current Architecture

### Runtime Summary

The retained UI runtime is currently centered on one `UiSystem` instance owned by
the rendering runtime. `UiSystem` is now a surface-owning facade. It owns one
default screen `UiSurface` for compatibility and can create additional surfaces.
Each `UiSurface` owns one `UiDocument`, one layer stack, one
`UiInputDispatcher`, one `UiFocusManager`, one `UiModalManager`, one
`UiMountManager`, one `UiNavigationGraph`, one `UiDragDropManager`, and one
`UiAnimationManager`, one `UiBindingManager`, one `UiAccessibilityManager`, and
one active `UiTheme`. `UiSystem` also owns one `UiWidgetAssetRegistry` for
authored reusable widget assets.
Renderer-side font bindings and command caches are tracked per surface because
`UiHandle` values remain
document-local.

The default screen surface preserves the old normalized `0..1` coordinate space.
Additional surfaces declare a normalized output rect; screen input is converted
into that surface's local `0..1` coordinate space before dispatch. Rendering
extracts commands per visible/render-enabled surface and transforms the
surface-local command vertices into the surface rect before submitting the
combined screen-space UI command buffer.

The render path is:

1. Game or engine systems mutate retained nodes through `UiSystem`.
2. `RenderingRuntime` asks `UiSystem` to extract commands for the output size.
3. `UiSystem` walks render-enabled surfaces in surface order, updates each
   dirty surface document, rebuilds its visual list if dirty, resolves it into
   a surface-local `UiCommandBuffer`, and appends it into the frame UI buffer.
4. The Vulkan UI render stage draws that command buffer as an overlay after the
   scene and particle passes.

The retained input path is now centralized once per engine frame.
`RenderingRuntime::dispatchUiInput(...)` builds a screen-normalized
`UiInputFrame` from `InputBindingRegistry`, calls `UiSystem::dispatchInput(...)`,
and stores a `UiDispatchResult` for compatibility systems to read. `UiSystem`
selects the top ordered input-enabled surface under the pointer, or the surface
that already owns pointer/cancel interaction, converts the frame to surface-local
coordinates, and dispatches through that surface's dispatcher. The dispatcher
consults that surface's `UiModalManager` before hit testing so the top modal can
block lower layers and route cancel/back input. Inventory and merchant grid
interactions still compute cell ownership manually because they are not yet
modeled as retained UI widgets.

The retained authoring path now has one more layer above primitive nodes:
`UiComposition` can build reusable widgets such as labels, panels, buttons,
stacks, separators, scroll containers, images, spacers, and progress bars.
Widgets are clients of the runtime. They own their retained subtree and local
interaction state, but their lifetime is still owned by the enclosing
composition and mount. They receive retained `UiEvent` values through
composition forwarding and expose semantic behavior such as button pressed
callbacks so feature code no longer needs to compare raw handles for every
control.

The retained non-pointer path is now graph-driven. `UiInputFrame` carries
abstract navigation requests (`Up`, `Down`, `Left`, `Right`, `Next`,
`Previous`, and `Submit`). The selected surface's `UiNavigationGraph` resolves
those requests against registered focusable nodes, explicit neighbor overrides,
computed layout bounds, tab indexes, groups, modal ownership, and node
visibility/enabled state. It asks `UiFocusManager` to apply focus; it does not
own focus itself. The dispatcher then emits retained `NavigationMove`,
`NavigationSubmit`, `NavigationWrap`, and `NavigationBlocked` events through
the normal capture/target/bubble pipeline.

The retained direct-manipulation path is now drag/drop driven. Widgets or
compatibility builders register drag sources with `UiDragPayload` values and
drop targets with accepted payload types. The selected surface's
`UiDragDropManager` arms a drag on pointer press, starts it after the source's
threshold, captures the pointer through `UiFocusManager`, resolves compatible
drop targets from the hit-tested path, suppresses click/navigation competition,
and emits `DragStart`, `DragMove`, `DragEnter`, `DragLeave`, `DragOver`,
`DragDrop`, `DragAccept`, `DragReject`, `DragCancel`, and `DragEnd` events.

The retained animation path is now centralized. Widgets, compositions, modal
descriptors, and compatibility code request target presentation state through
`UiSystem::animate(...)`, `animateOpacity(...)`, or `transitionStyleState(...)`.
Each surface's `UiAnimationManager` owns active property timelines, uses the
existing `gts::tween` easing/progress primitives, applies computed values to
retained node style/payload/layout properties, and cancels timelines
automatically when nodes, mounts, layers, or surfaces disappear.
`RenderingRuntime::renderFrame(...)` advances UI animations once per frame
before command extraction, so rendering consumes already-computed presentation.

The retained synchronization path is now centralized. Applications can expose
state through `UiObservable<T>` or custom `UiBindingSource` callbacks, while
compositions and widgets bind those sources to retained properties such as text,
visibility, enabled state, opacity, progress, primitive colors/tints, style
classes, and layout values. `RenderingRuntime::renderFrame(...)` updates UI
bindings once per frame before animation, layout, and command extraction, so
bindings establish desired state and the animation runtime interpolates
animatable changes.

The retained semantic path is now centralized. Widgets and compatibility code
register `UiSemanticDesc` data with the selected surface's
`UiAccessibilityManager`. The accessibility manager builds a semantic tree that
references retained nodes but is independent from render primitives, resolving
roles, names, descriptions, hints, values, ranges, relationships, live-region
policy, enabled/hidden/focused state, and bounds. Focus changes and binding
updates feed the announcement queue, while node, mount, layer, surface, and
document cleanup prune stale semantic state automatically. No OS accessibility
API is implemented yet; future Windows UIA, macOS Accessibility, Linux ATK,
web, VR, testing, and automation adapters should consume this engine semantic
model.

The retained persistence path is now centralized. `UiSerializedAsset` is a
versioned JSON asset model for authored retained UI intent. It can represent
widget type/id hierarchy, layout, style and theme references, default text/image
and progress values, binding paths, navigation metadata, drag/drop metadata,
animation timing hints, accessibility semantics, optional surface descriptors,
and layer descriptors. It deliberately does not serialize handles, focus,
hover, capture, computed geometry, active animations, announcements, callbacks,
or application objects. `UiSystem::instantiateUiAsset(...)` validates an asset,
instantiates it into a mount, and returns stable id-to-handle mappings for C++
behavior attachment.

### UiSystem

`UiSystem` is the render-facing facade over `UiSurface`. It exposes legacy
default-surface node creation, mutation, removal, layout/style/state/payload
setters, text font bindings, measurement, interaction, and command extraction,
plus surface-aware overloads for new runtime code.

Current responsibilities:

- Owns the surface registry and stable ordered surface list.
- Owns renderer-side text bindings by `(surface, UiHandle)`.
- Owns per-surface command buffer caches plus the combined extraction buffer.
- Delegates document, dispatcher, focus, modal, mount, navigation, drag/drop,
  animation, binding, and accessibility ownership to each `UiSurface`.
- Exposes the latest dispatch result.
- Exposes semantic tree and accessibility announcement APIs for runtime,
  tooling, and future platform adapters.
- Exposes serialized UI asset instantiation into existing mounts.
- Converts retained visual lists into renderer command data per surface.

`UiSystem::dispatchInput(...)` is the retained input entry point. It:

- Selects an input surface by surface order, visibility, input participation,
  pointer containment, active pointer ownership, and cancel ownership.
- Converts the frame into the chosen surface's local coordinates.
- Forces that surface document layout into normalized `1.0 x 1.0` space.
- Delegates to the chosen surface's `UiInputDispatcher`.
- Lets the dispatcher run that surface document's `UiDocument::hitTest(...)`.
- Lets that surface's `UiFocusManager` own hover, capture, active pointer, and
  focus state.
- Lets that surface's `UiModalManager` constrain hit testing and cancel routing.
- Records the owning layer for hovered, focused, pressed, released, clicked,
  active, and captured handles.
- Records modal ownership, blocking, and cancel/dismissal state.
- Publishes the frame's `UiDispatchResult`.

`UiSystem::updateInteraction(...)` remains as a compatibility helper over the
dispatcher.

Missing from `UiSystem` today:

- Dedicated render-target, world-space, and multi-window extraction backends.
- Per-surface keyboard/text event routing and physical controller-device mapping
  beyond the current cancel/navigation intent fields.
- Held-navigation repeat policy.
- Full multi-pointer dispatch. Focus state is already keyed by pointer id, but
  `UiInputFrame` still carries only the primary pointer.
- Mature layout services such as per-node invalidation boundaries, two-pass
  constrained text wrapping, and virtualized scroll content.
- First-class animation graph authoring and delayed modal/mount teardown after
  exit transitions.
- Two-way data editing, reflected ECS/property binding, and reusable widget
  asset definitions. Current data binding is intentionally one-way and
  runtime-owned.
- Platform accessibility bridges, voice/speech integration, automation tooling,
  and persisted semantic/widget asset authoring.

### UiDocument

`UiDocument` owns all retained nodes in an unordered map keyed by `UiHandle`.
Handles are plain `uint32_t` values. The allocator increments monotonically until
`clear()`, which resets handle numbering. There is no generation number, so a
stale handle from before a clear can numerically collide with a later node.

Current document ownership model:

- The document owns every `UiNode`.
- Each node stores its parent handle and child handles.
- The hierarchy is stored as child vectors.
- Removal is recursive.
- Reparenting detaches from the old parent and appends to the new parent.
- The document now has a hidden document root and one or more layer roots.
- `UiSystem::getRoot()` returns the default layer root for compatibility.
- `createNode(..., UI_INVALID_HANDLE)` attaches to the default layer root.

Current layer model:

- `UiLayerId` is a `uint32_t`.
- `UI_DEFAULT_LAYER` is created during `UiDocument::clear()`.
- Each layer owns a full-document container root.
- Layer roots are sorted by ascending `UiLayer::order`, then by layer id.
- Render traversal visits lower-order layers first.
- Hit testing walks children in reverse traversal order, so higher-order layers
  are tested first.
- Layer visibility maps to the layer root `visible` state.
- Layer input participation maps to the layer root `enabled` state.

This is a useful first primitive, but it is still document-local. It is not yet a
surface system and not yet a full input router. Modal policy lives above the
document in `UiModalManager`, not in layer roots themselves.

### Node Hierarchy

`UiNode` contains:

- `handle`
- `type`
- `parent`
- `children`
- authoring `layout`
- computed layout
- `style`
- state flags
- typed payload variant

Node payloads are primitive-specific:

- Container
- Rect
- Image
- NineSlice
- Text
- Grid
- Line
- Circle

The node tree is both the logical tree and the render tree. There is no separate
semantic tree, accessibility tree, focus tree, or composition tree.

### Lifetime

Raw node lifetime is still handle-based, but the engine now has a first
ownership primitive above raw handles. `UiMount` owns a retained subtree root,
its attachment location, parent/child mount relationships, and automatic cleanup
when the mount is destroyed.

Feature UI builders still store handle structs for compatibility. Feature
sync/event systems later mutate those handles. Those handle structs should
migrate behind mounts incrementally; the mount becomes the lifetime owner while
feature state remains free to cache interior handles for sync.

`UiSystem::clear()` clears the whole document, text bindings, focus, modal
state, dispatch state, and recreates the root mount. Individual nodes can still
be removed recursively for compatibility. If a removed node is an exact mount
root, `UiSystem::removeNode(...)` destroys the mount instead. Layer roots and
the hidden document root cannot be removed through `removeNode`.

Destroying a mount removes the retained subtree, destroys child mounts, clears
focus, hover, pointer capture, active pointer, keyboard/text/navigation focus,
prunes modal ownership, and removes text bindings under the subtree.

Remaining lifetime limitations:

- Existing feature builders are not yet authored as mounted compositions.
- Raw handles are not generation checked.
- Node-local callback registration and widget lifecycle hooks are future layers
  on top of composition and event propagation.

### Coordinate Systems

The retained UI document operates in normalized screen coordinates. A node's
computed layout bounds are usually in `0..1` document space. The command resolver
uses the output viewport width and height when converting primitives into
vertices.

Existing game UI code frequently computes pixel layouts manually, then converts
them to normalized offsets and fixed sizes before writing `UiLayoutSpec`.

Current coordinate model and limitations:

- `UiLayoutSpec` now has typed layout lengths for normalized values, parent
  percentages, surface width/height, content, em, and preliminary pixel units.
- The first pixel-unit implementation is normalized against the surface axis;
  true DPI-aware physical pixel scaling remains future work.
- Parent-relative layout is available through constraints and containers, while
  legacy anchors remain supported by `Canvas`.
- No world-space or render-target-local coordinate domains.
- No surface transforms for terminals, monitors, split-screen, VR, or AR.

### Layout

`UiLayoutSpec` is now both the compatibility canvas layout descriptor and the
first retained layout container descriptor. Existing absolute and anchored
fields remain supported through `UiLayoutMode::Canvas`, preserving old builders
that directly set offsets, anchors, fixed sizes, margins, padding, clip mode,
and `contentOffset`.

The document update path is now:

1. Measure every visible subtree.
2. Arrange every subtree from the surface root down.
3. Store computed bounds, content rect, clip rect, and measured size on each
   `UiNode`.
4. Rebuild the visual list from computed geometry.

Implemented layout modes:

- `Canvas`: compatibility absolute/anchored placement.
- `Stack`: vertical or horizontal flow with gap, main-axis alignment,
  cross-axis stretch, and grow distribution.
- `Grid`: equal row/column grid with gaps, cell coordinates, and spans.
- `Dock`: top/bottom/left/right/fill panel arrangement.
- `Overlay`: children share the parent content rect and use constraints for
  alignment and size.
- `Scroll`: canvas arrangement with forced clipping and `contentOffset`.
- `Aspect`: overlay-style arrangement with aspect-ratio constraints.
- `Constraint`: children resolve preferred/min/max size and alignment inside
  the parent content rect.

Implemented constraints include preferred size, min/max size, grow/shrink,
aspect ratio, horizontal/vertical alignment, margin, padding, and gaps. Text,
image, and grid payloads contribute intrinsic measurement. Rendering extraction
passes a font-backed text measurement callback so text nodes can participate in
content sizing; input dispatch uses the same layout pass with approximate text
measurement when render extraction has not run yet.

Current layout limitations:

- Layout invalidation is document-level dirty-flag based, not yet a per-subtree
  invalidation graph.
- Text wrapping is measured before final arranged width, so constrained
  re-measurement for width-dependent wrapping is still future work.
- Scroll layout clips and offsets content but does not yet own scrollbars,
  scroll ranges, virtualization, or input gestures.
- Pixel units are preliminary and not yet DPI-aware.
- Existing game UI is still mostly manually positioned and must migrate
  incrementally.

### Rendering

Rendering is retained-tree flattening followed by command-buffer resolution.

`UiDocument::rebuildVisualList()` recursively visits visible nodes and appends
visual primitives for drawable node types. Containers do not emit primitives.
The current traversal order is:

1. Hidden document root.
2. Layer roots from lowest order to highest order.
3. Each node before its children.
4. Siblings in child vector order.

Before explicit layers were added, top-level ordering was effectively root child
insertion order. That meant the render stack could change based on feature build
order or lazy creation. The visual novel UI, for example, was built lazily and
therefore tended to render above earlier dungeon UI roots. Engine tools update
after the active scene and also append tool UI after scene UI.

With layers, ordering can now be explicit at the first root level, but most game
UI still uses the default layer until it migrates.

Clipping is implemented through a computed clip rectangle:

- Nodes inherit the parent clip.
- `UiClipMode::ClipChildren` intersects the inherited clip with the node bounds.
- Visual primitives carry the effective clip.
- The command resolver CPU-clips most rectangular primitives.
- Rotated images are only bounds-tested against the clip before emitting a
  rotated quad.

There is no render batching by layer policy beyond command merging of adjacent
compatible commands. There is no per-layer render target, transition, stencil,
depth policy, or world-space integration.

### Hit Testing

Hit testing is recursive and document-local.

Current traversal:

1. Start at the hidden document root.
2. Reject invisible or disabled nodes.
3. If the node clips children and the point is outside its clip rect, reject the
   subtree.
4. Traverse children in reverse order.
5. Return the first child hit.
6. If no child hits, test the current node's bounds.
7. Return the node if it is interactable.

Circle nodes use circular hit testing against their bounds. Other node types use
rectangle testing.

Current hit test limitations:

- No event target path is returned.
- No capture phase.
- No bubble phase.
- No event consumption.
- No handler-owned propagation path.
- No surface priority.
- No hit-test path object for parent/child event routing.
- No separation between "visible", "hit test visible", and "receives input".

### Interaction

Interaction is currently centralized pointer dispatch plus a result struct:

- `hovered`
- `focused`
- `pressed`
- `released`
- `clicked`
- `active`
- `captured`
- owning layers
- pointer coordinates
- scroll deltas
- consumed flags
- modal ownership, modal depth, modal owner, and modal layer
- pointer, keyboard, navigation, and text-input blocking flags
- cancel target and dismissed modal for cancel/back input

Explicit retained events now exist for pointer enter/leave/down/up/click/move,
wheel, navigation move/submit/cancel, focus gained/lost, and drag/drop
lifecycle. Keyboard key events, text input, and broader lifecycle event delivery
are still future extensions of the same `UiEvent` route.

Retained interaction ownership is no longer initiated by feature systems.
VN choices now receive composition events. Menus, merchant buttons, skill-tree
nodes, and engine tool widgets still read `ctx.ui->dispatchResult()` during
migration. Manual inventory-grid, merchant-grid, and tool-world pointer ownership
still bypass retained dispatch.

### Keyboard, Focus, and Navigation

Keyboard input is currently handled outside the UI runtime through
`InputBindingRegistry` and feature-specific code. `UiFocusManager` now owns
keyboard focus, text-input focus, navigation focus, pointer hover, pointer
capture, active pointer, focus scopes, and focus restoration. Primary pointer
press requests keyboard focus through the focus manager. Abstract navigation
requests now route through each surface's `UiNavigationGraph`; raw key events,
text input events, physical controller devices, and held-repeat policy are still
future routing work.

Missing:

- Keyboard event routing to the focused owner.
- Text input routing to the text-input owner.
- Physical gamepad/controller mapping into navigation intent.
- Held-navigation repeat policy.
- Event-level modal focus trap.
- Focus lost/gained events.

### Styling

`UiStyle` is now the retained node's style request rather than the complete
presentation payload. A node may name a `styleClass`, request an explicit state
override, or provide explicit compatibility overrides for background color,
foreground color, and opacity.

`UiTheme` is the surface-local presentation data source. It owns:

- semantic color tokens.
- named metrics.
- named typography.
- named skins.
- named panel state skins.
- style classes with optional parent classes.
- state-specific style properties for normal, hover, pressed, focused,
  selected, disabled, active, checked, expanded, and error states.
- parent-theme inheritance.

`UiDocument::updateLayout(...)` receives the active theme so text measurement can
use resolved typography. `UiDocument::rebuildVisualList(...)` receives the same
theme so rect, image, nine-slice, text, line, and circle primitives can be
emitted from computed style. Legacy payload-local colors, text scales, image
tints, and panel skins still work when no style class or explicit style override
is present.

Higher-level systems such as VN presentation profiles, engine tool themes,
menus, inventory, merchant UI, and HUDs still contain feature-local styling.
Those are now migration targets, not separate engine styling models.

### Animation

`UiAnimationManager` is now the generic retained UI animation runtime. It is
surface-local and owns active property timelines for retained node style,
payload, and layout values. It consumes the shared `gts::tween` primitives for
easing/progress and applies interpolated presentation back to retained nodes
before rendering extracts the current frame.

VN stage tweens remain a domain-specific stage presentation system. They are not
the retained UI animation owner, but they share the same underlying tween
primitive.

Current retained UI animation covers opacity, colors/tints, layout offsets,
anchors, fixed size, content offset, theme-driven style-state transitions,
widget state hooks, and modal owner open/close hooks. It does not yet provide
animation graph authoring, reduced-motion policy, delayed modal/mount teardown,
or serialized animation assets.

### Data Binding

`UiBindingManager` is now the generic retained UI synchronization runtime. It is
surface-local and owns one-way relationships from application-owned state to
retained UI targets. Bindings are not widgets, not gameplay state, and not an
MVVM framework. They describe relationships such as:

- observable text -> label text.
- integer value + formatter -> label text.
- computed current/max value -> progress fill layout.
- boolean value -> retained visibility or enabled state.
- status value -> style class or presentation color.
- boolean value -> opacity transition.

The observable model is intentionally small. `UiObservable<T>` provides a
versioned value wrapper for simple runtime state, while `UiBindingSource` lets
applications provide custom read, revision, and lifetime callbacks for ECS,
editor, or game-owned data. Computed and multi-source bindings are represented
by custom source callbacks plus a combined revision function. Formatting and
transforms live on the binding descriptor rather than in widgets.

Binding update order is:

1. Application systems change their own state.
2. `UiBindingManager` evaluates changed sources.
3. The manager applies retained target properties.
4. `UiAnimationManager` interpolates animatable changes.
5. Layout and rendering consume retained state.

Bindings are cleaned by ownership boundaries. Destroying a node, mount, layer,
surface, or document removes bindings under that subtree. Shared observable
sources can also expire and be pruned automatically. Widgets expose convenience
methods such as label text binding and progress value binding, but the binding
target remains the retained node so bindings survive ordinary widget authoring
patterns without giving widgets ownership of runtime state.

Current binding limitations:

- One-way only. Text editing, sliders, property editors, and IME input will need
  explicit future two-way policies.
- No reflected ECS/property schema yet.
- No binding graph optimizer beyond source revision checks and value equality.
- No serialized binding declarations or widget asset definitions.
- No accessibility announcement queue yet; binding changes provide the future
  signal source for that runtime.

### Visual Novel, Dialogue, and Choices

The dialogue module is a generic graph runtime. It owns graph state, node
advance, visible choices, condition/action registries, and dialogue events. It
does not render UI.

The visual novel module is a presentation module above retained UI. It lazily
builds `VNDialogueUi`, mirrors active dialogue runtime state into VN presentation
state, and routes choice clicks or continue input back to the dialogue runtime.
VN stage data includes background mode, dimming, sprites, sprite z-order, and
tweened stage motion.

VN UI currently reads the centralized dispatch result for choice clicks. Because
it is built lazily and still uses the default screen document, it should move to
an explicit VN/dialogue layer during a later migration.

### Merchant UI

Merchant UI is game-owned and feature-local under `src/inventory/ui/`. It builds
a retained full-screen scrim/window and many retained handles for cells, item
icons, labels, buttons, and text.

Merchant UI reads centralized dispatch results for retained buttons and scrim
clicks, but still computes grid cell hover/selection behavior manually from its
own pixel layout. Merchant open/closed state is stored in a game singleton
component. Merchant input blocking is handled by `DungeonInputGate`, not by a
generic modal system.

### Inventory UI

Inventory UI is also game-owned under `src/inventory/ui/`. It computes a pixel
panel layout and writes normalized retained nodes for display. Drag/drop,
hovered cell, item footprint, rotation, and placement behavior are computed
manually from input and inventory layout data.

Inventory UI does not participate fully in engine retained hit testing. It uses
feature-specific ownership and feature-specific input routing.

### Menus

Game menus are feature-local under `src/menus/ui/`. Menu state is stored in
`GameMenuStateComponent`. Menu event handling uses both semantic input actions
and centralized retained UI dispatch. Menu open state is also part of
`DungeonInputGate` input blocking.

The skill tree is opened through menu state but owns a separate UI root and a
separate event handler. It uses retained line/circle/rect/text primitives and a
clipped viewport with `contentOffset` for panning.

### HUD and Prompts

Dungeon HUD and interaction prompt UI are retained roots built by
`DungeonUiController`. They mostly sync state to retained nodes and do not own
global input dispatch. They coexist with overlays by build order today unless
their roots are hidden by feature sync code.

### Debug and Tool UI

Engine tools are global development UI systems that run after the active scene's
controller systems. They build retained UI using tool widgets and read
centralized dispatch results. Tool viewport and world input capture is
represented by tool-specific singleton components, not by a generic UI capture
primitive.

Tool UI currently benefits from update/build timing: it is produced after scene
UI, which tends to put it above scene UI in the shared document unless explicit
layers are used.

## 2. Architectural Review

### Strengths

The current retained runtime has real value:

- Simple retained handles are easy for game systems to store and mutate.
- Dirty flags avoid rebuilding layout and visuals every time if nothing changed.
- Command-buffer caching reduces steady-state render cost.
- The primitive set covers useful basics: rect, image, nine-slice, text, grid,
  line, and circle.
- Text measurement, wrapping, alignment, and max lines exist.
- Clipping and content offset make scroll/pan widgets possible.
- The engine/game boundary is mostly generic at the primitive level.
- The dialogue module correctly separates graph runtime from presentation.
- Feature-owned UI folders keep game concepts mostly out of engine core.
- The new layer primitive removes the worst dependence on top-level insertion
  order for future UI roots.

### Weaknesses

The main weakness is that the engine has a retained primitive tree, not a full UI
runtime.

Specific weaknesses:

- The renderer currently composites all render-enabled surfaces into one
  screen-space command buffer; render-target, world-space, split-screen, and
  multi-window backends are not implemented yet.
- Most existing feature UI still uses the default screen surface.
- Layering was historically implicit; the new layer primitive starts to fix only
  the root-level ordering problem.
- Most existing feature UI still uses the default layer.
- Some UI ownership still bypasses retained primitives, especially inventory
  grids, merchant grids, skill-tree panning, tool viewport picking, and gizmo
  transforms.
- Hit testing and render order are coupled to the same retained traversal.
- Pointer capture now backs the drag/drop runtime, but multi-pointer/touch drag
  input is not implemented yet.
- Keyboard focus exists, but keyboard events are not yet routed to it.
- Abstract navigation exists in the UI runtime, but physical controller mapping
  and held-repeat policy remain future work.
- Event propagation exists at the composition level, but node/widget-level
  handler registration is not implemented yet.
- Modal ownership exists in the engine, but existing game UI has not yet
  migrated its open/closed state and gameplay gates onto modal descriptors.
- Gameplay input blocking is still game-specific and manually checks feature
  singleton state.
- Inventory, merchant grids, and tools implement their own input routing.
- Feature UIs are still built mostly as raw handle structs, not as reusable
  mounted compositions.
- Raw handles are not generation checked.
- Layout now has measure/arrange containers, but most existing feature UI still
  computes geometry manually and the layout engine is not yet a mature
  widget-grade solver.
- Styling is now structured around themes, but most existing feature UI still
  hardcodes presentation and has not migrated to style classes.
- UI animation is not integrated into the runtime.
- There is no separation between render tree, focus tree, semantic tree, and
  composition ownership.
- There is no path to world-space UI, render-target UI, split-screen UI,
  multiple windows, or VR/AR without adding major concepts.

### Architectural Verdict

The current architecture is not suitable as the long-term UI runtime for a
general-purpose engine. It should not be patched into shape by adding more
feature-specific roots, more build-order conventions, or more ad hoc input
guards.

The correct direction is to keep the retained primitive tree as one lower-level
building block, then add explicit runtime primitives above it:

- surfaces
- ordered layers
- mounted compositions
- centralized input dispatch
- focus management
- modal ownership
- richer layout
- structured styling
- runtime animation and transitions

## 3. Proposed Runtime Architecture

### Final Recommended Model

The engine should evolve to:

`UiRuntime -> UiSurface -> UiLayer -> UiMount/UiTree -> UiNode`

Each frame:

1. Applications and engine modules mutate mounted UI trees.
2. The UI runtime updates layout and animations.
3. The UI input dispatcher routes exactly one UI input frame through surfaces,
   layers, focus scopes, and nodes.
4. UI events are delivered in capture, target, and bubble phases.
5. The renderer extracts commands per surface and layer.
6. The rendering backend composites each surface according to its target.

### UiRuntime

`UiRuntime` should be the engine-owned UI service. It should replace the idea
that one `UiSystem` is the whole UI world.

Responsibilities:

- Own all UI surfaces.
- Own global UI frame update.
- Own input dispatch.
- Own focus managers per surface/window.
- Own modal managers per surface/window.
- Own theme/style registries.
- Own animation update for UI properties.
- Provide compatibility APIs while migrating current `UiSystem` callers.

Why this abstraction exists:

- Split-screen, multiple windows, world-space UI, render-target UI, VR, and tools
  require more than one UI target.
- Centralized input dispatch requires one service that can rank surfaces and
  layers before delivering events.
- Modal/focus state must be shared across feature UI trees without becoming
  gameplay-specific.

Rejected alternative:

- Keep one global `UiSystem` and add flags to roots. This fails for multiple
  viewports, world-space UI, input ownership, and render-target UI.

### UiSurface

A surface is a UI coordinate/input/render target.

Surface kinds should include:

- screen-space output
- viewport-local output
- world-space plane or object
- render target
- window
- custom host surface

Surface responsibilities:

- Own local coordinate space.
- Own conversion from raw pointer/window input to local UI coordinates or rays.
- Own layer stack.
- Own render extraction target.
- Declare input priority relative to other surfaces.
- Declare whether it participates in pointer, keyboard, text, controller, and
  navigation input.

Examples:

- Main game HUD surface.
- Player 1 split-screen viewport surface.
- Player 2 split-screen viewport surface.
- In-world computer terminal surface rendered to a texture.
- Editor tool chrome surface.
- VR wrist menu surface.

Rejected alternative:

- Model world-space UI as special transformed nodes inside the screen document.
  That hides input projection, render target, depth/composition, and ownership
  differences inside node flags.

### UiLayer

Layers should be first-class runtime state. The initial implementation now adds
document layers; the mature architecture should make layers surface-owned.

Layer responsibilities:

- Explicit order.
- Visible/enabled state.
- Input participation.
- Optional blocking of lower layers.
- Optional clipping root.
- Optional transition state.
- Optional render target/composition effect.
- Named purpose without gameplay vocabulary.

Recommended generic layer examples:

- `background`
- `hud`
- `panel`
- `overlay`
- `modal`
- `tooltip`
- `debug`
- `tools`

Layers should not know about merchants, inventories, dialogue, quests, or RPG
systems. Those are compositions mounted into generic layers.

Rejected alternative:

- Per-node z-index as the primary stacking primitive. Z-index is useful locally,
  but it does not express input blocking, modal participation, layer transitions,
  or surface-level ordering policy.

### UiMount and UiComposition

UI should become compositional. A dialogue interface, inventory, shop, settings
panel, debug window, codex, terminal, skill tree, or tooltip should be a
composition that can be mounted into a host.

`UiMount` is the ownership and attachment primitive. It creates a retained
container root, tracks parent/child mounts, supports attachment to layers, nodes,
or other mounts, and owns teardown cleanup.

`UiComposition` is the authoring primitive. It is a reusable object that builds
retained UI into a mount, owns feature-local UI runtime state and cached handles,
and receives update/destroy callbacks through a `UiCompositionContext`.

The responsibilities are intentionally separate:

- `UiMount` owns lifetime and attachment.
- `UiComposition` owns authoring and local behavior.
- `UiNode` owns retained rendering data.

Composition responsibilities:

- Build a reusable UI tree.
- Receive a mount-bound runtime context.
- Own cached handles privately.
- Own local UI runtime state such as expansion, transient selection, or
  animation phase.
- Expose typed feature parameters through the concrete composition class.
- Update retained nodes from those parameters.
- Destroy its authored nodes when requested.
- Create child mounts/compositions when needed.

Mount responsibilities:

- Own the retained subtree root for a composition or compatibility builder.
- Destroy the tree when unmounted.
- Destroy child mounts recursively.
- Clear focus, capture, active pointer, text focus, navigation focus, and modal
  ownership for the subtree.
- Attach below a surface/layer/mount point.
- Provide the future target for local services such as theme, localization, or
  data model.

Why this abstraction exists:

- A settings panel should be mountable in a pause menu, title screen, editor
  window, or in-world terminal.
- A dialogue UI should be mountable as a screen overlay, fullscreen VN layer, or
  world-space speech panel.
- Debug panels should be dockable or pop out to tool windows without rewriting
  their controls.

Rejected alternative:

- Feature-specific roots owned directly by scene controllers. That keeps every
  UI feature responsible for lifetime, ordering, input, and focus policy.
- A widget-library-first design. Widgets need composition boundaries, lifetime,
  focus, modal ownership, and event routing targets before a durable widget
  framework can exist.

### UiInputDispatcher

There should be exactly one UI input dispatch per rendered frame.

Input sources:

- pointer move/down/up/cancel
- pointer wheel
- keyboard key down/up
- text input
- controller navigation
- submit/cancel/back
- future gestures, stylus, touch, VR pointers, and spatial interaction

Dispatch responsibilities:

- Build input events from platform/input manager state.
- Determine candidate surfaces by input source.
- Convert input into each surface's coordinate domain.
- Walk layers from highest to lowest priority.
- Respect modal stack and layer blocking policy.
- Hit test once per pointer event.
- Ask `UiFocusManager` to apply hover, active pointer, capture, and focus
  transitions.
- Publish a frame result for gameplay/tool gating.

Event propagation responsibilities:

- Build an event target path from retained parent hierarchy.
- Resolve target layer, mount, and composition ownership.
- Deliver capture phase from root toward the target.
- Deliver target phase at the target node.
- Deliver bubble phase from target back toward root.
- Stop routing when a handler consumes the event.
- Preserve `preventDefault()` independently from propagation.

Phase 2 replaces production feature-owned calls to
`UiSystem::updateInteraction`.

Rejected alternative:

- Keep feature-owned polling. It cannot provide reliable ownership when two UIs
  overlap, and it cannot scale to multiple surfaces or controller focus.

### Event Model

UI events should be explicit values:

- `PointerMove`
- `PointerEnter`
- `PointerLeave`
- `PointerDown`
- `PointerUp`
- `PointerClick`
- `PointerWheel`
- `KeyDown`
- `KeyUp`
- `TextInput`
- `NavigationMove`
- `NavigationSubmit`
- `NavigationCancel`
- `FocusGained`
- `FocusLost`
- `ModalOpened`
- `ModalClosed`
- `MountAttached`
- `MountDetached`
- `CompositionMounted`
- `CompositionDestroyed`
- `DragStart`
- `DragMove`
- `DragDrop`
- `DragCancel`

Events should include:

- event type
- source device id
- pointer id where applicable
- local and screen coordinates
- target node
- target layer
- target mount
- target composition
- target path
- current propagation target
- modifiers
- consumed flag
- default-prevented flag

`UiInputDispatcher` creates frame events from the dispatch result. `UiSystem`
resolves the retained target path and routes each event through capture, target,
and bubble. `UiComposition::onEvent(...)` is the current event receiver.
Node-local callbacks and widget registration remain future layers on top of the
same propagation path.

### Focus Manager

Focus is a runtime subsystem, not a node flag set by pointer press.

Implemented first-pass focus concepts:

- pointer hover per pointer id
- pointer capture per pointer id
- active pointer owner per pointer id
- keyboard focus
- text input focus
- controller/navigation focus
- focus scopes
- nested focus scopes
- focus restoration stack
- node state reflection for hovered, focused, and pressed

Future focus concepts:

- modal focus traps beyond the current modal focus scope
- richer focus reasons in propagated events

Focus scopes should let a mounted composition own local navigation without
knowing where it is mounted. A settings panel should be able to trap tab focus
inside itself when modal, or participate in a parent focus scope when embedded.

### Modal Manager

Modal ownership should be first-class and generic.

The first retained-screen implementation now owns the modal stack, layer
blocking, cancel routing, and focus-scope restoration through `UiModalManager`.
The mature surface/composition architecture should extend that model per
surface/window.

Modal responsibilities:

- Maintain a modal stack per surface/window.
- Associate modal content with a layer, and later with a mount or host.
- Decide whether lower layers receive input.
- Decide whether lower layers remain visible.
- Trap keyboard/controller focus.
- Restore previous focus when dismissed.
- Route escape/back/cancel to the top modal.
- Support enter/exit transitions.
- Support nested modals.

The engine should not know what a "shop" or "inventory" is. A shop dialog,
inventory overlay, save confirmation, pause menu, and editor asset picker are all
modal or non-modal compositions with policies.

### Layout System

The layout system should evolve from normalized anchors to a two-pass
measure/arrange engine with typed units.

Core layout primitives:

- Canvas/overlay for absolute positioning.
- Anchors for common HUD placement.
- Stack for vertical/horizontal flows.
- Dock for tool/editor panels.
- Grid for forms, inventories, settings, and tables.
- Scroll view for clipped, offset content.
- Aspect box for fixed-format boards/previews.

Core unit types:

- pixels
- percent of parent
- viewport width/height
- content/auto
- em/font-relative metrics
- optional world units for world-space surfaces

Constraints:

- min/max width
- min/max height
- preferred width/height
- aspect ratio
- grow/shrink policy
- alignment
- padding/margin/gap

The existing normalized anchor layout can remain as a `Canvas`/`AnchorLayout`
adapter during migration.

### Styling and Themes

Styling should become structured but not web-specific.

Recommended primitives:

- `UiTheme`
- `UiStyleClass`
- `UiSkin`
- `UiTypography`
- `UiMetrics`
- `UiColorToken`
- state styles for normal/hover/pressed/focused/disabled/selected

The engine should provide the mechanism, not game vocabulary. A game can define
fantasy inventory skins, VN dialogue profiles, strategy-game tables, or editor
tool themes on top of the same token system.

Rejected alternative:

- Continue writing all colors/scales directly into node payloads. That makes
  large UI sets hard to reskin, hard to keep consistent, and hard to animate
  across state changes.

### Animation and Transitions

UI animation should integrate with retained state and composition lifecycle.

Required concepts:

- property animation for opacity, transform, color, size, offset, and clip.
- state transitions for hover/pressed/focused/disabled/selected.
- layer transitions.
- modal enter/exit transitions.
- composition mount/unmount transitions.
- sequencing and cancellation.

The existing tween module should be reused as the interpolation foundation. The
UI runtime should own how tweens bind to UI properties, how they are cancelled,
and how they interact with layout and unmounting.

### Rendering Model

The renderer should extract per surface and per layer.

Screen-space surfaces should composite in output space. Viewport surfaces should
extract against viewport-local dimensions. Render-target surfaces should produce
texture output. World-space surfaces should either render into scene geometry or
render to a texture consumed by scene geometry, depending on quality and
interaction requirements.

The render extractor should not decide UI policy. It should consume already
resolved surfaces, layers, visual lists, clipping state, and transitions.

## 4. Engine API Proposal

This is a conceptual public API sketch, not final C++.

```cpp
class UiRuntime
{
public:
    UiSurfaceId createSurface(const UiSurfaceDesc& desc);
    void destroySurface(UiSurfaceId surface);

    UiLayerId createLayer(UiSurfaceId surface, const UiLayerDesc& desc);
    void setLayerState(UiLayerId layer, const UiLayerState& state);

    UiMount mount(UiLayerId layer, UiComposition& composition, const UiMountDesc& desc);
    void unmount(UiMountId mount);

    UiDispatchResult dispatchInput(const UiFrameInput& input);
    void update(float unscaledDt);

    UiRenderPacket extract(UiSurfaceId surface, const UiRenderExtractDesc& desc);
};
```

```cpp
struct UiSurfaceDesc
{
    UiSurfaceKind kind;
    UiRect viewportPixels;
    UiInputParticipation input;
    UiRenderTarget target;
    int order;
};
```

```cpp
struct UiLayerDesc
{
    std::string name;
    int order;
    bool visible;
    bool inputEnabled;
    bool blocksLowerInput;
    bool clipsToSurface;
};
```

```cpp
class UiComposition
{
public:
    virtual void build(UiBuildContext& ctx) = 0;
    virtual void update(UiUpdateContext& ctx) {}
    virtual void handleEvent(UiEventContext& ctx, const UiEvent& event) {}
    virtual void unmounted(UiUnmountContext& ctx) {}
};
```

```cpp
class UiFocusManager
{
public:
    bool requestFocus(UiDocument& document, UiHandle handle, UiFocusReason reason);
    bool clearFocus(UiDocument& document, UiFocusReason reason);
    bool restoreFocus(UiDocument& document);
    bool capturePointer(UiDocument& document, UiPointerId pointer, UiHandle handle);
    bool releasePointer(UiPointerId pointer);
    UiFocusScopeId pushScope(UiHandle owner);
    bool popScope(UiDocument& document, UiFocusScopeId scope);
    UiHandle keyboardFocusedNode() const;
    UiHandle textInputFocusedNode() const;
    UiHandle navigationFocusedNode(UiInputDeviceId device) const;
};
```

```cpp
class UiModalManager
{
public:
    UiModalId pushModal(UiLayerId layer, UiComposition& composition, const UiModalDesc& desc);
    void requestDismiss(UiModalId modal, UiDismissReason reason);
    UiModalId topModal(UiSurfaceId surface) const;
};
```

Compatibility layer during migration:

- Keep `UiSystem::createNode`, `setLayout`, `setPayload`, and
  `extractCommands` as wrappers over the default screen surface and default
  layer.
- Mark `UiSystem::updateInteraction` as compatibility-only once dispatcher lands.
- Move new code toward `UiRuntime::dispatchInput`.

## 5. Migration Strategy

The migration must not be a big bang rewrite.

### Phase 0: Characterize Current Behavior

Objectives:

- Document actual behavior.
- Add small runtime tests around ordering and hit testing.
- Identify all direct `updateInteraction` callers.

Status:

- Completed as part of this investigation.

Validation:

- Build engine.
- Run targeted UI runtime tests.
- Grep for direct input dispatch callers.

### Phase 1: Explicit Document Layers

Objectives:

- Add generic layer ids and layer state.
- Keep existing default root behavior.
- Make root ordering explicit for future UI roots.
- Verify layer ordering affects both render traversal and hit testing.

Files/classes involved:

- `engine/core/ui/UiLayer.h`
- `engine/core/ui/UiDocument.h`
- `engine/core/ui/UiDocument.cpp`
- `engine/modules/rendering/core/ui/UiSystem.h`
- `engine/modules/rendering/core/ui/UiSystem.cpp`
- `engine/tests/ui/UiLayerRuntimeTest.cpp`

Expected API changes:

- `UiSystem::createLayer`
- `UiSystem::removeLayer`
- `UiSystem::setLayerOrder`
- `UiSystem::setLayerState`
- `UiSystem::getLayerRoot`
- `UiSystem::getDefaultLayer`

Compatibility:

- `UiSystem::getRoot()` still returns the default layer root.
- Existing builders that omit a parent still attach to the default layer.

Validation:

- `ui_layer_runtime` verifies higher-order layers win hit tests, order changes
  affect hit tests, disabled layer input no longer wins hit tests, and removed
  layers lose their root.

Status:

- Implemented.

### Phase 2: Central Input Dispatch Compatibility Layer

Objectives:

- Add `UiInputDispatcher` above current hit testing.
- Create one dispatch call per frame.
- Preserve `UiInteractionResult` for existing feature code.
- Produce per-frame consumed UI input summary for gameplay/tool gates.

Files/classes involved:

- New `engine/core/ui/UiEvent.h`
- New `engine/core/ui/UiInputDispatcher.h/.cpp`
- `UiSystem`
- `RenderingRuntime` or engine controller frame orchestration
- direct callers in menu, merchant, skill tree, VN, and tools

Expected API changes:

- Add `UiSystem::dispatchInput(...)` or introduce `UiRuntime`.
- Deprecate direct feature calls to `updateInteraction`.
- Expose last dispatch result by layer/surface.

Compatibility:

- Existing features can initially read the same clicked/hovered handles from the
  dispatch result.

Validation:

- Add tests for one dispatch per frame.
- Add tests for overlapping roots where only the top target receives click.
- Verify old menu/merchant/VN behavior remains functional.

Status:

- Implemented.

### Phase 3: Focus and Modal Stack

Objectives:

- Add focus scopes.
- Add keyboard/controller focus owner.
- Add modal stack with blocking policy.
- Route escape/back/cancel to top modal.
- Replace `DungeonInputGate` checks for UI blocking with generic UI dispatch
  results where possible.

Files/classes involved:

- New `UiFocusManager`
- New `UiModalManager`
- `UiInputDispatcher`
- `InputBindingRegistry` integration
- Dungeon menu/inventory/merchant state gates

Expected API changes:

- `pushModal`
- `dismissModal`
- `requestFocus`
- `FocusScope`
- modal policy descriptors

Compatibility:

- Existing game singleton open/closed state can still drive whether a modal is
  mounted during the first migration.

Validation:

- Unit tests for nested modal focus trap and restoration.
- Gameplay smoke test for inventory/menu/dialogue input blocking.

Status:

- Focus manager implemented in Phase 3A.
- Modal manager implemented in Phase 3B.

### Phase 4: Mount Ownership

Objectives:

- Add mount tokens and attachment points.
- Make retained subtrees engine-owned instead of only handle-owned.
- Clean focus, capture, modal, text binding, and child mount state on unmount.
- Preserve compatibility with current handle-struct builders.

Files/classes involved:

- New `UiMount.h/.cpp`
- `UiDocument` subtree helpers
- `UiFocusManager`
- `UiModalManager`
- `UiSystem`

Expected API changes:

- `createMount`
- `destroyMount`
- `attachMount`
- `detachMount`
- `mountFromNode`

Compatibility:

- Existing builders can keep returning handle structs while new code wraps
  their roots in mounts.

Validation:

- Test unmount recursively removes nodes and clears focus/capture/modal state.
- Test layer removal and document clear unwind mount ownership.

Status:

- Mount ownership implemented in Phase 4.

### Phase 5: Composition API

Objectives:

- Add composition ownership on top of mounts.
- Make feature UIs reusable units that build into a mount context.
- Preserve handle-builder compatibility while moving cached handles inside
  concrete composition classes.
- Prove the model with a small migrated game UI surface.

Files/classes involved:

- New `UiComposition`
- `UiSystem`
- `UiMount`
- `InteractionPromptComposition`

Expected API changes:

- `mountComposition`
- `attachComposition`
- `updateComposition`
- `updateCompositions`
- `rebuildComposition`
- `destroyComposition`
- `findComposition`
- `compositionFromMount`
- `compositionMount`
- mount-bound `UiCompositionContext`

Compatibility:

- Existing handle structs can be owned inside composition instances initially.
- Existing retained builders can continue returning handle structs until they are
  wrapped or rewritten as concrete compositions.

Validation:

- Test composition create/attach/update/rebuild/destroy lifecycle.
- Test nested composition cleanup through mount destruction.
- Test focus and modal cleanup through composition-owned mounts.
- Test layer removal and document clear cleanup.

Status:

- Composition lifecycle implemented in Phase 5.
- The dungeon interaction prompt now demonstrates the migrated pattern: the
  concrete composition owns prompt handles and update parameters, while the
  dungeon sync system only feeds state and asks `UiSystem` to update the
  composition.

### Phase 6: Event Propagation

Objectives:

- Convert dispatch results into retained UI events.
- Route events through capture, target, and bubble phases.
- Make `UiComposition` the first event receiver.
- Preserve compatibility with existing polling systems.
- Migrate one representative system from handle polling to composition events.

Files/classes involved:

- `UiEvent`
- `UiInputDispatcher`
- `UiSystem`
- `UiComposition`
- `UiDocument`
- `VNDialogueComposition`

Expected API changes:

- `UiComposition::onEvent(...)`
- `UiSystem::events()`
- `UiSystem::propagateEvent(...)`
- event target path and mount/composition metadata

Compatibility:

- `UiDispatchResult` remains available.
- `UiSystem::updateInteraction(...)` remains a compatibility wrapper.
- Existing feature handlers may continue polling during migration.

Validation:

- Test pointer enter/leave.
- Test pointer click capture/target/bubble.
- Test consumption and default prevention.
- Test nested mounts/compositions.
- Test modal routing and cancel.
- Test focus gained/lost events.
- Test destroyed targets are ignored.

Status:

- Event propagation implemented in Phase 6.
- VN choice selection now uses `VNDialogueComposition::onEvent(...)` instead of
  `VNSystem` comparing clicked handles from `UiDispatchResult`.

### Phase 7: Surfaces

Objectives:

- Promote current document to a default screen surface.
- Add surface descriptors and extraction per surface.
- Reserve surface kinds for viewport, render-target, world, window, and custom
  hosts.
- Keep the current renderer compatible by combining visible screen-space
  surfaces into the existing UI command buffer.

Files/classes involved:

- New `UiSurface.h`
- New `UiSurface.cpp`
- `UiSystem`
- `UiInputDispatcher`
- `UiComposition`
- `RenderingRuntime` remains the default screen-surface caller.

Expected API changes:

- `createSurface`
- `destroySurface`
- `setSurfaceDesc`
- surface-aware node/layer/mount/composition APIs
- `extractSurfaceCommandsRef(surface, ...)`

Compatibility:

- Current `UiSystem` remains a default-surface facade during migration.
- Existing game UI continues to live on the default screen surface.

Validation:

- Test independent hit testing and input routing on two surfaces.
- Test surface-local coordinate conversion.
- Test surface-local focus and modal ownership.
- Test multi-surface render extraction.

Status:

- `UiSurface` implemented in Phase 7.
- Default screen-surface compatibility preserved.
- Render-target, world-space, split-screen, and multi-window backends remain
  future integrations.

### Phase 8: Layout Engine Expansion

Objectives:

- Add typed units.
- Add stack/dock/grid/scroll layout containers.
- Preserve anchor layout as compatibility.
- Move pixel-to-normalized conversion out of game UI layouts over time.

Files/classes involved:

- `UiLayout.h`
- `UiDocument` layout pass
- new layout solver files
- inventory/merchant/menu layouts as later migrations

Expected API changes:

- `UiLength`
- `UiConstraints`
- layout container specs
- measure/arrange APIs

Compatibility:

- Existing `UiLayoutSpec` remains supported through an `AnchorLayout` adapter.

Validation:

- Unit tests for layout containers.
- Visual smoke for inventory/menu/dialogue.

### Phase 9: Styling and Themes

Objectives:

- Add theme tokens, style classes, skins, typography, and metrics.
- Let feature UIs consume theme values rather than hardcoding every color/scale.
- Unify engine tool theme and game theme mechanisms without sharing content.

Files/classes involved:

- `UiStyle.h`
- `UiTheme.h`
- `UiTheme.cpp`
- `UiSurface`
- `UiDocument`
- `UiSystem`
- tool widgets
- VN presentation profile bridge
- game UI builders

Expected API changes:

- surface-local `UiTheme`
- `UiSystem::setTheme(...)`
- `UiSystem::theme(...)`
- `UiSystem::setStyleClass(...)`
- state style resolution

Compatibility:

- Payload-local colors still work.
- Existing `UiStyle` callers continue to work.

Validation:

- Theme swap test.
- Button state style tests.
- Surface-local theme tests.
- Typography-driven layout tests.

### Phase 10: Animation and Transitions

Objectives:

- Bind tweens to UI properties.
- Support state transitions.
- Support mount/unmount and modal enter/exit transitions.

Files/classes involved:

- tween module
- new UI animation binding layer
- `UiRuntime::update`
- modal manager

Expected API changes:

- `animate(node, property, target, desc)`
- transition specs on layers/modals/styles

Compatibility:

- Existing manual VN tweening can migrate gradually.

Validation:

- Unit tests for cancellation and unmount safety.
- Visual smoke for modal transitions.

### Phase 11: Feature Migration

Objectives:

- Move merchant, inventory, menus, VN, HUD, and tools onto surfaces/layers/mounts.
- Remove remaining feature-owned manual pointer ownership where it should become
  retained UI behavior.
- Replace `DungeonInputGate` UI checks with runtime modal/input dispatch state.
- Keep gameplay vocabulary outside engine core.

Validation:

- Build game.
- Manual scene smoke for dungeon HUD, menu, inventory, merchant, VN dialogue,
  choices, tools, and combat UI.
- Grep for remaining direct calls to `updateInteraction`.

## 6. Initial Implementation Plan

The first implementation should establish primitives, not rewrite all UI at
once.

### Implementation 1: Document Layers

Done.

Purpose:

- Remove the worst accidental root-order dependency.
- Introduce the naming and ownership direction needed for surfaces and modal
  layers later.
- Keep the compatibility root intact.

Validated by:

- Engine release build.
- `ui_layer_runtime` CTest.

### Implementation 2: Input Dispatch Skeleton

Done.

Objectives:

- Add a dispatcher that calls current hit testing exactly once per frame for the
  default surface.
- Preserve old `UiInteractionResult` shape.
- Add a dispatch result that says whether pointer, keyboard, text, navigation,
  or back/cancel input was consumed by UI.
- Move feature code to read dispatch results instead of invoking dispatch.

Specific first files:

- `engine/engine/core/ui/UiEvent.h`
- `engine/engine/core/ui/UiInputDispatcher.h`
- `engine/engine/core/ui/UiInputDispatcher.cpp`
- `engine/engine/modules/rendering/core/ui/UiSystem.h`
- `engine/engine/modules/rendering/core/ui/UiSystem.cpp`
- `engine/engine/modules/rendering/runtime/RenderingRuntime.cpp`
- retained UI callers that previously initiated `UiSystem::updateInteraction`

Validation:

- Added `ui_input_dispatcher_runtime`.
- Verified production retained UI callers no longer call `updateInteraction`.
- Ran engine build, top-level game build, and targeted UI tests.

### Implementation 3: Focus State Extraction

Done.

Objectives:

- Move hover, active pointer, and focus out of `UiSystem` into a dedicated focus
  manager.
- Keep node state flags as presentation reflection, not ownership.

Specific first files:

- `UiFocusManager.h/.cpp`
- `UiInputDispatcher`
- `UiSystem`

Validation:

- Added `ui_focus_manager_runtime`.
- Covered pointer capture, focus restoration, invalid handle pruning, layer
  interactions, and dispatcher capture targeting.

### Implementation 4: Modal Stack

Done.

Objectives:

- Add generic modal policy.
- Associate modal ownership with an existing layer and owner node.
- Route cancel/back through the top modal.
- Produce UI input-blocking results for gameplay systems.

Specific first files:

- `UiModalManager.h/.cpp`
- `UiInputDispatcher`
- `UiFocusManager`
- `UiSystem`
- `UiInteraction`
- `RenderingRuntime`

Validation:

- Added `ui_modal_manager_runtime`.
- Covered nested modals, focus restoration, layer blocking, consumption flags,
  cancel routing, dismissals, hidden/disabled layer cleanup, hidden/disabled
  owner cleanup, destroyed owner cleanup, and non-blocking popup policy.

### Implementation 5: Mount Ownership

Done.

Objectives:

- Add an engine-owned lifetime object for retained UI subtrees.
- Support attachment to layers, nodes, and parent mounts.
- Destroy child mounts recursively.
- Clean focus, capture, modal, and text binding state when a mount is destroyed.
- Preserve current raw handle builders as compatibility users.

Specific first files:

- `UiMountTypes.h`
- `UiMount.h/.cpp`
- `UiDocument`
- `UiFocusManager`
- `UiModalManager`
- `UiInteraction`
- `UiInputDispatcher`
- `UiSystem`

Validation:

- Added `ui_mount_runtime`.
- Covered mount creation, parent-node attachment, nested mount destruction,
  child destruction, focus/capture cleanup, modal cleanup, layer removal,
  document clear, attach/detach, manual root removal, and invalid mount ids.

## 7. Phase 2 Implementation Report

### Investigation Graph

Before Phase 2, retained UI dispatch flowed through feature systems:

```
Platform input
    -> InputManager
    -> InputBindingRegistry
    -> feature UI event system
    -> feature-owned UiInputFrame construction
    -> UiSystem::updateInteraction(...)
    -> UiDocument::hitTest(...)
    -> UiInteractionResult
    -> feature UI event system
```

The direct retained dispatch initiators were:

- `src/menus/ui/MenuUiEventHandler.cpp`
- `src/inventory/ui/MerchantUiEventHandler.cpp`
- `src/classes/ui/SkillTreeUiEventHandler.h`
- `engine/modules/visualnovel/systems/VNSystem.hpp`
- `engine/modules/tools/ui/EngineToolShellSystem.hpp`

Each duplicated the same normalized pointer conversion, primary-button mapping,
scroll capture, call into `UiSystem::updateInteraction(...)`, and result
interpretation.

Several paths bypass retained UI dispatch and still compute ownership from raw
input:

- `src/inventory/ui/InventoryUiEventHandler.cpp` maps raw mouse position to
  inventory cells and performs drag/drop with raw primary-button edges.
- `src/inventory/ui/MerchantUiEventHandler.cpp` maps raw mouse position to
  merchant/player grid cells and selects grid items with raw primary-button
  edges.
- `src/classes/ui/SkillTreeUiEventHandler.h` still uses raw scroll and
  primary-button state for canvas pan/drag, while node/button hover and click now
  come from central dispatch.
- `engine/modules/tools/ui/EngineToolShellSystem.hpp` still computes viewport
  pointer capture from raw mouse position for tool world picking.
- Tool world picking and gizmo systems consume `EngineToolInputCaptureComponent`
  rather than retained UI hit testing.
- Camera/debug systems may read raw mouse deltas for non-UI control, which is
  outside retained UI dispatch.

### New Dispatch Flow

After Phase 2, retained UI dispatch flows through the engine:

```
Platform input
    -> InputManager
    -> InputBindingRegistry
    -> RenderingRuntime::dispatchUiInput(...)
    -> UiSystem::dispatchInput(...)
    -> UiInputDispatcher
    -> UiDocument::hitTest(...)
    -> UiDispatchResult
    -> feature UI systems read ctx.ui->dispatchResult()
```

`RenderingRuntime::dispatchUiInput(...)` runs once in `GravitasEngine` after
`platform.beginFrame()` and engine-level input handling, before fixed simulation
ticks and before controller systems. This placement gives every scene and tool
controller the same dispatch result for the frame and prevents feature systems
from racing each other to mutate retained interaction state.

### Dispatch Result

`UiDispatchResult` is the new runtime result surface. It contains:

- hovered, focused, pressed, released, clicked, active, and captured handles
- owning layer ids for each handle
- default surface id for the current screen-space document
- pointer position and scroll delta
- primary-button state
- pointer, keyboard, navigation, and text-input consumed flags
- frame id and dispatch sequence

At the end of Phase 2 the implementation performed primary-pointer dispatch and
read focus ownership from `UiFocusManager`. Keyboard event routing, navigation
event routing, text input event routing, modal routing, event propagation, and
surface routing had reserved fields and event types but were intentionally not
implemented yet.

### Event Model

`UiEvent.h` defines the future event vocabulary:

- pointer move, enter, leave, down, up, click, and wheel
- key down/up
- text input
- navigation move, submit, and cancel
- focus gained/lost
- drag start, move, drop, and cancel

The dispatcher does not yet bubble or capture events. These types remain the
stable foundation for modal, navigation, text input, drag/drop, and composition
phases.

### Compatibility

`UiSystem::updateInteraction(...)` remains as a compatibility API over
`UiInputDispatcher`. Production retained UI systems no longer call it after Phase
2. The remaining first-party references are the compatibility API itself and the
older `ui_layer_runtime` regression test.

Manual grid and tool-world pointer ownership remains intentionally unmigrated.
Those paths need retained grid widgets, drag/drop ownership, focus scopes, and
modal policy before they can become purely dispatch-result consumers.

### Validation

Added `ui_input_dispatcher_runtime`, covering:

- overlapping UI sibling priority
- explicit layer priority
- click ownership
- hover changes and node state updates
- disabled layers
- hidden layers
- click/release behavior
- release-away behavior
- dispatch exactly once per frame through frame-id caching

## 8. Phase 3A Implementation Report

### Focus Ownership Investigation

Before Phase 3A, retained UI ownership was split incorrectly:

- `UiInputDispatcher` owned `activeHandle` and `focusedHandle`.
- `UiInputDispatcher` used `lastDispatch.hovered` and `lastDispatch.pressed` as
  persistent state to mutate retained node flags.
- `UiStateFlags::hovered`, `focused`, and `pressed` were treated as both
  presentation flags and de facto ownership state.
- `UiDispatchResult` reported per-frame hovered/focused/pressed/clicked state,
  but there was no separate canonical runtime owner for focus.

Feature-owned state still exists outside retained focus:

- `InventoryUiState` owns hovered cells, hovered item id, dragged item id, and a
  `dragging` flag from raw mouse position and primary-button edges.
- `MerchantUiState` owns hovered pane/cell/item and selected pane/item for
  inventory grids; retained buttons use centralized dispatch.
- `SkillTreeUiState` owns hovered skill node id and canvas dragging/panning
  state; retained node/button hits use centralized dispatch.
- `MenuUiState` owns key-rebind capture state through `captureAction` and
  `captureArmed`.
- Combat UI owns action-menu selection index as gameplay/menu state, not engine
  UI focus.
- Engine tools still publish `EngineToolInputCaptureComponent` for tool chrome,
  viewport pointer ownership, world picking, and gizmo interaction. Gizmo
  hovered/active axis and selected entity remain tool-domain state.
- VN choice clicks now arrive through `VNDialogueComposition::onEvent(...)`, but
  VN playback/choice selection remains VN runtime state.

Those feature states are not all engine focus. Grid selection, selected combat
action, selected entity, and dragged inventory item are domain state. Pointer
hover, pointer capture, active pointer, keyboard focus, text-input focus,
navigation focus, and focus restoration are engine UI runtime state.

### Implemented Focus Model

`UiFocusManager` is now the authoritative owner for persistent retained UI
interaction ownership:

- `pointer id -> hovered node`
- `pointer id -> captured node`
- `pointer id -> active pointer node`
- keyboard-focused node
- text-input-focused node
- `input device id -> navigation-focused node`
- focus scope stack
- focus restoration stack

The current runtime still dispatches only the primary pointer because
`UiInputFrame` has one pointer position and primary button state. The focus
manager API is keyed by `UiPointerId` and `UiInputDeviceId` so future mouse,
touch, stylus, VR pointer, and gamepad paths can attach without redesigning the
ownership model.

Retained node `hovered`, `focused`, and `pressed` flags are now reflection.
`UiFocusManager` mutates those flags so existing skins and widgets continue to
work, but the flags are not authoritative ownership. If a node is hidden,
disabled, moved under a hidden/disabled layer root, removed, or reparented into
an unavailable branch, `UiSystem` prunes focus-manager ownership.

Pointer capture is separate from hover. Hit testing still reports the physical
hovered node. If a pointer is captured, the dispatcher uses the captured node as
the pointer target for press/release/click ownership and keeps
`pointerConsumed` true while capture exists.

Focus scopes are stack-based. Pushing a scope records the previous keyboard
focus and focus scope. Popping the top scope restores that recorded focus when
it is still valid, otherwise it falls back through the restoration stack. This
is the primitive modal ownership needs for focus restoration.

### Dispatcher Changes

`UiInputDispatcher` now dispatches input; it no longer owns persistent focus
state. It receives `UiFocusManager&` from `UiSystem::dispatchInput(...)` and:

- Prunes invalid focus handles before dispatch.
- Updates pointer hover through the focus manager.
- Uses captured pointer owner as the pointer target when capture is active.
- Requests active pointer and keyboard focus through the focus manager on
  primary press.
- Reports released/clicked handles from this frame's pointer transition.
- Publishes hovered, focused, pressed, released, clicked, active, and captured
  handles plus their owning layers in `UiDispatchResult`.
- Reports pointer, keyboard, navigation, and text-input consumed flags from
  focus ownership.

`UiSystem::updateInteraction(...)` remains a compatibility helper over the
central dispatch path.

### API Surface

New public runtime API:

- `UiSystem::focusManager()`
- `UiFocusManager::requestFocus(...)`
- `UiFocusManager::clearFocus(...)`
- `UiFocusManager::restoreFocus(...)`
- `UiFocusManager::requestTextInputFocus(...)`
- `UiFocusManager::setNavigationFocus(...)`
- `UiFocusManager::setHovered(...)`
- `UiFocusManager::capturePointer(...)`
- `UiFocusManager::releasePointer(...)`
- `UiFocusManager::setActivePointer(...)`
- `UiFocusManager::pushScope(...)`
- `UiFocusManager::popScope(...)`

`UiDispatchResult` now also reports:

- `captured`
- `capturedLayer`

`UiInputDeviceId`, `UiPointerId`, `UI_PRIMARY_INPUT_DEVICE`, and
`UI_PRIMARY_POINTER` live beside the input/dispatch result types so focus and
events share device identity vocabulary.

### Validation

Added `ui_focus_manager_runtime`, covering:

- hover transitions
- focus transitions and restoration
- pointer capture
- pointer capture release
- capture surviving hover changes
- active pointer pressed-state reflection
- nested scopes and focus restoration
- disabled node focus rejection
- hidden node focus rejection
- destroyed node focus/capture/active pruning
- layer visibility/input interactions
- independent keyboard, text input, and navigation focus
- dispatcher use of captured pointer target

The existing layer and input-dispatcher runtime tests still pass.

### Remaining Technical Debt

- `UiInputFrame` still represents one primary pointer. The focus manager is
  multi-pointer-ready, but dispatch input is not.
- The navigation graph now handles abstract traversal and submit; physical
  gamepad mapping, held-repeat policy, and richer role-specific behavior remain
  future work.
- There are no text widgets or IME/text event routing yet.
- Feature-owned inventory grid drag/drop, skill-tree pan, key-rebind capture,
  tool viewport capture, world picking, and gizmo drag remain outside generic UI
  focus/drag ownership.

### Phase 3A Handoff to Modal Ownership

At the end of Phase 3A, `UiModalManager` was the next correct milestone because
the engine had the required foundation for first-pass modal ownership:

- explicit ordered layers
- centralized dispatch
- focus scopes
- focus restoration
- pointer capture and active pointer ownership
- dispatch consumed flags

No additional primitive needed to precede modal ownership. The modal manager
started as policy over existing layers and focus scopes, not as a
composition/mount rewrite. Composition and mounts should follow after modal
policy because modal ownership can now define where mounted trees participate
in focus, input blocking, and restoration.

## 9. Phase 3B Implementation Report

### Modal Behavior Investigation

Before Phase 3B, modal-like ownership was distributed across engine modules and
game feature state:

- `DungeonInputGate` manually locked gameplay input while floor transitions,
  combat transitions, menu state, inventory state, merchant state, or blocking
  VN playback were active.
- `DungeonInputSystem` preserved only menu commands while that gate reported
  gameplay locked.
- `MenuUiEventHandler` opened and closed the game menu, controls/video panels,
  skill tree, and keybinding capture state through feature singletons and
  feature actions.
- `InventoryUiEventHandler` blocked inventory interaction behind merchant, menu,
  and VN state, then performed raw pointer and drag/drop ownership for the
  inventory grid.
- `MerchantUiEventHandler` closed on Back and used centralized dispatch for
  retained buttons, while merchant/player grids still computed hover/selection
  manually from raw pointer position.
- The VN runtime used playback state, presentation state, and execution
  profiles such as `dialogue_overlay` and `fullscreen_dialogue` to block or
  freeze gameplay, capture pointer input, and control render build policy.
- Engine tools used `EngineToolInputCaptureComponent` to distinguish tool chrome
  pointer ownership, viewport/world consumption, keyboard capture, and gizmo
  state.

These paths contain both engine modal policy and legitimate domain state.
Open/closed inventory state, selected merchant item, selected skill, selected
entity, dragged inventory item, and VN playback state remain game/tool domain
state. Exclusive UI interaction, layer blocking, focus-scope activation, cancel
routing, and input-consumption summaries are engine modal policy.

### Implemented Modal Model

`UiModalManager` is now the retained UI runtime owner for modal policy:

- Maintains a stack of modal descriptors.
- Tracks modal id, owner node, layer, and focus scope.
- Supports nested modals by pushing one focus scope per modal.
- Restores focus by popping the modal's focus scope through
  `UiFocusManager`.
- Routes cancel/back to the top modal only.
- Dismisses the top modal when its descriptor allows cancel dismissal.
- Prunes invalid modals when their layer is removed, hidden, disabled, or when
  their owner node is destroyed.
- Publishes modal state for dispatch results.

The first modal descriptor intentionally describes policy rather than content:

- modal layer
- owner node
- optional initial focus node
- layer blocking policy
- cancel-dismiss policy
- focus restoration policy
- pointer, keyboard, navigation, and text-input consumption flags

Implemented layer blocking policies are:

- `AllOtherLayers`: the top modal layer is the only pointer-hit-tested layer.
- `LowerLayers`: layers at or below the modal layer order are blocked, while
  higher layers can remain interactive.
- `None`: the modal participates in focus/cancel policy without pointer layer
  blocking.

The manager does not create UI nodes, build windows, render overlays, animate
transitions, or know gameplay vocabulary.

### Dispatcher Integration

The dispatch path is now:

```
Platform input
    -> InputManager
    -> InputBindingRegistry
    -> RenderingRuntime::dispatchUiInput(...)
    -> UiSystem::dispatchInput(...)
    -> UiInputDispatcher
    -> UiModalManager policy query
    -> UiDocument hit test
    -> UiFocusManager ownership update
    -> UiDispatchResult
```

`UiInputDispatcher` consults `UiModalManager` before pointer hit testing. If the
top modal restricts pointer interaction to its layer, dispatch performs a layer
scoped hit test through `UiDocument::hitTestLayer(...)`. If the hovered result
belongs to a blocked layer, dispatch clears the hover target. Captured and
active pointer owners are released if their layers become blocked.

The dispatcher reports:

- `modalActive`
- `modalDepth`
- `modal`
- `modalOwner`
- `modalMount`
- `modalLayer`
- `pointerBlocked`
- `keyboardBlocked`
- `navigationBlocked`
- `textBlocked`
- `cancelTargetModal`
- `dismissedModal`
- `cancelPressed`
- `cancelConsumed`

Keyboard, navigation, and text-input blocking are exposed as consumed flags, but
key event routing, navigation event routing, and text input delivery remain
future work.

### Public API Surface

New engine-facing APIs:

- `UiSystem::modalManager()`
- `UiSystem::pushModal(...)`
- `UiSystem::popModal(...)`
- `UiSystem::dismissTopModal(...)`
- `UiModalManager::pushModal(...)`
- `UiModalManager::popModal(...)`
- `UiModalManager::dismissTopModal(...)`
- `UiModalManager::routeCancel(...)`
- `UiModalManager::pruneInvalidModals(...)`
- `UiModalManager::isLayerBlockedForPointer(...)`
- `UiModalManager::state()`

Compatibility remains intact. Existing feature systems can continue to use their
singletons and read `ctx.ui->dispatchResult()` while migrating incrementally.
`UiSystem::updateInteraction(...)` remains a compatibility API over the
centralized dispatcher.

### Validation

Added `ui_modal_manager_runtime`, covering:

- single modal state publication
- nested modal stack integrity
- focus restoration
- pointer and layer blocking
- keyboard, navigation, and text-input blocking flags
- cancel routing to the top modal only
- manual dismiss
- nested dismiss
- hidden layer cleanup
- disabled layer cleanup
- hidden owner cleanup
- disabled owner cleanup
- destroyed owner cleanup
- non-blocking popup policy

The existing `ui_layer_runtime`, `ui_input_dispatcher_runtime`, and
`ui_focus_manager_runtime` regressions continue to pass.

### Remaining Compatibility Paths

The engine primitive exists, but game/tool systems have not yet migrated their
modal-like behavior onto descriptors:

- `DungeonInputGate` still manually checks game feature state.
- Menu/inventory/merchant/VN open state still lives in game or module
  singletons.
- Inventory and merchant grids still own raw pointer hover/click/drag behavior.
- Keybinding capture still uses menu-local state.
- Tool viewport capture, world picking, and gizmo manipulation still use
  tool-specific capture state.
- VN execution profiles still own world sleep/render policy; that remains
  correct, but VN blocking policy can later also publish a modal descriptor.

### Phase 3B Handoff to Mount Ownership

At the end of Phase 3B, the single highest-value missing primitive was
`UiMount`.

Modal ownership now can say which layer owns interaction, but the runtime still
has no generic lifetime/location object for a subtree. Feature UIs are still
manually-created roots with handles scattered through feature state. A mount
token should come before surfaces, event propagation, layout, styling,
animation, navigation, and drag/drop because it gives every later system a
stable unit of UI ownership.

`UiMount` should own:

- the root subtree attached under a layer or future mount point
- unmount cleanup
- focus/capture/modal pruning for the subtree
- optional parent/child mount relationships
- the bridge from current handle-based builders to future `UiComposition`
  authoring

`UiComposition` should follow as the authoring abstraction that builds into a
mount. Surfaces could wait because the current default screen document was
enough to migrate lifetime ownership first. Event propagation, navigation,
drag/drop, layout, styling, and animation all become cleaner once they can
target mounted subtrees instead of feature-specific root handles.

## 10. Phase 4 Implementation Report

### Retained UI Ownership Investigation

Before Phase 4, retained UI ownership was still distributed through raw handles:

- VN dialogue UI built one root and stored `VNDialogueUiHandles`; destruction
  manually removed the root.
- Dungeon HUD and interaction prompt builders returned handle structs; sync
  systems owned those handles and no automatic teardown existed beyond scene UI
  clear.
- Inventory, merchant, menu, and skill-tree coordinators built handle structs,
  copied them into event handlers and sync systems, and reset the C++ wrappers
  without engine-owned subtree lifetime.
- Combat scenes built `CombatUiHandles` directly during scene load and passed
  them into sync systems and billboard components.
- Engine tool shell and tool panels each stored root handles and manually
  removed them when tools were hidden or destroyed.
- Shared widgets such as panel skins, dropdowns, sliders, HP bars, and tool
  controls returned interior handles but did not own subtree lifetime.

Those handle structs remain useful as sync caches. They are not good lifetime
owners. Lifetime ownership, attachment, recursive teardown, focus cleanup, and
modal cleanup now belong to mounts.

### Implemented Mount Model

`UiMount` is now the retained UI ownership primitive:

- `UiMountId` identifies a mounted subtree.
- Each mount owns one retained container root.
- A mount can attach to a layer, an arbitrary parent node, or a parent mount.
- Parent/child mount relationships are tracked explicitly.
- Destroying a parent mount destroys all child mounts.
- `UiSystem::removeNode(...)` treats removal of an exact mount root as mount
  destruction.
- Removing a layer destroys mounts attached to that layer.
- `UiSystem::clear()` resets the mount table and recreates the root mount.

The root mount is conceptual ownership for the document root. It cannot be
destroyed and does not replace layer roots.

### Cleanup Rules

Destroying a mount automatically:

- removes text bindings below the mount root
- dismisses modals owned by the mount or by owner nodes inside the subtree
- clears pointer hover under the subtree
- releases pointer capture under the subtree
- clears active pointer ownership under the subtree
- clears keyboard focus under the subtree
- clears text-input focus under the subtree
- clears navigation focus under the subtree
- removes focus scopes owned by nodes under the subtree
- removes the retained subtree from `UiDocument`
- removes child mount records recursively

This cleanup is engine policy. Feature systems should not need to manually
clear retained focus, capture, or modal state for mounted UI.

### API Surface

New engine-facing APIs:

- `UiSystem::mountManager()`
- `UiSystem::createMount(...)`
- `UiSystem::destroyMount(...)`
- `UiSystem::attachMount(...)`
- `UiSystem::detachMount(...)`
- `UiSystem::findMount(...)`
- `UiSystem::mountFromNode(...)`
- `UiSystem::rootMount()`
- `UiSystem::mountRoot(...)`
- `UiFocusManager::clearForSubtree(...)`
- `UiModalManager::dismissModalsOwnedByMount(...)`
- `UiModalManager::dismissModalsForSubtree(...)`

`UiModalDesc` and `UiDispatchResult` now carry modal mount ownership through
`ownerMount` / `modalMount`. `UiSystem::pushModal(...)` infers the owner mount
from the modal owner node when the caller has not supplied one.

### Validation

Added `ui_mount_runtime`, covering:

- mount creation
- parent-node attachment and child-mount inference
- nested mount destruction
- child mount destruction without destroying the parent
- focus, hover, active pointer, pointer capture, text focus, and navigation
  cleanup
- focus scope cleanup
- modal mount ownership and cleanup
- layer removal cleanup
- document clear reset
- attach/detach between layers
- manual removal of a mount root through `removeNode`
- invalid mount ids

The existing layer, input-dispatcher, focus-manager, and modal-manager runtime
tests continue to pass.

### Remaining Compatibility Paths

The engine primitive exists, but current feature UI has not yet migrated onto
mount creation:

- At this phase, VN dialogue UI still returned `VNDialogueUiHandles`.
- Dungeon HUD and prompt UI still return handle structs.
- Inventory, merchant, menu, and skill-tree coordinators still pass handle
  structs to handlers and sync systems.
- Combat UI and billboard UI still use raw handles from scene load.
- Engine tool panels still store and destroy root handles manually.
- Shared widgets still return interior handle bundles.

These are compatibility paths. The migration should wrap top-level feature UI
roots in mounts and then move builder/sync handle bundles into concrete
`UiComposition` classes.

## 11. Phase 5 Implementation Report

### Retained Builder Investigation

Before Phase 5, mount ownership existed but most UI authoring was still
procedural and handle-oriented:

- VN dialogue builds a retained root, panel skin nodes, nameplate, text, and
  choice handles through `VNDialogueUiHandles`; update and choice-hit helpers
  operate on that exposed handle bundle.
- Dungeon HUD builds debug/minimap roots and caches handles in
  `DungeonHudSyncSystem`.
- The interaction prompt built a root/background/label handle bundle and the
  sync system owned the handle cache directly.
- Inventory, merchant, menu, and skill-tree coordinators build handle structs,
  copy them into event handlers and sync systems, and synchronize state by
  mutating raw retained handles every frame.
- Combat scene setup builds `CombatUiHandles` during scene load and passes them
  to combat sync systems and billboard state.
- Engine tool panels implement their own `build/update/destroy` pattern and
  usually store one root handle plus panel-local handles.
- Shared UI helpers such as panel skins, dropdowns, HP bars, and editor widgets
  return interior handle bundles for callers to store.

The duplicated behavior is consistent: each feature creates nodes, exposes
handles, stores the handles elsewhere, syncs from feature state, and manually
removes the root. Mounts now own lifetime, but without compositions the runtime
still could not say which reusable UI object authored a subtree or owned its
cached handles.

### Implemented Composition Model

`UiComposition` is now the reusable retained UI authoring primitive:

- `UiCompositionId` identifies a mounted composition.
- `UiCompositionContext` exposes the owning `UiSystem`, `UiDocument`, resource
  provider, mount id, and mount root.
- `UiComposition::build(...)` creates retained nodes under the mount root.
- `UiComposition::update(...)` synchronizes retained nodes from composition
  parameters or local runtime state.
- `UiComposition::destroy(...)` lets the composition release its authored nodes
  and private handle caches before mount teardown.
- `UiSystem::rebuildComposition(...)` runs destroy/build on the same mount for
  explicit rebuild flows.

The runtime deliberately does not prescribe gameplay parameters. Concrete
composition classes own their own typed setters, state structs, or future data
bindings. The engine only owns lifecycle and mount integration.

### API Surface

New engine-facing APIs:

- `UiSystem::mountComposition(...)`
- `UiSystem::attachComposition(...)`
- `UiSystem::updateComposition(...)`
- `UiSystem::updateCompositions()`
- `UiSystem::rebuildComposition(...)`
- `UiSystem::destroyComposition(...)`
- `UiSystem::findComposition(...)`
- `UiSystem::compositionFromMount(...)`
- `UiSystem::compositionMount(...)`

Composition cleanup is integrated into `UiSystem::destroyMount(...)`,
`UiSystem::removeLayer(...)`, and `UiSystem::clear()`. Destroying a parent mount
destroys child composition records before the retained subtree is removed.

### Feature Demonstration

The dungeon interaction prompt now demonstrates the migration model:

- `InteractionPromptComposition` owns the prompt handles and prompt update
  parameters.
- `DungeonUiController` mounts the composition through
  `UiSystem::mountComposition(...)`.
- `InteractionPromptSyncSystem` stores a `UiCompositionId`, feeds prompt text and
  viewport parameters into the concrete composition, and calls
  `UiSystem::updateComposition(...)`.
- The old handle-based constructor remains as a compatibility path for scenes
  that still build the prompt directly.

This proves the intended migration without rewriting inventory, merchant, menu,
VN, tool, or combat UI.

### Validation

Added `ui_composition_runtime`, covering:

- composition creation
- mounting into a new mount
- attachment to an existing mount
- update and parameter synchronization
- explicit rebuild
- destroy lifecycle
- nested composition cleanup through parent mount destruction
- focus cleanup through composition-owned mount destruction
- modal cleanup through composition-owned mount destruction
- layer removal cleanup
- document clear cleanup

The existing layer, input-dispatcher, focus-manager, modal-manager, and mount
runtime tests continue to pass.

### Remaining Builder Patterns

The following compatibility paths remain:

- Dungeon HUD still exposes `DungeonHudHandles`.
- The Yune spatial test scene still uses the handle-backed interaction prompt
  path.
- Inventory, merchant, menu, and skill-tree coordinators still expose handle
  structs to sync and event systems.
- Combat UI and billboard UI still build raw handles during scene load.
- Engine tool panels still use panel-local root handles.
- Shared widgets still return interior handle bundles.

`VNDialogueUiHandles` still exists as an internal helper cache owned by
`VNDialogueComposition`.

These should migrate one composition at a time. The preferred pattern is now:

`feature state -> concrete UiComposition parameters -> updateComposition -> retained nodes`

## 12. Phase 6 Implementation Report

### Polling Investigation

Before Phase 6, the runtime owned hit testing but feature systems still
interpreted interaction manually:

- VN read `ctx.ui->dispatchResult().toInteractionResult()` and compared clicked
  handles against choice buttons and labels.
- Menu UI read clicked handles for scrim, resume, panel navigation, settings
  rows, dropdown options, and wheel scrolling.
- Skill tree UI read hovered/clicked handles for node hover, unlock requests,
  and viewport panning.
- Inventory UI mapped raw pointer position into grid cells, then used input
  press/release edges for drag/drop.
- Merchant UI combined raw pointer-to-grid mapping with clicked handles for
  buttons, scrim, pay/sell/leave, and confirmation flows.
- Engine tool panels received `UiInteractionResult` and compared hovered,
  pressed, clicked, and scroll fields against panel-local handles.
- Tool world picking and gizmos still use tool-specific capture state outside
  retained UI.

The common duplication was: poll dispatch result, compare raw handles, infer an
event, perform feature behavior. Phase 6 establishes the engine path that makes
those inferred events explicit.

### Implemented Event Pipeline

The retained event pipeline is now:

`Platform/Input -> UiInputDispatcher -> UiEvent values -> UiSystem propagation -> UiComposition::onEvent(...)`

`UiInputDispatcher` creates events for the currently implemented input surface:

- pointer enter
- pointer leave
- pointer move
- pointer down
- pointer up
- pointer click
- pointer wheel
- navigation cancel
- focus gained
- focus lost

`UiSystem` owns propagation because it can resolve the ownership data that the
core dispatcher should not know:

- target path from retained parent hierarchy
- target layer
- target mount
- target composition
- current propagation target
- current mount
- current composition
- target-local pointer position

### Propagation Semantics

Propagation has three phases:

- Capture: root toward the target's parent.
- Target: the target node.
- Bubble: target back toward root.

`event.consume()` stops further propagation. `event.preventDefault()` records
that default behavior should not run but does not stop propagation. This
separation is important for future widgets: a child can prevent a default
action while still allowing parent containers, analytics, tool overlays, or
accessibility layers to observe the event.

`UiComposition::onEvent(...)` is invoked when the current propagation node
belongs to a mounted composition. This means parent compositions can observe
capture/bubble around child compositions, while child compositions receive
events for their own subtree.

Destroyed targets are invalidated before routing. If a handler destroys the
target subtree during propagation, routing stops safely.

### API Surface

New or expanded APIs:

- `UiEvent`
- `UiComposition::onEvent(...)`
- `UiSystem::events()`
- `UiSystem::propagateEvent(...)`
- `UiDocument::pathFromRoot(...)`
- `UiInputDispatcher::events()`

`UiDispatchResult` remains available for compatibility and for gameplay/tool
input gating during migration.

### Feature Demonstration

VN choice selection now demonstrates composition-owned event behavior:

- `VNDialogueComposition` builds/syncs/destroys the VN dialogue UI inside a
  mount.
- `VNDialogueComposition::onEvent(...)` receives `PointerClick`, maps it to a
  choice using the event target path, consumes the event, and records the choice
  intent.
- `VNSystem` no longer compares clicked handles from `UiDispatchResult` for
  choices. It consumes the choice intent from the composition.

This preserves VN runtime behavior while moving the actual retained UI
interaction into the composition that authored the choice buttons.

### Validation

Added `ui_event_propagation_runtime`, covering:

- pointer enter
- pointer leave
- pointer click
- capture phase
- target phase
- bubble phase
- consumption
- default prevention
- nested compositions
- modal click routing
- modal cancel routing
- focus gained
- focus lost
- destroyed target cleanup

The existing layer, input-dispatcher, focus-manager, modal-manager, mount, and
composition runtime tests continue to pass.

### Remaining Polling Paths

The event system exists, but these compatibility paths remain:

- Menu UI still maps propagated intent through `UiInteractionResult` style
  handle comparisons.
- Skill tree still polls hovered/clicked handles and raw scroll.
- Inventory and merchant still map raw pointer position into grids and own drag
  state.
- Engine tool panels still receive `UiInteractionResult`.
- Tool viewport capture, world picking, and gizmos still use tool-specific
  capture.
- Shared widget helpers still expose handles rather than registering handlers.

These should migrate onto widgets, retained events, and the drag/drop runtime
as their controls are revisited.

## 13. Phase 7 Implementation Report

### Single-Surface Assumption Investigation

Before Phase 7, `UiSystem` was the UI universe. The concrete single-surface
assumptions were:

- `UiSystem` owned exactly one `UiDocument`.
- `UiDocument` owned all layers, so layer ids were only meaningful inside that
  one document.
- `UiHandle` allocation restarted on `UiDocument::clear()`, and handles were
  treated as globally meaningful because only one document existed.
- `UiInputDispatcher` always wrote `UI_DEFAULT_SURFACE` into
  `UiDispatchResult` and `UiEvent`.
- `UiFocusManager` was global to the one document, so pointer hover, capture,
  keyboard focus, text focus, and navigation focus had no surface boundary.
- `UiModalManager` owned one modal stack for the whole runtime.
- `UiMountManager` owned one mount tree rooted in the one document.
- `UiComposition` records were keyed only by mount id, which would collide once
  multiple mount managers existed.
- Text font bindings were keyed only by `UiHandle`, which would collide once
  multiple documents existed.
- Event propagation resolved target paths against the one document and resolved
  compositions by one global mount id map.
- Rendering extraction resolved one visual list into one command buffer.
- `RenderingRuntime::dispatchUiInput(...)` normalized mouse coordinates against
  the output window and assumed that was the UI coordinate space.
- The Vulkan `UiRenderStage` accepted one already-combined screen-space
  `UiCommandBuffer`.

Phase 7 changes these assumptions by making the default screen UI just one
surface. Handles, layers, mounts, focus, modals, and dispatcher state are now
surface-local. Compatibility APIs still target the default surface, but new
surface-aware APIs preserve the surface id wherever a document-local id crosses
runtime boundaries.

### Implemented Surface Model

`UiSurface` is now the engine primitive for a retained UI universe. It owns:

- `UiSurfaceDesc`: name, kind, order, output rect, visibility, enabled state,
  input participation, and render participation.
- One `UiDocument`.
- One ordered layer stack through the document.
- One `UiInputDispatcher`.
- One `UiFocusManager`.
- One `UiModalManager`.
- One `UiMountManager`.
- Screen-to-surface coordinate conversion.

Implemented surface kinds are:

- `Screen`
- `Viewport`
- `RenderTarget`
- `World`
- `Window`
- `Custom`

Only screen-rect extraction is implemented now. The other kinds are stable
engine vocabulary for future render-target, world-space, and window backends.

### UiSystem Surface Facade

`UiSystem` now owns the surface registry and remains the compatibility facade.
Existing methods such as `createNode(...)`, `createLayer(...)`,
`focusManager()`, `modalManager()`, `createMount(...)`, and
`mountComposition(...)` still operate on `UI_DEFAULT_SURFACE`.

New surface-aware APIs include:

- `createSurface(...)`
- `destroySurface(...)`
- `setSurfaceDesc(...)`
- `findSurface(...)`
- `surfaceIds()`
- `createLayer(surface, ...)`
- `createNode(surface, ...)`
- `setLayout(surface, ...)`
- `setState(surface, ...)`
- `setPayload(surface, ...)`
- `setTextFont(surface, ...)`
- `pushModal(surface, ...)`
- `createMount(surface, ...)`
- `mountComposition(surface, ...)`
- `compositionFromMount(surface, mount)`
- `compositionSurface(composition)`
- `extractSurfaceCommandsRef(surface, ...)`

Composition records now store both surface id and mount id. The internal
composition lookup key is `(surface, mount)`, because mount ids remain
surface-local. Text bindings and command caches are also per surface because
node handles remain document-local.

### Input Routing

`UiSystem::dispatchInput(...)` still executes once per engine frame. It now:

1. Selects the top ordered visible/enabled/input-enabled surface containing the
   screen-space pointer.
2. Falls back to a surface with active pointer ownership when the pointer leaves
   its rect during a held interaction.
3. Routes cancel/back to the highest ordered input-enabled surface with modal or
   keyboard ownership.
4. Converts the screen-normalized input frame into surface-local coordinates.
5. Dispatches through that surface's `UiInputDispatcher`.
6. Propagates generated events through the selected surface's retained tree.

This is intentionally still a first version. It establishes the ownership
boundary and surface-local routing without implementing world ray projection,
multi-pointer routing, multi-window event sources, or controller navigation.

### Focus And Modal Isolation

Focus and modal state are now surface-local:

- Surface A can have keyboard focus while Surface B has separate keyboard focus.
- Pointer capture, hover, active pointer, text focus, and navigation focus live
  inside the owning surface.
- Modal stacks are independent per surface.
- Cancel routing targets the selected surface's top modal owner.
- Destroying a surface clears that surface's focus, capture, active pointer,
  modal stack, mounts, and compositions without touching the default surface.

`UiSystem::pushModal(surface, desc)` resolves a missing modal layer from the
modal owner or owner mount before falling back to the surface default layer.
This keeps modal policy surface-local while avoiding repetitive feature code for
the common "modal owner already lives on a layer" case.

### Composition And Mount Integration

`UiCompositionContext` now includes `surface`. Surface-aware compositions should
use `context.surface` when they call back into `UiSystem`, for example:

`context.ui.createNode(context.surface, UiNodeType::Rect, context.root)`

Existing compositions that use default-surface APIs continue to work on the
default screen surface. New reusable compositions can mount into any surface and
keep their handles private, but those handles remain meaningful only inside the
composition's surface.

### Render Extraction

`UiSystem::extractCommandsRef(...)` now extracts visible/render-enabled surfaces
in surface order. Each surface resolves its document into a surface-local
`UiCommandBuffer`; the facade appends those commands into the final buffer after
transforming vertex positions through the surface output rect.

The current Vulkan UI stage still renders the final combined command buffer as a
screen-space overlay. Future render-target, world-space, split-screen, and
multi-window backends should reuse the same surface ownership model and replace
or extend the extraction target.

### Validation

Added `ui_surface_runtime`, covering:

- default surface compatibility
- multiple surfaces
- overlapping surface ordering
- hidden surface input routing
- input-disabled surface routing
- surface-local coordinate conversion
- surface-local focus isolation
- surface-local modal isolation
- cancel routing to a surface modal
- mount/composition ownership on a custom surface
- destroyed surface cleanup
- multi-surface render extraction
- clear restoring only the default surface

The existing layer, input-dispatcher, focus-manager, modal-manager, mount,
composition, and event-propagation runtime tests continue to pass.

### Remaining Single-Surface Compatibility Paths

The default surface compatibility path remains intentionally broad:

- Game UI builders still call default-surface `UiSystem` APIs.
- Most game features still store raw handles without storing a surface id.
- `ctx.ui->dispatchResult()` still exposes only the selected surface's last
  frame result rather than a history of all surfaces.
- `RenderingRuntime` currently creates only the default screen surface.
- The Vulkan UI stage consumes one combined screen-space command buffer.
- World-space, render-target, split-screen, VR/AR, and multi-window surfaces are
  not implemented yet.
- `UiInputFrame` still represents one primary pointer in screen-normalized
  coordinates.

These are compatibility paths, not blockers. The important ownership boundary is
now in place.

### Next Primitive

The single highest-value missing primitive is now the **Layout Engine**.

With surfaces implemented, the runtime can finally answer where UI exists and
which coordinate/input/render universe owns it. The next largest architectural
limit is that authored UI is still mostly absolute or anchor-based retained
node placement. That forces every reusable composition to hardcode geometry,
which weakens the value of surfaces, compositions, styling, animation, and
future widgets.

Layout should come before styling, animation, navigation, drag/drop, data
binding, and a widget framework because those systems need stable geometry
contracts:

- Styling needs metrics and layout slots before skins can scale consistently.
- Animation needs layout participation for transitions and reflow.
- Navigation needs spatial relationships and focusable layout groups.
- Drag/drop needs hit regions and container-relative placement rules.
- Data binding and widgets need reusable structural layout instead of raw
  handle geometry.

The next milestone should introduce surface-local layout primitives such as
stack, dock, grid, overlay, constraints, typed units, viewport/surface units,
content measurement, and invalidation. It should preserve the current retained
node model while replacing hand-authored fixed geometry as the default way to
compose UI.

## 14. Phase 8 Implementation Report

### Manual Layout Investigation

Before Phase 8, the runtime owned interaction, lifetime, composition, and
surface boundaries, but feature code still owned most geometry. The repeated
patterns were:

- VN choice rows computed row height, row gap, and y offsets manually.
- Dungeon interaction prompt code converted scene-viewport pixels to normalized
  offsets every frame.
- Inventory and merchant UI builders compute panel, grid, cell, icon, label,
  and button rectangles manually.
- Menu and skill-tree builders use local helpers for fixed normalized
  rectangles, content offsets, clipped panels, and graph node placement.
- Engine tool widgets still author fixed panel/control rectangles directly.
- HUD and prompt builders mostly create anchored/fixed retained nodes and then
  sync systems update visibility/text.

The geometry that belongs in the engine is repeated container behavior:
stacking, overlay alignment, grid/dock placement, scroll clipping, preferred
size, min/max constraints, aspect ratio, padding, margins, and text
measurement. Geometry that remains composition-owned is feature intent: which
controls exist, which data they show, and feature-specific spatial layouts such
as a skill-tree graph or inventory item footprint until drag/drop/widgets are
introduced.

### Implemented Layout Architecture

The retained `UiNode` hierarchy is now also the layout hierarchy. A separate
layout tree was rejected for this phase because it would duplicate retained
ownership before the engine has a widget framework, semantic tree, or style
tree. Keeping layout on `UiNode` preserves compatibility and gives
compositions one authored tree:

`UiSurface -> UiDocument -> UiLayer -> UiMount -> UiComposition -> UiNode/Layout`

`UiDocument::updateLayout(...)` now performs a two-pass layout update:

1. Measure visible nodes bottom-up.
2. Arrange nodes top-down from the surface root.

Measurement computes `UiComputedLayout::measuredSize` from explicit
constraints, content measurement, primitive intrinsic size, child layout, and
padding. Arrangement computes `bounds`, `contentRect`, and `clipRect`.
Rendering and hit testing consume only computed layout.

`UiLayoutSpec` gained:

- `UiLayoutMode`: `Canvas`, `Stack`, `Grid`, `Dock`, `Overlay`, `Scroll`,
  `Aspect`, and `Constraint`.
- `UiLayoutLength`: `Auto`, `Normalized`, `Percent`, `SurfaceWidth`,
  `SurfaceHeight`, `ParentWidth`, `ParentHeight`, `Content`, `Em`, and
  preliminary `Pixels`.
- `UiLayoutConstraints`: preferred/min/max sizes, grow/shrink, aspect ratio,
  and alignment.
- Container fields for stack axis/alignment/gap, grid rows/columns/gaps/spans,
  dock edge, margins, padding, clipping, and content offset.

`Canvas` is the compatibility adapter for existing absolute and anchored
layouts. Existing builders can keep using old `positionMode`, `anchorMin`,
`anchorMax`, `offsetMin`, `offsetMax`, `fixedWidth`, and `fixedHeight` fields.
New compositions should prefer container intent and constraints.

### Text Measurement And Surface Integration

`UiSystem::extractSurfaceCommandsRef(...)` now passes a text measurement
callback into `UiDocument::updateLayout(...)`. The callback uses surface-local
text bindings and `GlyphLayoutEngine::measureUiText(...)`. Dispatch-side layout
still works without renderer extraction by falling back to approximate text
measurement.

Layout remains surface-local. Each `UiSurface` owns one document and each
document computes normalized local geometry before the surface extraction step
maps vertices into the surface output rect.

### Feature Demonstrations

Two representative migrations prove the intended authoring model:

- `InteractionPromptComposition` now turns its mount root into an `Overlay`
  host and lets the prompt root declare preferred size, bottom margin, and
  center/end alignment. The composition no longer receives viewport pixel
  dimensions or recomputes absolute coordinates every frame.
- `VNDialogueUi` choice rows now use a vertical `Stack` on the choice layer.
  Choice buttons declare measured row height through constraints; the stack
  owns row placement and gaps.

The old interaction-prompt handle path remains as a compatibility fallback and
continues to perform manual viewport conversion. Inventory, merchant, menus,
skill tree, HUD, and tool widgets still contain substantial manual layout and
should migrate incrementally as they are touched.

### Validation

Added `ui_layout_runtime`, covering:

- stack layout
- grid layout
- dock layout
- overlay alignment
- aspect-ratio constraints
- scroll clipping and content offset
- text measurement invalidation
- nested layout and percent constraints
- anchored layout compatibility
- surface-local layout extraction

Regression status:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`

all pass in the engine release build.

### Remaining Layout Debt

- Dirty tracking is still document-level. The next performance improvement is a
  subtree invalidation graph with cached measure/arrange results.
- Text wrapping measurement is not yet width-constrained by the final arranged
  slot; a mature text layout pass should remeasure when final width changes.
- `Scroll` provides clipping and content offset but not scrollbars, scroll
  range calculation, virtualized children, wheel ownership, or kinetic input.
- Pixel units are preliminary and need DPI/pixel-density semantics.
- Existing feature UI remains mostly manually positioned.
- Layout can consume theme metrics through compositions, but `UiLayoutSpec`
  itself does not yet support metric-backed length values.

### Next Primitive

With Layers, Dispatch, Focus, Modals, Mounts, Compositions, Event Propagation,
UiSurface, and Layout implemented, the single highest-leverage next primitive is
the **Styling / Theme System**.

Styling should precede animation, navigation, drag/drop, data binding, and a
widget framework because layout now provides geometry slots, but visuals and
metrics are still duplicated across features. A theme system centralizes color,
typography, spacing, borders, panel skins, state visuals, and metric tokens. It
will make future widgets and compositions reusable across game HUDs, VN
frontends, editor tools, terminals, and debug panels without copying colors,
font scales, padding, and button-state code into every feature.

Animation depends on stable style/layout properties to transition. Navigation
depends on consistent focusable widgets. Drag/drop depends on styled hit
targets and layout regions. Data binding needs reusable widgets. A widget
framework should come after layout and styling define how controls size and look.

## 15. Phase 9 Implementation Report

### Styling Investigation

Before Phase 9, the runtime owned interaction, lifetime, surfaces, composition,
events, and geometry, but presentation was still scattered across features.
The repeated patterns were:

- `UiRectData` colors authored directly in combat, dungeon, inventory, merchant,
  menu, HUD, and tool builders.
- `UiTextData` colors and scales authored directly by builders and sync systems.
- `UiPanelSkin` and `UiPanelStateSkin` used for panel-like visuals, but without
  a generic registry or style-class lookup.
- `VNPresentationProfile` structured dialogue panel, nameplate, choice, and text
  appearance for VN only.
- `EditorTheme` and `ToolTheme` structured editor/tool presentation above the
  retained runtime, but widgets resolved those tokens manually into payloads.
- Inventory, merchant, menus, skill-tree, HUD, and prompt code each carried
  local spacing, padding, button, text, and panel constants.

The presentation that belongs in the engine is the reusable mechanism:
semantic tokens, metrics, typography, skins, style classes, state styles,
inheritance, and theme resolution. Vocabulary remains application-owned. The
engine should know how to resolve `PrimaryButton`; it should not know what a
merchant, dialogue choice, inventory slot, or dungeon prompt means.

### Implemented Styling Architecture

The implemented flow is:

`UiComposition -> style request -> UiSurface theme -> computed style -> retained visual primitive`

`UiTheme` now owns:

- semantic color lookup through string tokens.
- metric lookup for spacing and sizing values.
- typography lookup for font asset, scale, weight, line height, letter spacing,
  and paragraph spacing.
- skin lookup for panels and future skinnable primitives.
- panel state skin lookup for compatibility with existing panel helpers.
- style class lookup with parent class inheritance.
- state-specific properties for hover, pressed, focused, selected, disabled,
  active, checked, expanded, and error states.
- parent theme inheritance, with inherited style classes resolving color and
  typography tokens through the active child theme.

`UiStyle` now carries a style-class request and explicit compatibility
overrides. This keeps raw payload styling working while allowing new
compositions to name presentation intent.

`UiSurface` owns the active theme. `UiSystem::setTheme(...)`,
`UiSystem::theme(...)`, and surface-aware overloads expose theme assignment and
lookup. Theme changes mark the surface document layout and visuals dirty because
typography and metrics can affect measurement, not only rendering.

`UiDocument::updateLayout(...)` accepts the active theme so text measurement
uses resolved typography. `UiDocument::rebuildVisualList(...)` accepts the same
theme so node payloads are converted into visual primitives through computed
style. Rendering remains independent; the command resolver consumes already
resolved primitives and does not know theme policy.

### Feature Demonstration

The dungeon interaction prompt now has a feature-owned
`src/dungeon/ui/DungeonUiTheme.h`. `DungeonUiController` assigns that theme to
the default screen surface. `InteractionPromptBuilder` consumes theme metrics
for prompt size, bottom margin, and text inset, and assigns style classes for
the prompt panel and label. `InteractionPromptComposition` updates text content
without reauthoring text color or font scale.

The old non-composition interaction-prompt sync path remains as a compatibility
fallback and still contains hardcoded presentation constants. Inventory,
merchant, menus, skill tree, HUD, VN profiles, and editor tools still have
substantial local styling and should migrate incrementally.

### Validation

Added `ui_theme_runtime`, covering:

- semantic color lookup.
- metric lookup.
- typography lookup.
- style class inheritance.
- state style resolution.
- missing-style fallback.
- parent-theme inheritance with child token override.
- panel skin and panel state skin lookup.
- theme-driven visual primitive colors.
- runtime theme switching.
- surface-local themes.
- payload-color compatibility.
- typography-driven layout and visual primitive resolution.
- explicit node style overrides.

Regression status:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`
- `ui_theme_runtime`

all pass in the engine release build.

### Remaining Styling Debt

- Themes are C++ authored. Data-driven theme assets and hot reload are future
  work.
- Style properties include border and shadow fields, but retained primitives do
  not render borders, radii, or shadows yet.
- Metrics are available through `UiTheme`, but `UiLayoutSpec` still stores
  numeric values directly. A later pass should add metric-backed layout lengths
  or composition helpers.
- Font weight, line height, letter spacing, and paragraph spacing are stored but
  not fully consumed by text layout.
- There is no selector language, cascade, or widget state machine. Style classes
  are intentionally explicit for now.
- Existing feature UI remains mostly hardcoded and must migrate gradually.
- Editor/VN/game presentation profiles should become theme builders or theme
  adapters instead of independent styling systems.

## 16. Phase 10 Implementation: Widget Framework

### Investigation

The retained UI runtime had reached a point where ownership, interaction,
geometry, and presentation were engine-owned, but feature code still assembled
common controls directly from primitive nodes. Repeated patterns included:

- menu buttons, VN choice buttons, tool buttons, and prompt buttons built as a
  rect or panel plus a text child, then tested by raw handle comparison.
- panels and dialogue boxes built as rect/nine-slice containers with local
  padding conventions.
- labels, separators, scroll containers, spacers, and progress indicators
  reauthored by each feature.
- inventory and merchant slots, skill nodes, editor rows, and toolbar buttons
  combining the same panel/image/text/overlay structure with feature-local
  hover/pressed/selected behavior.

Those patterns map cleanly to reusable widgets. The widget layer should absorb
control-local retained node ownership, local state, style requests, layout
intent, and semantic callbacks. Composition remains the owner of feature UI
behavior and lifetime; widgets remain clients of composition, layout, styling,
events, and retained nodes.

### Architecture

`UiWidget` is the base class for reusable composition-owned controls. A widget:

- owns a retained subtree root and any child handles it creates.
- receives retained `UiEvent` values through composition forwarding.
- translates pointer/focus state into local control state.
- requests theme style classes, typography, metrics, and layout intent.
- exposes semantic behavior, such as `UiButtonWidget::onPressed`.

A widget does not own surfaces, mounts, modal policy, focus management, event
propagation, rendering, or composition lifetime. `UiWidgetContext` deliberately
contains only the runtime access a widget needs: `UiSystem`, surface id,
surface-local document, resource provider, mount id, and composition root.

The initial engine widget set is intentionally small but representative:

- `UiLabelWidget`
- `UiPanelWidget`
- `UiButtonWidget`
- `UiImageWidget`
- `UiStackWidget`
- `UiSpacerWidget`
- `UiSeparatorWidget`
- `UiScrollViewWidget`
- `UiProgressBarWidget`

This set proves the core framework across text, containers, interactive
controls, layout containers, visual primitives, clipped containers, and
stateful value display without introducing gameplay vocabulary.

### Implementation

`UiWidget.h` implements the first widget framework in the rendering UI module.
Widgets use surface-aware `UiSystem` APIs and build ordinary retained nodes, so
primitive nodes remain the rendering substrate and all compatibility code
continues to work.

Important implementation decisions:

- Widgets are value objects owned by compositions. They are not registered as a
  global runtime system.
- Widgets keep child containers enabled for hierarchy traversal while leaving
  `interactable` false unless the widget itself should receive hits.
- `UiButtonWidget` consumes retained `PointerClick` target events and exposes a
  semantic pressed callback plus `consumePressed()` for compatibility callers.
- `UiPanelWidget`, `UiStackWidget`, and `UiScrollViewWidget` express layout
  containers through `UiLayoutSpec` instead of computing positions.
- `UiSeparatorWidget` and `UiProgressBarWidget` use theme style classes
  (`Separator`, `ProgressBar.Track`, `ProgressBar.Fill`) added to the default
  theme.

### Feature Demonstration

The dungeon interaction prompt now uses `UiPanelWidget` and `UiLabelWidget`
inside `InteractionPromptComposition`. The composition still owns the prompt's
feature state, while widgets own the retained panel/text subtrees and consume
the dungeon theme's style and metric requests.

The VN dialogue choice rows now use `UiButtonWidget`. The public
`consumeClickedChoiceIndex()` flow remains compatible, but the composition now
forwards retained click events into the buttons and consumes the widget's
semantic pressed result instead of treating the choice as a hand-assembled
panel plus label.

### Validation

Added `ui_widget_runtime`, covering:

- widget creation through composition mount.
- nested panel/stack/label/button/separator/progress subtrees.
- theme-driven widget visual primitives.
- layout-driven stack geometry and progress fill geometry.
- event propagation into `UiButtonWidget`.
- hover, pressed, and focused state reflection on button nodes.
- semantic button pressed callback.
- click consumption and default prevention.
- disabled button behavior.
- composition destruction cleaning widget subtrees.

Regression status now includes:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`
- `ui_theme_runtime`
- `ui_widget_runtime`

### Remaining Widget Debt

- Widgets are C++ authored value objects. There is no data-driven widget asset
  format or factory registry yet.
- The first set does not include text fields, checkboxes, sliders, dropdowns,
  tabs, list views, tree views, tables, property rows, or toolbars.
- Widget event forwarding is explicit from composition code. A future event
  registration layer can reduce boilerplate once widget identity and navigation
  metadata mature.
- Button activation now supports pointer clicks and navigation submit. Richer
  widget roles, value editing, and text-entry behavior remain future work.
- Existing editor widgets, menu builders, inventory slots, merchant slots, HUD
  widgets, and many VN panel elements still use primitive-node builders and
  should migrate gradually.

### Next Primitive

With Layers, Dispatch, Focus, Modals, Mounts, Compositions, Event Propagation,
UiSurface, Layout, Styling, and Widgets implemented, the single highest-value
remaining primitive is the **Navigation Graph**.

Widgets create the first reusable focusable controls. The next architectural
gap is not visual polish; it is how keyboard, controller, Steam Deck, console,
and accessibility-oriented input move between those controls predictably.
Navigation should build directly on the focus manager, event propagation,
layout geometry, style states, and widget roles. Animation, drag/drop, data
binding, and virtualization all benefit from widgets, but none of them solves
the engine-wide question of which control receives non-pointer interaction.

Navigation Graph should therefore precede animation, drag/drop, data binding,
accessibility, and large-list virtualization. It will turn the widget layer from
pointer-capable controls into fully navigable engine UI primitives.

## 17. Phase 11 Implementation: Navigation Graph

### Investigation

The runtime already distinguished persistent focus from per-frame dispatch, but
non-pointer movement still had no engine policy. Current assumptions were:

- Pointer clicks assigned keyboard focus through `UiFocusManager`.
- `UiFocusManager` had a navigation-focus slot per input device, but no graph
  computed traversal.
- `UiInputFrame` carried pointer and cancel state only.
- `RenderingRuntime` only mapped mouse primary and cancel into retained UI
  dispatch.
- VN choice buttons had become widgets, but activation was still pointer-first.
- Menus, inventory, merchant, skill tree, and editor/tool controls still owned
  feature-local interaction or polling compatibility paths.

The ownership split is now:

- Widgets declare navigation metadata and semantic activation behavior.
- `UiNavigationGraph` chooses traversal targets.
- `UiFocusManager` owns focused, navigation-focused, and restored nodes.
- `UiInputDispatcher` creates navigation events and publishes dispatch result
  ownership.
- Compositions and widgets react to retained navigation events.

### Architecture

`UiNavigationGraph` is surface-local. It stores focusable navigation nodes
identified by retained `UiHandle`, with metadata for:

- focusable/enabled state.
- semantic role.
- focus scope id.
- navigation group.
- explicit tab index.
- wrap policy.
- explicit neighbor overrides for `Up`, `Down`, `Left`, `Right`, `Next`, and
  `Previous`.
- submit activation participation.

Traversal is hybrid:

- Explicit neighbors win when present.
- `Next` and `Previous` use explicit tab index first, then computed spatial
  top-left order, with registration order only as a stable final tie-breaker.
- Directional movement uses computed layout bounds and chooses the nearest
  candidate in the requested direction.
- Disabled, hidden, destroyed, wrong-scope, wrong-group, and modal-blocked
  nodes are skipped.
- Modal navigation remains inside the top modal owner or layer when the modal
  consumes navigation.

The graph does not own focus. A successful movement calls
`UiFocusManager::setNavigationFocus(...)` and `requestFocus(...)`. Focus events
continue to come from focus changes, not from the graph owning focus state.

### Implementation

Implemented:

- `UiNavigationTypes.h`
- `UiNavigationGraph.h`
- `UiNavigationGraph.cpp`
- surface-local `UiSurface::navigationGraph()`
- `UiSystem::navigationGraph(...)`, `registerNavigationNode(...)`, and
  `unregisterNavigationNode(...)`
- `UiInputFrame` navigation request fields.
- `UiDispatchResult` navigation ownership fields.
- retained `NavigationMove`, `NavigationSubmit`, `NavigationWrap`, and
  `NavigationBlocked` event generation.
- default engine input bindings for arrows, Tab, Shift+Tab, Enter, Space, and
  Escape.
- `RenderingRuntime::dispatchUiInput(...)` mapping of engine UI navigation
  actions into `UiInputFrame`.

`UiSystem` also unregisters navigation nodes during layer, node, and mount
teardown before the document loses ancestry information. Each surface clears
its graph when cleared.

### Widget Integration

`UiButtonWidget` now registers itself as a navigation node by default and
handles `NavigationSubmit` as semantic activation. Button descriptors can set
navigation role, group, tab index, and wrap behavior. Pointer-driven behavior
continues unchanged.

### Feature Demonstration

VN dialogue choice buttons are now graph-driven because they are
`UiButtonWidget` instances. A navigation move focuses a choice, and navigation
submit activates the same semantic choice path as a pointer click.

The existing handle-backed game menu now registers its major buttons, settings
rows, and video rows as navigation nodes. The menu remains a compatibility
polling path, but `NavigationSubmit` is translated into the existing clicked
handle path so activation comes from the graph.

### Validation

Added `ui_navigation_runtime`, covering:

- tab order independent of creation order.
- Shift+Tab/previous traversal.
- directional movement through computed layout bounds.
- disabled node skipping.
- hidden node skipping.
- wrap behavior.
- widget activation through navigation submit.
- submit event consumption/default prevention by `UiButtonWidget`.
- modal navigation restriction.
- surface-local navigation routing.
- destroyed composition pruning.

Regression status now includes:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`
- `ui_theme_runtime`
- `ui_widget_runtime`
- `ui_navigation_runtime`

### Remaining Navigation Debt

- Navigation nodes are C++ registered. There is no data-driven navigation
  metadata format yet.
- There is no controller/gamepad physical-device mapping yet; the runtime now
  consumes abstract navigation intent.
- Widget roles are stored but not yet consumed by accessibility or specialized
  value-editing behavior.
- There is no repeat-rate policy for held navigation actions.
- Existing inventory, merchant, skill-tree, editor, and tool controls still own
  manual selection or drag-like pointer behavior.
- Explicit neighbor editing tools do not exist yet.

### Next Primitive

With Layers, Dispatch, Focus, Modals, Mounts, Compositions, Event Propagation,
UiSurface, Layout, Styling, Widgets, and Navigation implemented, the single
highest-value remaining primitive is the **Drag & Drop Runtime**.

Drag/drop should precede animation, data binding, accessibility, and
virtualized lists because it is the remaining core interaction ownership model.
Inventories, merchant grids, editor docking, asset browsers, graph editors,
resize handles, viewport gizmos, and world terminals all need the same engine
answer to: who owns the drag, what is the payload, what is the drop target, how
does capture behave, and how does cancellation restore state? The runtime now
has the necessary foundations: surfaces, focus, modal policy, event
propagation, layout bounds, widgets, and navigation roles.

## 18. Phase 12 Implementation: Drag & Drop Runtime

### Investigation

Existing direct-manipulation behavior fell into three groups:

- Inventory and merchant UI compute raw pointer positions, grid cells, hovered
  items, selected instances, and click/drag-like ownership in feature handlers.
- Skill-tree panning owns canvas drag state directly in the feature UI state.
- Engine tools own editor direct manipulation: shared tool sliders read
  pressed handles and pointer positions, world picking/gizmos own viewport
  capture and axis drag state, and future docking/asset/graph tools would
  otherwise repeat that ownership pattern.

The runtime responsibility is the interaction policy: source ownership,
payload identity, pointer capture, compatible target resolution, cancellation,
drop acceptance/rejection, cleanup, and event delivery. The feature
responsibility remains meaning: what an inventory item, asset, graph node,
skill node, gizmo axis, or editor object actually does when dropped.

### Architecture

`UiDragDropManager` is surface-local. It owns:

- registered drag sources.
- registered drop targets.
- an opaque typed payload (`UiDragPayload` type/id/label/object).
- armed and active drag state.
- pointer id, source, current target, previous target, start position, and
  pointer deltas.
- pointer capture through `UiFocusManager`.
- compatible drop-target lookup through the retained node parent path.
- cancellation and cleanup when nodes, mounts, layers, or surfaces disappear.

Dragging is a state machine:

`Idle -> Armed -> Dragging -> Dropped | Rejected | Cancelled -> Idle`

Arming happens on pointer press over a registered source. A source-controlled
threshold starts the drag. Starting captures the pointer. Movement resolves the
nearest compatible registered target by walking from the hit-tested node toward
the root. Release over a compatible target emits an accepted drop; release
without a compatible target emits rejection. Cancel input emits cancellation.
Dragging suspends navigation so focus traversal and direct manipulation do not
compete for the same frame.

Payloads are deliberately generic. The engine stores semantic identity and an
optional shared opaque object but never interprets gameplay/editor data. This
keeps inventory items, assets, graph nodes, editor selections, and future
modded payloads outside engine vocabulary.

### Implementation

Implemented:

- `UiDragTypes.h`
- `UiDragDropManager.h`
- `UiDragDropManager.cpp`
- surface-local `UiSurface::dragDropManager()`
- `UiSystem::dragDropManager(...)`
- `UiSystem::registerDragSource(...)`
- `UiSystem::registerDropTarget(...)`
- source/target cleanup during node, mount, layer, and surface teardown.
- drag fields in `UiDispatchResult`.
- drag fields in `UiEvent`.
- retained drag event generation in `UiInputDispatcher`.
- optional drag source/drop target descriptors on `UiPanelWidget` and
  `UiButtonWidget`.

Drag events now use the existing capture/target/bubble propagation pipeline:

- `DragStart`
- `DragMove`
- `DragEnter`
- `DragLeave`
- `DragOver`
- `DragDrop`
- `DragAccept`
- `DragReject`
- `DragCancel`
- `DragEnd`

### Feature Demonstration

The shared engine tool slider builder now registers slider tracks as drag
sources with an `engine.tool.slider` payload. Existing panels still compute
their slider value from pointer position, but pointer capture and drag lifecycle
now belong to the runtime instead of being an implicit pressed-handle behavior.
This migrates a representative editor/tool direct-manipulation path without
rewriting the tools module.

The widget runtime test also demonstrates widget-authored drag sources and drop
targets through `UiPanelWidget` descriptors, proving the intended composition
authoring model.

### Validation

Added `ui_drag_drop_runtime`, covering:

- drag begin.
- drag move.
- pointer capture.
- compatible target enter/over/drop.
- accepted drop.
- rejected drop.
- click suppression after drag.
- cancelled drag.
- destroyed target cleanup.
- modal pointer blocking.
- surface-local drag capture and cross-surface rejection.

Regression status now includes:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`
- `ui_theme_runtime`
- `ui_widget_runtime`
- `ui_navigation_runtime`
- `ui_drag_drop_runtime`

### Remaining Drag/Drop Debt

- Inventory and merchant grids still own gameplay-specific drag/selection
  logic and should migrate to retained slots registered as drag sources/drop
  targets.
- Skill-tree panning still owns canvas drag state directly.
- Viewport world picking and gizmos still own world-space capture and domain
  transforms; they can later consume the same drag ownership model once
  viewport/world-space drag targets exist.
- Drag previews are represented architecturally by payload/source state but do
  not yet have a first-class rendered preview helper.
- Cross-surface drag is intentionally rejected by the first implementation.
  Future editor windows/render targets can add explicit cross-surface policy
  without changing source/target registration.

## 19. Phase 13 Implementation: Animation Runtime

### Investigation

Existing time-based UI presentation was split across unrelated systems:

- `VNStage` owns sprite position, scale, rotation, alpha, shake, and dimming
  tweens directly for visual-novel stage presentation.
- Widget hover/pressed/focused visuals were immediate style or payload changes.
- Modal open/close and prompt visibility changed immediately.
- Layout changes, scroll offsets, and theme state changes snapped instantly.
- Scene texture/particle/object animation systems exist, but they are world
  presentation systems rather than retained UI property ownership.

The reusable interpolation primitive already existed in `modules/tween/Tween.h`.
The UI runtime should consume that easing/progress math, while owning retained
UI lifecycle, property targeting, interruption, cleanup, and integration with
surfaces, mounts, widgets, modals, layout, and themes.

### Architecture

`UiAnimationManager` is surface-local. It owns active retained UI property
timelines and never owns rendering, widgets, focus, modal policy, or gameplay
meaning.

Implemented animatable properties:

- `Opacity`
- `BackgroundColor`
- `ForegroundColor`
- `RectColor`
- `TextColor`
- `ImageTint`
- `NineSliceTint`
- `LayoutOffsetMin`
- `LayoutOffsetMax`
- `LayoutFixedSize`
- `LayoutContentOffset`

The runtime model is:

`Widget/Composition/Modal -> desired presentation -> UiAnimationManager -> retained node properties -> renderer`

Animation descriptors carry a target node, property, optional start value,
target value, duration, delay, easing, repeat/ping-pong/loop flags, snap policy,
and optional completion callback. Starting a new animation for the same
`(node, property)` interrupts the old one and uses the current retained value as
the new start unless a caller supplies an explicit start value.

Style-state transitions are theme-driven. A widget can request transition to a
target `UiStyleState`; the manager resolves the previous and target presentation
through `UiTheme`, then animates explicit node style overrides. Themes remain
presentation data. Widgets do not interpolate values.

Layout animation remains separate from layout solving. The layout engine
continues to compute geometry; animation writes authored layout properties such
as offsets, fixed size, and scroll/content offset over time. Rendering consumes
the resulting computed layout after extraction updates dirty documents.

Modal integration is policy-level. `UiModalDesc` can request open/close opacity
timing for its owner. The modal manager still owns modal stack policy and focus
restoration; the animation runtime only owns temporal presentation.

### Implementation

Implemented:

- `UiAnimationTypes.h`
- `UiAnimationManager.h`
- `UiAnimationManager.cpp`
- surface-local `UiSurface::animationManager()`
- `UiSystem::animationManager(...)`
- `UiSystem::animate(...)`
- `UiSystem::animateOpacity(...)`
- `UiSystem::transitionStyleState(...)`
- `UiSystem::cancelAnimation(...)`
- `UiSystem::cancelAnimations(...)`
- `UiSystem::updateAnimations(dt)`
- automatic animation cleanup during node, mount, layer, and surface teardown.
- render-frame ticking in `RenderingRuntime::renderFrame(...)` before UI
  command extraction.
- optional state animation descriptors on `UiButtonWidget`.
- optional modal owner open/close opacity transitions on `UiModalDesc`.

The core target now includes the shared tween header directory so UI animation
uses `gts::tween::Tween<float>`, `TweenEase`, and easing functions instead of
duplicating interpolation policy.

### Feature Demonstration

Two representative transitions now use runtime-owned animation:

- `UiButtonWidget` can animate theme-resolved state transitions such as
  normal-to-hover background changes through `UiStyleTransitionDesc`.
- `InteractionPromptComposition` fades its panel and label in/out through
  `UiSystem::animate(...)` and `animateOpacity(...)` rather than feature-owned
  interpolation.

### Validation

Added `ui_animation_runtime`, covering:

- opacity interpolation and completion.
- interruption from the current animated value.
- cancellation with completion.
- layout offset animation.
- theme-driven button hover transition.
- mount destruction cleanup.
- modal open/close transition hooks.
- surface-local animation isolation.

Regression status now includes:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`
- `ui_theme_runtime`
- `ui_widget_runtime`
- `ui_navigation_runtime`
- `ui_drag_drop_runtime`
- `ui_animation_runtime`

### Remaining Animation Debt

- VN stage animation still owns sprite/stage tweens directly because it is a
  domain-specific presentation runtime, not retained UI node animation.
- Inventory, merchant, menus, and many editor tools still snap most retained
  visual changes immediately.
- Drag previews and drop feedback have hooks through drag events and animatable
  properties but no first-class preview animation helper yet.
- Modal close animation does not delay modal pop or mount destruction; future
  transition policy should coordinate deferred teardown.
- There is no reduced-motion/accessibility policy, animation graph authoring,
  or serialized animation asset format yet.

### Phase 13 Conclusion

With Layers, Dispatch, Focus, Modals, Mounts, Compositions, Event Propagation,
UiSurface, Layout, Styling, Widgets, Navigation, Drag & Drop, and Animation
implemented, the next required primitive was **Data Binding** because the
remaining duplication was synchronization code that manually copied model state
into retained UI every frame.

## 20. Phase 14 Implementation: Data Binding Runtime

### Investigation

Current retained UI synchronization still appears in many feature systems:

- `InteractionPromptSyncSystem` reads `InteractionPromptUiState`, writes prompt
  composition state, and previously caused retained label/opacity updates.
- `DungeonHudSyncSystem` caches floor/debug/minimap revisions and manually
  writes text payloads, grid payloads, visibility, and layout.
- `CombatUiSyncSystem` writes action menu labels, selected row colors, HP text,
  HP fill widths, log lines, banner alpha, and enemy nameplate state every
  frame.
- `InventoryUiSyncSystem` computes panel geometry, visibility, item icon
  payloads, stack labels, tooltip text, and hover/drag colors manually.
- `MenuUiSyncSystem` writes menu button labels, settings labels, dropdown rows,
  action bindings, visibility, and enabled state manually.
- `VNDialogueComposition` delegates to `VNDialogueUi::sync(...)`, which mirrors
  dialogue runtime state into retained dialogue text, speaker text, choices,
  and choice button state.
- Engine tool panels such as `SceneGizmoPanel` manually update button text,
  toggle colors, status labels, and visibility.

The repeated generic patterns are text formatting, boolean visibility/enabled
state, progress/fill geometry, style/color state, payload replacement, and
conditional layout invalidation. The application-specific parts are inventory
item meaning, merchant pricing rules, dialogue graph semantics, combat state
rules, and editor command behavior. Those remain application logic. The runtime
now owns the synchronization relationship between state and retained targets.

### Architecture

`UiBindingManager` is surface-local and owns one-way binding records. A binding
record contains:

- target retained node.
- target property.
- source read callback.
- optional source revision callback.
- optional source lifetime callback.
- optional transform.
- optional text formatter.
- owner mount metadata.
- optional animation timing for animatable targets.

The binding source model is deliberately small:

- `UiObservable<T>` is a lightweight versioned value wrapper for common
  composition/widget state.
- `bindObservable(...)` produces binding sources from observable references or
  `shared_ptr` observables.
- `UiBindingSource` supports arbitrary application/ECS/editor data through
  type-erased read/revision/lifetime callbacks.
- Computed and multi-source bindings are just custom source callbacks with a
  combined revision value.

Implemented target properties include text, visibility, enabled state,
interactable state, opacity, progress, rect color, text color, image asset,
image tint, style class, full layout, layout offsets, layout anchors, fixed
size, and content offset. Text bindings can format any binding value into
display text. Progress bindings write layout anchor state and can animate via
the animation runtime.

Bindings update before animation. That ordering means bindings write desired
retained state, then `UiAnimationManager` interpolates animatable changes for
the current frame. Text and visibility changes invalidate layout or visual state
through the existing `UiDocument` dirty path. Bindings do not bypass events,
focus, modal policy, widgets, or rendering.

### Implementation

Implemented:

- `UiBindingTypes.h`
- `UiBindingManager.h`
- `UiBindingManager.cpp`
- surface-local `UiSurface::bindingManager()`
- `UiSystem::bindingManager(...)`
- `UiSystem::bind(...)`
- `UiSystem::unbind(...)`
- `UiSystem::unbindTarget(...)`
- `UiSystem::updateBindings()`
- automatic binding cleanup during node, mount, layer, and surface teardown.
- render-frame binding evaluation in `RenderingRuntime::renderFrame(...)`
  before UI animation and command extraction.
- widget convenience bindings for label text/visibility, panel visibility,
  button text/enabled/visibility, image asset/tint, and progress values.
- layout anchor animation support in `UiAnimationManager` so progress bindings
  can animate retained geometry.

### Feature Demonstration

`InteractionPromptComposition` now uses runtime data bindings for prompt text
and opacity state:

- `UiObservable<std::string>` drives the label text.
- `UiObservable<bool>` drives panel and label opacity through animated
  `UiBindableProperty::Opacity` bindings.
- the composition update only keeps nodes visible long enough for a fade-out to
  finish; it no longer copies text or starts opacity tweens manually.

The runtime tests also demonstrate computed progress binding through
`UiProgressBarWidget::bindValue(...)`, including animation from the previous
fill width to the new computed value.

### Validation

Added `ui_binding_runtime`, covering:

- one-way text binding.
- formatting.
- computed multi-source progress binding.
- progress animation integration.
- visibility and enabled state binding.
- layout invalidation from text changes.
- expired source cleanup.
- mount cleanup.
- surface-local binding ownership and surface cleanup.

Regression status now includes:

- `ui_layer_runtime`
- `ui_input_dispatcher_runtime`
- `ui_focus_manager_runtime`
- `ui_modal_manager_runtime`
- `ui_mount_runtime`
- `ui_composition_runtime`
- `ui_event_propagation_runtime`
- `ui_surface_runtime`
- `ui_layout_runtime`
- `ui_theme_runtime`
- `ui_widget_runtime`
- `ui_navigation_runtime`
- `ui_drag_drop_runtime`
- `ui_animation_runtime`
- `ui_binding_runtime`
- `ui_accessibility_runtime`

## 21. Phase 15 Implementation: Accessibility Runtime

### Investigation

Before this phase, semantic meaning was implicit and scattered:

- `UiButtonWidget` knew when it was pressed, but the runtime had no persistent
  role/name/state model for that control.
- `UiLabelWidget` rendered text, but no runtime tree could identify it as a
  label, status message, heading, or live update.
- `UiProgressBarWidget` owned value geometry through layout, but the current
  value and range were not available outside visual state.
- `UiNavigationGraph` stored navigation roles for traversal, but those roles
  were not an accessibility tree or platform-facing semantic model.
- `UiFocusManager` reflected focused flags into nodes, but no announcement
  stream observed focus transitions.
- `UiBindingManager` synchronized visual properties, but bound value changes
  could not produce semantic notifications.
- Feature UIs such as the interaction prompt and VN/dialogue paths expressed
  user-facing meaning only through visible text and feature state.

Runtime-owned semantics are now separated from domain meaning. Applications
still own what an item, quest, dialogue line, or editor asset means; widgets and
compositions describe how that meaning appears as generic UI roles, names,
values, ranges, and relationships.

### Architecture

`UiAccessibilityManager` is surface-local. It owns semantic descriptors keyed by
retained `UiHandle`, builds a semantic tree from the retained hierarchy, and
publishes an announcement queue. The semantic tree references nodes for bounds,
visibility, enabled state, focus state, and parent/child ordering, but it is not
the render tree. Decorative images and hidden nodes are omitted, and semantic
relationships such as `labelledBy`, `describedBy`, `controls`, `owns`,
`activeDescendant`, `popup`, and `tooltip` are stored independently from render
order.

Semantic roles are engine vocabulary, not gameplay vocabulary. The first role
set includes windows, dialogs, panels, buttons, toggles, checkboxes, radios,
sliders, text boxes, images, labels, headings, groups, lists, trees, tables,
progress bars, status regions, toolbars, menus, tabs, viewports, canvases,
graphs, inspectors, and `Unknown`.

Semantic properties include:

- name, description, hint, and value.
- enabled, hidden, focused, selected, checked, expanded, read-only, and busy.
- live region policy (`Off`, `Polite`, `Assertive`).
- range metadata for sliders/progress.
- structural metadata such as level, index, count, parent, and children.

Announcements are engine events for semantic consumers, not speech synthesis.
The first announcement kinds are focus changed, value changed, live region,
action, state changed, and custom. Future platform adapters can translate them
to OS APIs; testing and automation can consume them directly.

### Integration

`UiSurface` owns one `UiAccessibilityManager`. `UiSystem` exposes semantic
registration, tree snapshot, update, announcement, and drain APIs. Node, layer,
mount, composition, surface, and document cleanup prune semantic descriptors.
`dispatchInput(...)` updates accessibility after retained event propagation so
focus announcements observe the final frame state.

Widgets now describe semantics:

- labels register `Label` by default and can be promoted to `Status` or another
  role.
- buttons register `Button`, preserve a `labelledBy` relationship to their text
  node, and publish action announcements when activated.
- progress bars register `ProgressBar` with `0..1` range metadata.
- images are decorative by default and only enter the semantic tree when named
  or explicitly non-decorative.
- panels can opt into `Panel` semantics when they represent a meaningful group.

Bindings can name an `accessibilityTarget`. Text and progress bindings update
semantic value/range state, and live-region nodes announce value changes during
the same surface-local binding update.

The interaction prompt demonstrates the migration path: its label is now a
`Status` live region named "Interaction prompt", and its existing
`UiObservable<std::string>` text binding drives both the visible text and
semantic announcement value.

### Validation

Added `ui_accessibility_runtime`, covering:

- semantic tree construction.
- role assignment and name resolution.
- `labelledBy` relationships.
- focus announcements.
- button action announcements.
- live-region binding updates.
- progress range binding updates.
- mount cleanup.
- surface-local semantic isolation and surface cleanup.

### Remaining Accessibility Debt

- Platform accessibility adapters are not implemented.
- There is no speech synthesis, voice control, screen reader bridge, or
  accessibility settings UI.
- Widget coverage is initial; complex widgets such as lists, trees, tabs,
  text fields, sliders, and graph canvases still need semantic descriptors.
- Navigation consumes visibility/enabled state but does not yet use the
  semantic tree as an automation/accessibility query source.
- High contrast, large text, and reduced-motion policy are exposed as
  accessibility policy data but are not yet wired into theme or animation
  resolution.
- Domain announcements such as combat log updates, quest updates, inventory
  errors, and dialogue advancement still need feature migration.

### Remaining Binding Debt

- Combat, dungeon HUD, inventory, merchant, menus, VN dialogue, and engine tool
  panels still contain substantial manual retained UI sync.
- The runtime is intentionally one-way. Text fields, sliders, property editors,
  and IME-aware controls still need explicit future two-way binding policy.
- There is no reflected ECS/property binding bridge yet.
- Binding evaluation is source-revision/value based, not a dependency-graph
  scheduler.

## 22. Phase 16 Implementation: UI Serialization Runtime

### Investigation

Before this phase, the runtime graph was complete but not durable. Every
surface, layer, mount, composition, widget, layout, style, binding,
accessibility descriptor, navigation node, drag source, and transition hint was
authored through C++ builder code. That meant the engine could own UI behavior
at runtime, but tools and mods still could not save, load, diff, inspect,
version, generate, or hot-reload authored UI structure.

The investigation split state into three categories:

- Serializable intent: widget hierarchy, widget ids, layout specs, style/theme
  references, default text/image/progress values, binding paths, semantic
  descriptors, navigation metadata, drag/drop metadata, animation timing hints,
  optional surface descriptors, and layer descriptors.
- Runtime-only state: retained handles, computed layout, visual lists, command
  buffers, focus/hover/capture, pointer ownership, active animations, current
  announcements, modal stack, dispatch results, widget-local pressed/hovered
  transients, and resource pointers.
- Application-owned behavior: callbacks, gameplay data, ECS objects, domain
  object references, binding source resolution, formatter/transform functions,
  localization lookup, and feature-specific meaning.

### Architecture

The first UI serialization format is versioned JSON. JSON was chosen because it
is human-readable, diffable, convenient for tests and mods, and already used by
engine/game assets. The implementation is intentionally UI-owned: it adds a
small `UiJsonValue` parser/writer in the rendering UI module instead of pulling
in a new dependency or reusing domain-private parsers.

The asset model is:

`UiSerializedAsset -> optional Surface -> Layers -> Widget Tree`

Each `UiSerializedWidget` stores authored intent:

- stable `id` used for behavior attachment.
- widget `type` such as `Panel`, `Label`, `Button`, `Image`, `ProgressBar`,
  `Spacer`, `Stack`, `Separator`, or `ScrollView`.
- layout specification and text alignment.
- style class / label style class.
- default text, image, progress, visibility, enabled, and interactable values.
- one-way binding declarations by path.
- semantic role/name/value/range/live-region metadata.
- navigation role, group, tab order, wrap policy, and future neighbor ids.
- drag source and drop target metadata.
- style transition timing hints.
- child widgets.

Bindings serialize relationships, not objects. A binding stores property, path,
optional formatter/transform names, and optional animation timing. During
instantiation an application-provided `IUiSerializedBindingResolver` turns paths
such as `prompt.text` or `health.percent` into live `UiBindingSource`
callbacks. If no resolver can satisfy a path, instantiation reports a validation
error instead of inventing application data.

`UiSerializationRuntime::instantiate(...)` builds the retained graph under an
existing mount. `UiSystem::instantiateUiAsset(...)` is the public facade. The
loader returns `UiSerializedInstance`, which maps serialized ids to retained
handles so C++ can attach behavior without owning structure.

### Versioning And Validation

Assets carry a numeric `schema`. Schema `0` migrates to version `1` with a
warning. Future schema versions are rejected. Validation detects:

- missing root widget.
- unsupported schema.
- unknown widget types.
- duplicate widget ids.
- invalid grid dimensions/spans.
- missing binding paths.
- missing style classes when a validation theme is supplied.
- invalid mount/surface during instantiation.
- unresolved binding paths during instantiation.

The current schema is deliberately small but extensible. It avoids serializing
runtime handles or implementation details so future schema migrations can
rename widget fields, add prefab/template references, add localization keys, or
replace direct fields with asset references.

### Feature Demonstration

`InteractionPromptComposition` now loads its structure from a serialized UI
asset string. The asset defines the prompt panel, label, layout, style classes,
fade binding hints, text binding path, accessibility `Status` role, and polite
live region. The C++ composition still owns behavior: it provides a binding
resolver for `prompt.text` and `prompt.opacity`, attaches the runtime font
pointer to the loaded label handle, and keeps the retained nodes visible while
fade-out animations complete.

This demonstrates the intended boundary:

- asset owns structure and presentation intent.
- C++ owns observable state and behavior.
- runtime owns mount lifetime, bindings, animation, semantics, and rendering.

### Validation

Added `ui_serialization_runtime`, covering:

- JSON parse.
- schema migration.
- validation errors.
- missing style detection.
- widget tree round-trip serialization.
- surface and layer descriptor serialization.
- layout serialization.
- binding serialization.
- accessibility serialization.
- navigation metadata registration.
- drag/drop metadata registration.
- runtime instantiation under a mount.
- binding source resolution.
- semantic updates from bindings.
- save/load file round-trip.

### Remaining Serialization Debt

- The first implementation supports the initial widget set only.
- There is no prefab/template inheritance yet.
- There is no localization key model yet.
- There is no hot-reload file watcher; runtime support exists through
  re-instantiation/rebuild, but watching and diff application are future tool
  work.
- Serialized explicit navigation neighbors are parsed as ids, but final handle
  patch-up is future work.
- The loader builds structure but does not preserve widget objects for
  built-in event callbacks; C++ behavior attachment still happens through
  stable ids, compositions, and widgets.
- There is no visual editor, localization runtime, or live file watcher yet.

## 23. Phase 17 Implementation: Widget Asset Definitions

### Investigation

Serialization made retained UI durable, but every serialized tree was still a
one-off document. Repeated authored structures remained visible across the
engine and game:

- prompt panels are panel/label trees with the same fade, live-region, and
  bottom-aligned layout policy.
- menu buttons repeat a rect plus centered label plus navigation metadata.
- merchant and inventory slots repeat panel/image/label/overlay patterns.
- progress bars repeat track/fill/semantic range metadata.
- editor/tool rows repeat label/value/button structures.

Those repeated structures are not runtime systems and should not become
gameplay classes. They are authored assets: reusable definitions that expand
into serialized widget trees.

### Architecture

`UiWidgetAssetDefinition` is the authored asset layer above
`UiSerializedAsset`.

A widget asset stores:

- stable asset `id`, schema version, asset version, display name,
  description, tags, and dependencies.
- optional `baseAsset` for inheritance.
- parameters with type, default value, required flag, and description.
- named slots with target widget ids and optional default children.
- variants that override parameter defaults, slot defaults, or root widget
  fields.
- one root `UiSerializedWidget` tree.

The registry resolves assets in this order:

1. Load the base asset, if any.
2. Merge the derived asset over the base asset.
3. Apply the requested variant.
4. Collect parameter defaults and caller overrides.
5. Fill named slots.
6. Substitute `{{parameter}}` tokens in authored string fields.
7. Prefix stable widget ids for reusable instances.
8. Expand nested asset references into ordinary serialized widgets.

The result is not a special runtime object. It is a resolved
`UiSerializedWidget` tree. `UiSerializationRuntime` remains the only loader
that creates retained nodes.

### Runtime Integration

`UiSystem` now owns a `UiWidgetAssetRegistry` and exposes:

- `widgetAssets()`
- `instantiateWidgetAsset(...)`

`UiSerializedWidget` can also reference an asset directly through `asset`,
`variant`, `parameters`, and `slots`. During `UiSystem::instantiateUiAsset(...)`,
the serialization runtime asks the registry to expand asset references before
validation and instantiation.

This keeps the boundaries clean:

- Widget assets own reusable authored structure.
- Serialization owns storage and runtime graph loading.
- Widgets own behavior and retained primitives.
- Compositions own feature logic and callback attachment.

### Inheritance, Variants, Slots, And Parameters

Inheritance merges asset metadata, parameters, slots, variants, and root widget
trees. Children with the same stable id are merged; new children are appended.
This supports `Button -> DangerButton` style reuse without copying the full
button tree.

Variants avoid asset duplication for common presentation or sizing choices,
such as `primary`, `compact`, or `danger`. Variants can override parameter
defaults, slot defaults, or specific root fields.

Slots let assets compose other authored trees. A dialogue window can define
`Header`, `Body`, `Choices`, and `Footer`; an inventory slot can define
`Icon`, `Quantity`, `Overlay`, and `Selection`. Slot content can itself be an
asset reference, so reusable authored assets compose recursively.

Parameters are string-substituted with `{{name}}` in asset-authored fields.
They are intentionally generic; the engine does not know whether `label`,
`icon`, or `valuePath` is gameplay, editor, or tool data.

### Feature Demonstration

`InteractionPromptComposition` now registers two widget assets:

- `engine.status_prompt`: reusable panel plus live status label structure.
- `dungeon.interaction_prompt`: derived dungeon-specific presentation defaults.

The composition instantiates `dungeon.interaction_prompt` through
`UiSystem::instantiateWidgetAsset(...)`. C++ still owns the prompt observable
state, binding resolver, runtime font pointer, and visibility behavior while
the reusable authored asset owns panel/label structure, layout, style classes,
binding paths, animation hints, and accessibility metadata.

### Validation

Added `ui_widget_asset_runtime`, covering:

- asset parsing and round-trip serialization.
- registry lookup.
- base asset inheritance.
- variants.
- parameter substitution.
- slots and nested asset references.
- serialized UI documents that instantiate widget assets.
- runtime handle mappings, style classes, text payloads, and navigation
  metadata.
- widget asset file save/load.

### Remaining Widget Asset Debt

- Asset files are supported, but there is no directory/package loader yet.
- No live file watching or hot reload is implemented.
- No asset inheritance diff UI or visual editing metadata is implemented.
- No localization key model is attached to parameters yet.
- Serialized navigation neighbor ids are still not patched to final handles.
- Behavior attachment remains C++ by stable id; callback serialization remains
  intentionally out of scope.

### Next Capability

With Widget Asset Definitions complete, the single highest-value capability to
build next is **Visual UI Editor**.

The runtime now has durable serialized UI and reusable authored widget assets.
A visual editor is the first capability that exercises the full platform:
surfaces, mounts, widgets, layout, themes, bindings, accessibility semantics,
serialization, and widget asset definitions. Localization, automation, hot
reload, and virtualized lists are valuable, but they either consume authored
assets or specialize runtime behavior. A visual editor should come next because
it turns the completed architecture into an authoring workflow and will expose
schema, validation, reusable asset, and inspection gaps before UI content
scales further.

## 24. Final Recommendation

Adopt the surface/layer/composition runtime:

`UiRuntime -> UiSurface -> UiLayer -> UiMount/UiTree -> UiNode`

Keep the current retained node primitives as the low-level drawing and layout
substrate, but stop treating `UiSystem` plus one document as the whole UI
runtime. The engine should own explicit surfaces, layers, mountable
compositions, centralized input dispatch, focus, modal policy, layout, styling,
widgets, navigation, drag/drop, animation, data binding, accessibility
semantics, serialization, and future presentation systems.
`UiWidgetAssetRegistry` should own reusable authored widget definitions above
serialization while leaving runtime behavior in widgets and compositions.

This architecture is the smallest durable set of primitives that naturally
supports visual novels, RPG HUDs, strategy panels, action-game overlays, editor
tools, debug interfaces, split-screen, multiple windows, world-space terminals,
render-target UI, modding, and future UI systems without adding gameplay
vocabulary to the engine.

The first committed direction was explicit layers. The second committed
direction is centralized retained UI input dispatch. The third committed
direction is engine-owned focus management. The fourth committed direction is
engine-owned modal policy. The fifth committed direction is engine-owned mount
lifetime. The sixth committed direction is engine-owned composition authoring.
The seventh committed direction is retained UI event propagation. The eighth
committed direction is surface-local UI ownership through `UiSurface`. The ninth
committed direction is engine-owned retained layout. The tenth committed
direction is surface-local styling and theme resolution. The eleventh committed
direction is reusable composition-owned widgets. The twelfth committed
direction is surface-local non-pointer traversal and activation through
`UiNavigationGraph`. The thirteenth committed direction is surface-local direct
manipulation through `UiDragDropManager`. The fourteenth committed direction is
surface-local temporal presentation through `UiAnimationManager`. The fifteenth
committed direction is surface-local one-way synchronization through
`UiBindingManager`. The sixteenth committed direction is surface-local semantic
UI meaning and announcements through `UiAccessibilityManager`. The seventeenth
committed direction is durable retained UI structure through the UI
Serialization Runtime. The eighteenth committed direction is reusable authored
UI definitions through `UiWidgetAssetRegistry`.

The next milestone should implement a Visual UI Editor. It should precede
localization runtime, automation runtime, hot reload, and virtualized lists
because the runtime now has the persistent and reusable authored model that an
editor can create, validate, inspect, and save.
