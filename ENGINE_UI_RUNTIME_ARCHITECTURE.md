# Engine UI Runtime Architecture Investigation

## Status

This document records the UI runtime investigation performed against the current
engine and game code. It also records the recommended long-term architecture and
the staged migration plan.

The investigation found that the current UI runtime is useful as a retained
primitive renderer, but it is not a complete UI runtime architecture for a
general-purpose engine. The engine needs first-class concepts for surfaces,
layers, composition, centralized input dispatch, focus, modal ownership, richer
layout, styling, and animation.

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

## 1. Current Architecture

### Runtime Summary

The retained UI runtime is currently centered on one `UiSystem` instance owned by
the rendering runtime. That `UiSystem` owns one `UiDocument`, per-node text font
bindings, a cached `UiCommandBuffer`, a `UiInputDispatcher`, a
`UiFocusManager`, a `UiModalManager`, and a `UiMountManager`.

The runtime currently has one screen-space document. The document coordinate
space is normalized from `0..1` in both axes. Rendering later resolves those
normalized coordinates into the current output pixel dimensions.

The render path is:

1. Game or engine systems mutate retained nodes through `UiSystem`.
2. `RenderingRuntime` asks `UiSystem` to extract commands for the output size.
3. `UiSystem` updates layout if dirty, rebuilds the visual list if dirty, and
   resolves the visual list into a `UiCommandBuffer`.
4. The Vulkan UI render stage draws that command buffer as an overlay after the
   scene and particle passes.

The retained input path is now centralized once per engine frame.
`RenderingRuntime::dispatchUiInput(...)` builds a `UiInputFrame` from
`InputBindingRegistry`, calls `UiSystem::dispatchInput(...)`, and stores a
`UiDispatchResult` for feature systems to read. The dispatcher consults
`UiModalManager` before hit testing so the top modal can block lower layers and
route cancel/back input. Inventory and merchant grid interactions still compute
cell ownership manually because they are not yet modeled as retained UI widgets.

### UiSystem

`UiSystem` is the render-facing facade over `UiDocument`. It exposes node
creation, mutation, removal, layout/style/state/payload setters, text font
bindings, measurement, interaction, and command extraction.

Current responsibilities:

- Owns the retained document.
- Owns text font bindings by `UiHandle`.
- Owns the command buffer cache.
- Owns the input dispatcher and focus manager.
- Owns the modal manager.
- Owns the mount manager.
- Exposes the latest dispatch result.
- Converts the retained visual list into renderer command data.

`UiSystem::dispatchInput(...)` is the retained input entry point. It:

- Forces document layout into normalized `1.0 x 1.0` space.
- Delegates to `UiInputDispatcher`.
- Lets the dispatcher run `UiDocument::hitTest(...)`.
- Lets `UiFocusManager` own hover, capture, active pointer, and focus state.
- Lets `UiModalManager` constrain hit testing and cancel routing.
- Records the owning layer for hovered, focused, pressed, released, clicked,
  active, and captured handles.
- Records modal ownership, blocking, and cancel/dismissal state.
- Publishes the frame's `UiDispatchResult`.

`UiSystem::updateInteraction(...)` remains as a compatibility helper over the
dispatcher.

Missing from `UiSystem` today:

- Event queue.
- Event propagation.
- Per-surface dispatch.
- Per-layer dispatch policy beyond the new layer root state.
- Keyboard event routing.
- Text input event routing.
- Controller navigation graph.
- Multiple surfaces.
- Full multi-pointer dispatch. Focus state is already keyed by pointer id, but
  `UiInputFrame` still carries only the primary pointer.
- Drag/drop as a runtime primitive.
- Modal enter/exit transitions.

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
- There is no composition object or mount-local event lifecycle yet.

### Coordinate Systems

The retained UI document operates in normalized screen coordinates. A node's
computed layout bounds are usually in `0..1` document space. The command resolver
uses the output viewport width and height when converting primitives into
vertices.

Existing game UI code frequently computes pixel layouts manually, then converts
them to normalized offsets and fixed sizes before writing `UiLayoutSpec`.

Current coordinate limitations:

- No typed unit system.
- No explicit pixel units in layout specs.
- No viewport units separate from normalized document units.
- No parent-relative percent unit beyond anchors.
- No world-space or render-target-local coordinate domains.
- No surface transforms for terminals, monitors, split-screen, VR, or AR.

### Layout

`UiLayoutSpec` supports two position modes:

- `Absolute`
- `Anchored`

It supports two size modes:

- `Fixed`
- `FromAnchors`

Anchored layout computes from the parent content rect, using anchor min/max,
offset min/max, margins, and padding. Absolute layout positions relative to the
parent content rect and uses fixed width/height after margins. `contentOffset`
shifts the child layout content rect, which is currently used for scroll-like or
pan-like behavior without moving the container itself.

Current layout limitations:

