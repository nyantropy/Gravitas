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

## 1. Current Architecture

### Runtime Summary

The retained UI runtime is currently centered on one `UiSystem` instance owned by
the rendering runtime. That `UiSystem` owns one `UiDocument`, per-node text font
bindings, a cached `UiCommandBuffer`, and a small interaction state consisting of
the last hovered, focused, pressed, clicked, and active handles.

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

The input path is not a single central path. Several feature systems construct
their own `UiInputFrame` from `InputBindingRegistry` and call
`UiSystem::updateInteraction(...)` directly. Each call recomputes global hover,
pressed, and focused state for the same document. Inventory grid interaction
does not use the engine hit test at all; it computes cell ownership manually.

### UiSystem

`UiSystem` is the render-facing facade over `UiDocument`. It exposes node
creation, mutation, removal, layout/style/state/payload setters, text font
bindings, measurement, interaction, and command extraction.

Current responsibilities:

- Owns the retained document.
- Owns text font bindings by `UiHandle`.
- Owns the command buffer cache.
- Owns the last interaction result.
- Owns the current active pointer handle.
- Owns the current focused handle, though focus is only click-assigned.
- Converts the retained visual list into renderer command data.

`UiSystem::updateInteraction(...)` is not an event dispatcher. It is a polling
helper. It:

- Forces document layout into normalized `1.0 x 1.0` space.
- Runs `UiDocument::hitTest(...)`.
- On primary press, stores the hovered handle as `activeHandle`.
- On primary press over a node, stores that node as `focusedHandle`.
- On primary release, reports a click if the release target is the same as the
  active target.
- Mutates hovered, focused, and pressed flags on affected nodes.

Missing from `UiSystem` today:

- Event queue.
- Event propagation.
- Per-surface dispatch.
- Per-layer dispatch policy beyond the new layer root state.
- Keyboard event routing.
- Text input routing.
- Controller navigation.
- Multi-pointer support.
- Drag/drop as a runtime primitive.
- Focus scopes.
- Modal ownership.

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
surface system, not yet a modal system, and not yet a full input router.

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

Lifetime is manual and handle-based. Feature UI builders store handle structs.
Feature sync/event systems later mutate those handles.

`UiSystem::clear()` clears the whole document and all text bindings. Individual
nodes can be removed recursively. Layer roots and the hidden document root cannot
be removed through `removeNode`.

There is no mount token, no scoped owner object, no automatic unmount on owner
destruction, and no generation-checked handle type.

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
- No pointer capture except `UiSystem::activeHandle`.
- No hover ownership per pointer id.
- No layer blocking policy beyond disabled layer roots.
- No surface priority.
- No modal stack.
- No separation between "visible", "hit test visible", and "receives input".

### Interaction

Interaction is currently state mutation plus a small result struct:

- `hovered`
- `focused`
- `pressed`
- `clicked`
- pointer coordinates
- scroll deltas

There are no explicit events such as pointer enter/leave/down/up/click/move,
wheel, key down/up, text input, submit, cancel, navigation move, focus gained, or
focus lost.

Interaction ownership is distributed. The current direct callers of
`updateInteraction` include menu UI, merchant UI, skill-tree UI, visual novel UI,
and engine tool UI. Each caller can overwrite global hover/focus/press state for
the shared document during the same frame.

### Keyboard, Focus, and Navigation

Keyboard input is currently handled outside the UI runtime through
`InputBindingRegistry` and feature-specific code. UI focus is not a keyboard
focus system. It is a handle set when primary pointer press lands on an
interactable node.

Missing:

- Keyboard focus owner.
- Text input owner.
- Focus scopes.
- Focus restoration.
- Tab order.
- Controller navigation graph.
- Navigation submit/cancel routing.
- Modal focus trap.
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

VN UI currently competes through the same global document and the same
`updateInteraction` call as other feature UIs. Because it is built lazily, it has
historically relied on creation order for its overlay position.

### Merchant UI

Merchant UI is game-owned and feature-local under `src/inventory/ui/`. It builds
a retained full-screen scrim/window and many retained handles for cells, item
icons, labels, buttons, and text.

Merchant UI calls `UiSystem::updateInteraction(...)` for buttons and retained
interactive nodes, but also computes grid cell hover/selection behavior manually
from its own pixel layout. Merchant open/closed state is stored in a game
singleton component. Merchant input blocking is handled by `DungeonInputGate`,
not by a generic modal system.

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
and `UiSystem::updateInteraction(...)`. Menu open state is also part of
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
controller systems. They build retained UI using tool widgets and call
`UiSystem::updateInteraction(...)`. Tool input capture is represented by
tool-specific singleton components, not by a generic UI capture primitive.

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
- Input dispatch is distributed across multiple feature systems.
- Multiple systems can call `updateInteraction` in one frame and overwrite the
  same global hover/focus/pressed flags.