- No measurement pass for child-driven size.
- No auto/content sizing.
- No stack layout.
- No dock layout.
- No grid layout manager.
- No flex layout.
- No min/max constraints.
- No aspect-ratio constraint.
- No layout invalidation boundaries.
- No intrinsic text measurement integration into layout.
- No reusable layout containers.

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

There are no explicit events such as pointer enter/leave/down/up/click/move,
wheel, key down/up, text input, submit, cancel, navigation move, focus gained, or
focus lost. The event vocabulary exists in `UiEvent.h`, but propagation is not
implemented yet.

Retained interaction ownership is no longer initiated by feature systems.
Menus, merchant buttons, skill-tree nodes, VN choices, and engine tool widgets
read `ctx.ui->dispatchResult()`. Manual inventory-grid, merchant-grid, and
tool-world pointer ownership still bypass retained dispatch.

### Keyboard, Focus, and Navigation

Keyboard input is currently handled outside the UI runtime through
`InputBindingRegistry` and feature-specific code. `UiFocusManager` now owns
keyboard focus, text-input focus, navigation focus, pointer hover, pointer
capture, active pointer, focus scopes, and focus restoration. Primary pointer
press requests keyboard focus through the focus manager, but key events, text
input events, and navigation events are not routed through retained UI yet.

Missing:

- Keyboard event routing to the focused owner.
- Text input routing to the text-input owner.
- Tab order.
- Controller navigation graph.
- Navigation submit/cancel routing.
- Event-level modal focus trap.
- Focus lost/gained events.

### Styling

`UiStyle` exists, but most visible styling is payload-local. Rect colors, text
colors/scales, image tint, panel skins, and button state colors are usually
written directly by feature builders or sync systems.

There is no theme system, no style class system, no skin registry, no typography
scale, no metric token system, and no inherited visual style model. Higher-level
systems such as VN presentation profiles and engine tool widgets provide some
structure above the raw UI runtime, but that structure is feature-local.

### Animation

There is no generic UI animation runtime. The engine has a tween module and the
VN stage uses tweens for sprite/background presentation, but retained UI nodes do
not have built-in transitions, state animations, timelines, or modal enter/exit
animations.

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

- Only one screen-space document exists.
- No first-class UI surface abstraction exists.
- Layering was historically implicit; the new layer primitive starts to fix only
  the root-level ordering problem.
- Most existing feature UI still uses the default layer.
- Some UI ownership still bypasses retained dispatch, especially inventory
  grids, merchant grids, tool viewport picking, and drag/drop behavior.
- Hit testing and render order are coupled to the same traversal, and input
  routing still lacks event propagation.
- Pointer capture exists, but routed pointer events and drag/drop are not yet
  built on it.
- Keyboard focus exists, but keyboard events are not yet routed to it.
- Controller navigation does not exist in the UI runtime.
- Event propagation does not exist.
- Handler-level event consumption does not exist.
- Modal ownership exists in the engine, but existing game UI has not yet
  migrated its open/closed state and gameplay gates onto modal descriptors.
- Gameplay input blocking is still game-specific and manually checks feature
  singleton state.
- Inventory, merchant grids, and tools implement their own input routing.
- Feature UIs are still built mostly as raw handle structs, not as reusable
  mounted compositions.
- Mount ownership exists, but existing game/tool UI has not yet migrated to it.
- Raw handles are not generation checked.
- Layout is too low-level for long-lived engine use.
- Styling is not structured around reusable themes or skins.
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

### UiMount and Composition

UI should become compositional. A dialogue interface, inventory, shop, settings
panel, debug window, codex, terminal, skill tree, or tooltip should be a
composition that can be mounted into a host.

The first `UiMount` primitive is now implemented as ownership and attachment
only. It creates a retained container root, tracks parent/child mounts, supports
attachment to layers, nodes, or other mounts, and owns teardown cleanup.
`UiComposition` is still future work and should build on this mount primitive.

Composition responsibilities:

- Build a reusable UI tree.
- Receive a typed mount context.
- Expose typed state/update/event hooks.
- Own child mounts if needed.
- Unmount cleanly through a mount token.

Mount responsibilities:

- Own the retained subtree root for a composition or compatibility builder.
- Destroy the tree when unmounted.
- Destroy child mounts recursively.
- Clear focus, capture, active pointer, text focus, navigation focus, and modal
  ownership for the subtree.
- Preserve or restore focus when requested.
- Attach below a surface/layer/mount point.
- Optionally provide local services such as theme, localization, or data model.

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
- Build an event target path.
- Deliver capture phase.
- Deliver target phase.
- Deliver bubble phase.
- Stop routing when consumed by policy or handler.
- Ask `UiFocusManager` to apply hover, active pointer, capture, and focus
  transitions.
- Publish a frame result for gameplay/tool gating.

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
- target path
- modifiers
- consumed flag
- default-prevented flag

Nodes and compositions should be able to register event handlers. The runtime
should support capture/target/bubble propagation because parent widgets such as
scroll views, lists, windows, and modals need policy control around child nodes.

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

- modal focus traps
- focus lost/gained events

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

- Add composition hosts on top of mounts.
- Make feature UIs reusable units that build into a mount context.
- Start with engine tools or menus because they are mostly self-contained.

Files/classes involved:

- New `UiComposition`
- `UiMount`
- tool UI widgets
- menu UI builders

Expected API changes:

- `mount(layer, composition)`
- `unmount(token)`
- mount-local context

Compatibility:

- Existing handle structs can be owned inside composition instances initially.

Validation:

- Test composition build/update/unmount lifecycle.
- Tool/menu smoke test.

### Phase 6: Surfaces

Objectives:

- Promote current document to a default screen surface.
- Add surface descriptors and extraction per surface.
- Support viewport-local surfaces for split-screen/editor viewports.
- Define world-space surface contract even if rendering remains basic initially.

Files/classes involved:

- New `UiSurface.h`
- New `UiRuntime.h/.cpp`
- `RenderingRuntime`
- `UiRenderStage`
- frame graph integration

Expected API changes:

- `createSurface`
- `destroySurface`
- `extract(surface)`

Compatibility:

- Current `UiSystem` remains a default-surface facade during migration.

Validation:

- Test independent hit testing on two surfaces.
- Test viewport-local coordinates.
- Render smoke for default screen surface.

### Phase 7: Layout Engine Expansion

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

### Phase 8: Styling and Themes

Objectives:

- Add theme tokens, style classes, skins, typography, and metrics.
- Let feature UIs consume theme values rather than hardcoding every color/scale.
- Unify engine tool theme and game theme mechanisms without sharing content.

Files/classes involved:

- `UiStyle.h`
- new `UiTheme.h`
- tool widgets
- VN presentation profile bridge
- game UI builders

Expected API changes:

- theme registry
- style classes
- state style resolution

Compatibility:

- Payload-local colors still work.

Validation:

- Theme swap test.
- Button state style tests.

### Phase 9: Animation and Transitions

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

### Phase 10: Feature Migration

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
- VN choice clicks consume retained dispatch results, but VN playback/choice
  selection remains VN runtime state.

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
- There is no navigation graph or navigation event delivery yet.
- There are no text widgets or IME/text event routing yet.
- Focus gained/lost events are defined in `UiEvent.h` but not emitted.
- Event propagation, capture/target/bubble delivery, and handler registration
  are still future work.
- Feature-owned inventory grid drag/drop, skill-tree pan, key-rebind capture,
  tool viewport capture, world picking, and gizmo drag remain outside generic UI
  focus.

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

- VN dialogue UI still returns `VNDialogueUiHandles`.
- Dungeon HUD and prompt UI still return handle structs.
- Inventory, merchant, menu, and skill-tree coordinators still pass handle
  structs to handlers and sync systems.
- Combat UI and billboard UI still use raw handles from scene load.
- Engine tool panels still store and destroy root handles manually.
- Shared widgets still return interior handle bundles.

These are compatibility paths. The migration should wrap top-level feature UI
roots in mounts first, then later move builders into `UiComposition`.

### Next Primitive

The single highest-value missing primitive is now `UiComposition`.

Mounts answer who owns a subtree and how it is attached. They do not answer how
a reusable UI unit is authored, updated, parameterized, or unmounted as a
coherent feature. `UiComposition` should come next because it turns the current
builder/handle-struct pattern into a reusable runtime object that builds into a
mount context.

Surfaces, event propagation, layout, styling, animation, navigation, and
drag/drop all become cleaner after composition exists because they can target
composition boundaries instead of feature-specific handle bundles.

## 11. Final Recommendation

Adopt the surface/layer/composition runtime:

`UiRuntime -> UiSurface -> UiLayer -> UiMount/UiTree -> UiNode`

Keep the current retained node primitives as the low-level drawing and layout
substrate, but stop treating `UiSystem` plus one document as the whole UI
runtime. The engine should own explicit surfaces, layers, mountable
compositions, centralized input dispatch, focus, modal policy, layout, styling,
and animation.

This architecture is the smallest durable set of primitives that naturally
supports visual novels, RPG HUDs, strategy panels, action-game overlays, editor
tools, debug interfaces, split-screen, multiple windows, world-space terminals,
render-target UI, modding, and future UI systems without adding gameplay
vocabulary to the engine.

The first committed direction was explicit layers. The second committed
direction is centralized retained UI input dispatch. The third committed
direction is engine-owned focus management. The fourth committed direction is
engine-owned modal policy. The fifth committed direction is engine-owned mount
lifetime.

The next milestone should implement `UiComposition` on top of `UiMount`.
Composition should precede surfaces, event propagation, layout expansion,
styling, animation, navigation, and drag/drop because it creates the reusable
UI unit those systems need to target.