- Hit testing and render order are coupled to the same traversal, but input
  ownership is not centralized.
- Pointer capture is only an internal active handle.
- Keyboard focus is not a real focus system.
- Controller navigation does not exist in the UI runtime.
- Event propagation does not exist.
- Event consumption does not exist.
- Modal ownership does not exist.
- Gameplay input blocking is game-specific and manually checks feature
  singleton state.
- Inventory, merchant grids, and tools implement their own input routing.
- Feature UIs are built as roots, not as reusable mountable compositions.
- There is no mount token or scoped lifetime owner.
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

Composition responsibilities:

- Build a reusable UI tree.
- Receive a typed mount context.
- Expose typed state/update/event hooks.
- Own child mounts if needed.
- Unmount cleanly through a mount token.

Mount responsibilities:

- Own the root handle(s) for a composition.
- Destroy the tree when unmounted.
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
- Write hover, capture, and focus transitions.
- Publish a frame result for gameplay/tool gating.

This should replace feature-owned calls to `UiSystem::updateInteraction`.

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

Focus must be a runtime subsystem, not a node flag set by pointer press.

Required focus concepts:

- pointer hover per pointer id
- pointer capture per pointer id
- keyboard focus
- text input focus
- controller/navigation focus
- focus scopes
- nested focus scopes
- focus restoration stack
- modal focus traps
- focus lost/gained events

Focus scopes should let a mounted composition own local navigation without
knowing where it is mounted. A settings panel should be able to trap tab focus
inside itself when modal, or participate in a parent focus scope when embedded.

### Modal Manager

Modal ownership should be first-class and generic.

Modal responsibilities:

- Maintain a modal stack per surface/window.
- Mount modal content into a modal layer or host.
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
    void requestFocus(UiNodeId node, UiFocusReason reason);
    void clearFocus(UiFocusScopeId scope);
    UiNodeId keyboardFocus() const;
    UiNodeId navigationFocus(UiInputDeviceId device) const;
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

### Phase 4: Surfaces

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

### Phase 5: Composition and Mount API

Objectives:

- Add mount tokens and composition hosts.
- Make feature UIs mountable under layers.
- Start with engine tools or menus because they are mostly self-contained.

Files/classes involved:

- New `UiComposition`
- New `UiMount`
- `UiDocument` lifetime helpers
- tool UI widgets
- menu UI builders

Expected API changes:

- `mount(layer, composition)`
- `unmount(token)`
- mount-local context

Compatibility:

- Existing handle structs can be owned inside composition instances initially.

Validation:

- Test unmount recursively removes nodes and clears focus/capture.
- Tool/menu smoke test.

### Phase 6: Layout Engine Expansion

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

### Phase 7: Styling and Themes

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

### Phase 8: Animation and Transitions

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

### Phase 9: Feature Migration

Objectives:

- Move merchant, inventory, menus, VN, HUD, and tools onto surfaces/layers/mounts.
- Remove feature-owned calls to `updateInteraction`.
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

Next.

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
- direct callers of `UiSystem::updateInteraction`

Validation:

- Add `UiInputDispatcherTest`.
- Verify direct callers are reduced.
- Run engine build and targeted UI tests.

### Implementation 3: Focus State Extraction

Objectives:

- Move hover, active pointer, and focus out of `UiSystem` into a dedicated focus
  manager.
- Keep node state flags as presentation reflection, not ownership.

Specific first files:

- `UiFocusManager.h/.cpp`
- `UiInputDispatcher`
- `UiSystem`

Validation:

- Pointer capture tests.
- Focus restoration tests.

### Implementation 4: Modal Stack

Objectives:

- Add generic modal policy.
- Mount modal content into a modal layer.
- Route cancel/back through the top modal.
- Produce UI input-blocking results for gameplay systems.

Specific first files:

- `UiModalManager.h/.cpp`
- `UiLayer.h`
- `UiInputDispatcher`
- game menu/inventory/merchant integration points

Validation:

- Nested modal tests.
- Dungeon scene manual smoke.

## 7. Final Recommendation

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

The first committed direction should be the explicit layer primitive already
implemented here. The next implementation should be centralized input dispatch,
because input ownership is currently the deepest architectural fault exposed by
the VN/dialogue/merchant/inventory layering issue.
