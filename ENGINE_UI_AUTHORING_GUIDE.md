# Engine UI Authoring Guide

This is the practical handbook for building retained UI in Gravitas.

The runtime supports many historical and low-level paths. New feature UI should
not choose freely between them. Use the preferred authoring model below unless
you are maintaining compatibility code, writing a primitive visualization, or
building engine tooling that explicitly needs a lower layer.

## The Standard Model

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
`UiTheme` style classes, metrics, typography, skins, and tokens for all
presentation decisions. Receive user input through retained `UiEvent` values.
Use bindings for one-way data synchronization when the value has an observable
source. Treat retained handles and primitive nodes as implementation details.

## Investigation Summary

The current codebase has multiple authoring styles because the runtime matured
incrementally. These styles are not equally preferred.

| Authoring style | Current examples | Classification | Future rule |
| --- | --- | --- | --- |
| `UiComposition` mounted through `UiSystem` | Combat, dungeon HUD, inventory, merchant, menus, VN | Preferred | Every feature screen, panel, overlay, and child view starts here. |
| Reusable C++ widgets | Labels, panels, buttons, stacks, item grid, progress bars | Preferred | Widgets own reusable controls and interaction behavior. |
| Serialized widget assets | Interaction prompt asset | Preferred for reusable static structure | Use for reusable authored structure, tool-authored UI, and localized UI assets. |
| `UiTheme` style classes, skins, metrics, typography | Dungeon theme, VN profile theme bridge | Preferred | All colors, spacing, text scale, padding, and state visuals come from the theme. |
| Stack, Grid, Dock, Scroll, Constraint, Aspect layout | Runtime widgets and newer VN choice layout | Preferred | Use these to express structure and flow. |
| Overlay layout | VN root/stage, prompt host, modal-like layering | Preferred only for stacking peers | Use for layered regions, not for manual placement of every child. |
| Canvas with normalized `rect(...)` helpers | Menus, inventory, merchant, combat, skill tree | Compatibility | Leave working code alone; do not expand this style for new ordinary UI. |
| Anchored `anchorMin/anchorMax` rectangles | Most migrated game compositions | Compatibility | Use only for full-fill, edge pinning, or bridging old layouts. |
| Absolute/fixed normalized positions | HUD/minimap/debug overlays, VN sprite presentation | Low-level exception | Allowed for primitive visualization and stage/world-space projection only. |
| Raw `createNode`, `setPayload`, handle bundles | HUD minimap, specialized grids, tests, engine primitives | Low-level / legacy | Prefer widgets; use raw nodes only for primitive visualizations or new widgets. |
| Direct payload colors/text scales | HUD, compatibility builders | Legacy | Use theme style classes and widget setters/bindings instead. |
| Reading `dispatchResult()` and comparing handles | Compatibility tools/older UI paths | Legacy compatibility | New UI handles events in `UiComposition::onEvent(...)` and widgets. |
| Manual spacing, row positions, per-frame layout mutation | Menus/dropdowns, HUD minimap, historical VN | Legacy / exception | Use stack/grid/dock/scroll, theme metrics, and stable layout slots. |
| VN `VNLayoutProfile` coordinate rectangles | Yune profile and default VN profile | Compatibility | Do not add more coordinate rects; prefer semantic layout metrics and slots. |

## Ownership Rules

### UiSurface

Owns a complete UI universe: document, layers, focus, modals, navigation,
drag/drop, animation, binding, accessibility, and input/render participation.

Use a new surface only for a separate output domain: editor preview, split
screen, tool surface, render target, or a future world-space surface.

Do not create a surface just to organize a feature panel.

### UiLayer

Owns coarse ordering and input blocking at the surface level.

Use layers for broad strata:

- scene HUD
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
- forward retained events to its widgets in `onEvent(...)`.
- emit feature commands rather than mutate gameplay rules directly.
- own child widget instances and implementation handles.
- attach child compositions through mounts when a child is a feature-owned view.

A composition should not:

- calculate button positions or row offsets.
- hardcode colors, font sizes, padding, or spacing.
- compare raw handles for ordinary button behavior.
- bypass widgets for controls that already exist.
- own gameplay rules such as pricing, combat damage, inventory legality, or
  quest conditions.
- build competing fullscreen roots when a shell or slot exists.

### Widget

Owns a reusable retained subtree and control behavior.

Widgets:

- create retained nodes as an implementation detail.
- register navigation, drag sources, drop targets, semantics, and state visuals.
- consume events that belong to the control.
- expose semantic callbacks or command-oriented results.

Widgets do not own mounts, surfaces, feature state, gameplay rules, or renderer
resources.

### Retained Node

Retained nodes are low-level primitives. New feature code should rarely create
them directly. Use them inside widgets, widget assets, renderer/tool adapters,
primitive visualizations, and compatibility code.

## Layout Rules

The preferred layout model is container-first. A parent declares how its
children flow; children declare sizing, alignment, and growth. Do not manually
calculate coordinates for ordinary UI.

### Stack

Purpose: one-dimensional flow.

Use for:

- vertical menus
- choice lists
- form rows
- setting rows
- inventory details
- property inspector fields
- notification lists

Do not use manual y offsets for rows that can be a stack.

Example:

```cpp
gts::ui::UiStackDesc actions;
actions.layout.constraints.preferredWidth = {UiLayoutUnit::Percent, 1.0f};
actions.layout.constraints.preferredHeight = {UiLayoutUnit::Content, 0.0f};
actions.axis = UiLayoutAxis::Vertical;
actions.crossAxisAlignment = UiLayoutAlignment::Stretch;
actions.gapMetric = "menu.action.gap";
actionStack.build(widgetContext, panel.content(), actions);
```

### Grid

Purpose: regular two-dimensional placement.

Use for:

- inventory cells
- icon palettes
- editor asset thumbnails
- settings option matrices
- skill-tree prototypes when graph placement is not needed

Use grid layout for equal cells. Use a custom widget if cells map to domain
objects and need drag/drop, navigation, or selection.

Do not calculate each cell's normalized rectangle in the composition.

### Dock

Purpose: classic window chrome and panel regions.

Use for:

- window header/body/footer
- editor sidebars
- settings pages with title, content, action bar
- HUD bands with left/right/fill regions

Do not build window internals with a list of absolute rectangles.

### Scroll

Purpose: clipped scrollable content.

Use for:

- quest logs
- long settings pages
- asset browser lists
- combat logs
- inventory descriptions

Scroll owns clipping and content offset. The child content should still use
stack/grid/dock.

### Constraint

Purpose: size and align a child inside a parent slot.

Use for:

- centered modal boxes
- right-aligned action groups
- bounded tooltips
- fixed-ratio panels that should not fill their parent

Prefer constraints over normalized root rectangles when the intent is
"center this panel at this size" or "align this panel to the bottom end."

### Aspect

Purpose: preserve aspect ratio inside an available slot.

Use for:

- portraits
- thumbnails
- minimap panels when they can be treated as a component
- preview images

Do not hand-scale image width/height if the aspect layout can express it.

### Overlay

Purpose: layer peers over the same content rect.

Use for:

- background image + dimming + foreground content
- panel + floating badge
- modal scrim + modal box
- tooltip over a control
- VN stage layers

Overlay is not a general replacement for layout. If a parent has many ordinary
children, use stack/grid/dock inside the overlay region.

### Canvas

Purpose: compatibility placement and low-level primitive staging.

Use for:

- preserving existing normalized-rect UI until migrated.
- primitive visualization such as minimap tiles, graphs, debug overlays.
- VN sprite/stage presentation where coordinates are content coordinates, not
  control layout.
- tests that need exact geometry.

Do not use Canvas for new menus, forms, windows, lists, or panels.

### Anchors

Purpose: fill, pin, or bridge compatibility rectangles.

Preferred uses:

- full-fill root: anchor min `(0,0)`, anchor max `(1,1)`.
- simple edge pinning.
- old code that still uses normalized `rect(x, y, w, h)`.

Do not use anchors to manually place every child in a new panel.

### Absolute / Fixed

Purpose: low-level exact placement in normalized surface units.

Use only for:

- primitive visualizations that are computed from domain geometry.
- renderer/world-space adapters.
- stage sprites or tool canvas content.
- compatibility code.

Do not use absolute fixed placement for normal controls.

## Canonical Layout Patterns

### Window

Composition:

- panel widget as window root.
- dock layout inside panel content.
- header label docked top.
- content area as stack/grid/scroll.
- footer action stack docked bottom or end.

Theme:

- `Feature.Window`
- `Feature.WindowTitle`
- `Feature.WindowBody`
- `Feature.ActionButton`
- spacing metrics such as `feature.window.padding`, `feature.action.gap`.

Navigation:

- buttons use one navigation group.
- rows use tab indexes only when natural order is ambiguous.

### Menu

Composition:

- modal or menu composition mounted into menu layer.
- optional scrim button/panel.
- centered window.
- vertical stack of buttons.
- secondary pages use the same shell with a scrollable body.

Layout:

- no manual row y offsets.
- action lists are stacks.
- setting rows are dock or grid rows inside a scroll view.

### Settings

Composition:

- window shell.
- tab/section selector stack.
- scroll view for current section.
- each row is a setting widget: label + control + optional status.

Theme:

- row gap and section gap are metrics.
- labels and values use typography tokens.

Navigation:

- rows are focusable controls when editable.
- cancel closes dropdowns first, then returns to parent page.

### Dialogue

Composition:

- frontend shell owns stage, dialogue panel, interaction slot, and modal slot.
- dialogue panel content is dock/stack based: name row, text body, footer.
- choices are a vertical stack of buttons.

Layout:

- semantic descriptors: panel alignment, panel width, panel insets, name height,
  text padding, choice width, choice alignment, choice gap, continue alignment.
- compatibility `VNLayoutProfile` rectangles may seed these values but should
  not be extended.

### Merchant

Composition:

- mounted into the Interaction Frontend interaction slot.
- window or child view, not a separate fullscreen overlay.
- item grid widget for inventory cells/items.
- action stack for buy/sell/pay/cancel.
- status label bound or updated from merchant UI state.

Ownership:

- merchant gameplay owns inventory legality, prices, theft, ownership, and
  transaction commands.
- UI owns selection, focus, visible feedback, drag/drop event translation, and
  command emission.

### Inventory

Composition:

- standalone inventory is a feature composition.
- embedded inventory uses the same item grid widget inside a parent shell.
- detail panel is dock/stack/scroll content.

Interaction:

- drag/drop registration belongs to the item grid widget.
- move legality belongs to inventory gameplay logic.
- status messages return through feature UI state.

### Quest Log

Composition:

- window shell.
- quest list stack or virtualized list widget.
- details scroll view.
- optional objective checklist widget.

Theme:

- status colors are semantic tokens such as success/warning/disabled.
- row spacing and selected-row styling are style classes.

### Tooltip

Composition:

- lightweight composition or widget asset mounted into tooltip/modal layer.
- constraint layout aligned near target bounds.
- panel with text stack.

Rules:

- tooltips are not gameplay state.
- tooltip bounds are computed by the tooltip service or widget, not every
  feature composition.

### Popup / Modal

Composition:

- push a modal owner through `UiModalManager`.
- mount content into the modal slot or modal layer.
- scrim belongs to modal shell.
- content is a window pattern.

Rules:

- true confirmations, blocking item details, save prompts, and destructive
  actions are modal.
- merchant trade, inventory, quest views, and crafting screens are not modal
  just because they are prominent; they are child interaction views.

### HUD

Composition:

- HUD root mounted on HUD layer.
- use dock/constraint for stable regions.
- use widgets for meters, prompts, and status text.
- use raw nodes only for primitive visualization such as minimap cell grids or
  debug render data.

Rules:

- HUD does not own gameplay state.
- HUD observes gameplay state through state systems and events.

### Editor Panel

Composition:

- editor surface or tool layer.
- dock layout for main areas.
- panel widgets for docked panes.
- property inspector rows as stack of property widgets.
- asset browser as grid/list widget over a scroll view.

Rules:

- editor-only helpers may exist in tools modules, but they should still consume
  the same surfaces, mounts, layout, themes, events, navigation, and bindings.

## Theme Rules

All presentation choices come from `UiTheme`.

| Presentation decision | Source |
| --- | --- |
| Color | semantic color token or style class |
| Text color | foreground token or style class |
| Typography scale/weight/line height | named typography |
| Padding | panel skin content padding, style padding, or theme metric |
| Spacing/gaps | theme metrics |
| Border radius/width | style class |
| Shadows | style class |
| Panel image/nine-slice | named skin |
| Button hover/pressed/disabled visuals | style states or panel state skin |
| Progress/selection/error visuals | style states and semantic tokens |

Feature UI may define feature-local theme tokens, but it must define them in a
theme builder or package theme asset. It should not assign literal colors,
font scales, or padding inside composition code.

Allowed exceptions:

- debug overlays.
- primitive domain visualizations, such as minimap tile colors.
- temporary compatibility code during migration.
- tests that intentionally verify raw payload behavior.

Theme naming convention:

```text
Feature.Component.Part
Feature.metric.name
FeatureTypographyName
FeatureSkinName
```

Examples:

```text
Inventory.Window
Inventory.ItemLabel
inventory.grid.gap
Inventory.BodyText
Inventory.WindowSkin
```

## Widget Rules

Use widgets for any reusable control or semantic UI unit.

Create or reuse a widget when:

- the element owns interaction behavior.
- more than one feature needs the same control.
- accessibility semantics are non-trivial.
- it registers navigation, drag/drop, bindings, or state transitions.
- it owns repeated child nodes.

Do not create a widget only to avoid writing three lines of composition code.
Widgets should reduce conceptual duplication, not just hide local structure.

Existing core widgets:

- `UiLabelWidget`
- `UiPanelWidget`
- `UiButtonWidget`
- `UiImageWidget`
- `UiStackWidget`
- `UiSpacerWidget`
- `UiSeparatorWidget`
- `UiScrollViewWidget`
- `UiProgressBarWidget`

Feature widgets should follow the same pattern:

- descriptor struct for authoring inputs.
- `build(...)`, `sync(...)` when needed, `onEvent(...)`, `destroy(...)`.
- root handle hidden behind `root()`.
- no gameplay mutation.
- callbacks/commands for semantic output.

## Composition Rules

Preferred composition lifecycle:

```cpp
void FeatureComposition::build(UiCompositionContext& context)
{
    gts::ui::UiWidgetContext widgets(context);
    context.ui.setLayout(context.surface, context.root, gts::ui::fillLayout());

    // Build stable structure once.
    shell.build(widgets, context.root, shellDesc());
    bodyStack.build(widgets, shell.content(), bodyStackDesc());
    primaryAction.build(widgets, footerRoot(), primaryActionDesc());

    // Bind stable data relationships when sources exist.
    title.bindText(widgets, bindObservable(viewTitle));
}

void FeatureComposition::update(UiCompositionContext& context)
{
    gts::ui::UiWidgetContext widgets(context);

    shell.setVisible(widgets, view.open);
    status.setText(widgets, view.status);
    itemGrid.sync(widgets, gridDescFromView());
}

void FeatureComposition::onEvent(UiCompositionContext& context, UiEvent& event)
{
    gts::ui::UiWidgetContext widgets(context);

    itemGrid.onEvent(widgets, event);
    primaryAction.onEvent(widgets, event);

    if (primaryAction.consumePressed())
        commands.push_back(FeatureCommand{FeatureCommandType::Confirm});
}
```

Rules:

- `build(...)` declares structure.
- `update(...)` synchronizes state.
- `onEvent(...)` translates retained events to feature commands.
- `destroy(...)` destroys widgets or lets mount destruction clean the subtree.
- Layout is stable after `build(...)` unless the view truly changes structure.
- Repeated dynamic data should use widgets, stacks, grids, scroll views, or
  widget assets, not per-frame absolute coordinates.

## Interaction Rules

| Responsibility | Owner |
| --- | --- |
| Pointer hit testing and event creation | `UiInputDispatcher` |
| Pointer capture, hover, keyboard/text/navigation focus | `UiFocusManager` |
| Pointer click behavior for a button | `UiButtonWidget` |
| Navigation traversal | `UiNavigationGraph` |
| Modal stack, blocking, cancel routing | `UiModalManager` |
| Drag lifecycle and compatible target resolution | `UiDragDropManager` |
| Drag source/drop target declaration | Widget descriptors |
| One-way state synchronization | `UiBindingManager` |
| Time-based retained presentation | `UiAnimationManager` |
| Accessibility roles and announcements | Widgets and `UiAccessibilityManager` |
| Gameplay result of a UI command | Feature gameplay system |

New UI should not poll `dispatchResult()` for ordinary controls. Use
`UiComposition::onEvent(...)` and widget event forwarding. `dispatchResult()`
remains available for compatibility, tests, and specialized tools.

Navigation rules:

- focusable widgets register with `UiNavigationGraph`.
- use navigation groups per panel or list.
- use tab indexes only where child order does not express the desired order.
- cancel/back is a retained navigation event; the composition converts it to a
  feature command.

Drag/drop rules:

- widgets declare sources and targets.
- drag payload identifies domain objects but does not execute domain rules.
- feature gameplay validates and applies the drop command.

Modal rules:

- use modals only for blocking decisions or transient blocking surfaces.
- do not fake modals with fullscreen roots and manual input gates.
- child interaction views stay in the Interaction Frontend interaction slot.
- confirmations and item details use the modal slot.

## Interaction Frontend

The Interaction Frontend is the canonical application of the runtime. It is the
shell for NPC interaction presentation, not just dialogue.

The frontend owns:

- VN stage background, dimming, sprites, and dialogue panel.
- choice buttons.
- an interaction slot for non-modal feature child views.
- a modal slot for true blocking overlays.
- the surface, layers, mounts, and published frontend state.

Feature systems integrate by:

1. Starting or continuing an interaction session through
   `InteractionFrontendSessionComponent`.
2. Reading `VNFrontendStateComponent` for the interaction slot or modal slot.
3. Mounting their feature composition into the appropriate slot.
4. Publishing feature-local UI state snapshots.
5. Consuming feature commands from the composition/coordinator.
6. Leaving gameplay rules in the owning feature.

Interaction child views:

- merchant trade
- inventory handoff
- quest handoff
- crafting
- training
- banking
- gifting
- relationship screens
- NPC-specific notifications

Modal slot views:

- buy/sell confirmation
- discard confirmation
- item detail popup when it blocks interaction
- save/load prompt
- settings prompt
- destructive action confirmation

## Inventory and Merchant Guidance

Merchant belongs to the Interaction Frontend because a merchant is an NPC
interaction mode. It shares stage presentation, dialogue feedback, choices,
and the same focus/modal ownership as other NPC views.

Inventory is a composition because it is feature UI over inventory state. It
may be standalone, mounted into a merchant view, or embedded in another
interaction view, but the inventory feature owns the data and rules.

Drag/drop belongs to the runtime because pointer capture, drag threshold,
target resolution, hover transitions, cancel, and drop events are generic UI
behavior. The drag payload should identify inventory objects; it should not
decide whether a move is legal.

Gameplay owns inventory rules:

- item dimensions.
- stack rules.
- ownership and payment state.
- merchant prices.
- theft consequences.
- container restrictions.
- mutation of inventory grids.

UI owns interaction:

- selected item/cell.
- focused item/cell.
- visible drag state.
- status messages.
- command emission.
- accessible names and hints.

## VN Layout Profile Rule

`VNPresentationProfile` and `VNLayoutProfile` are compatibility authoring seeds.
They exist so current game content can keep working.

Do not add new coordinate rectangles to `VNLayoutProfile` for future needs.
The current rectangles already mix several meanings: panel anchoring, relative
overlay placement, child content slots, and choice-region placement. Future VN
layout authoring should become semantic and theme-driven.

Preferred future descriptors:

- dialogue panel alignment.
- panel width and max width.
- panel bottom inset.
- panel content padding.
- nameplate height and alignment.
- speaker/body stack gap.
- body max lines.
- continue indicator alignment.
- choice stack alignment.
- choice width.
- choice row height.
- choice gap.
- interaction slot padding.
- modal slot padding.

Migration guidance:

1. Keep reading existing `VNLayoutProfile` fields.
2. Derive semantic layout metrics from them in the VN theme/profile bridge.
3. Add semantic fields only when a caller needs new behavior.
4. Prefer semantic fields over additional raw rectangles.
5. Leave raw rectangles as compatibility overrides until all shipped content has
   migrated.

## Preferred Examples

### Dialogue Composition

```cpp
// Build once.
stageOverlay.build(widgets, context.root, stageOverlayDesc());
dialoguePanel.build(widgets, context.root, dialoguePanelDesc());
dialogueStack.build(widgets, dialoguePanel.content(), dialogueStackDesc());
speakerLabel.build(widgets, dialogueStack.root(), speakerDesc());
bodyLabel.build(widgets, dialogueStack.root(), bodyDesc());
choiceStack.build(widgets, context.root, choiceStackDesc());

// Sync.
speakerLabel.setText(widgets, runtime.getDialogue().speaker);
bodyLabel.setText(widgets, runtime.getDialogue().visibleText);
syncChoiceButtons(widgets, runtime.getDialogue().choices);
```

No row y offsets. No direct text colors. No handle click comparison.

### Merchant Composition

```cpp
window.build(widgets, context.root, merchantWindowDesc());
contentDock.build(widgets, window.content(), contentDockDesc());
playerGrid.build(widgets, contentRoot, playerGridDesc());
actionStack.build(widgets, footerRoot, actionStackDesc());
sellButton.build(widgets, actionStack.root(), sellButtonDesc());
cancelButton.build(widgets, actionStack.root(), cancelButtonDesc());
```

The grid widget emits selection/drop commands. Merchant gameplay applies the
transaction.

### Inventory Composition

```cpp
window.build(widgets, context.root, inventoryWindowDesc());
mainDock.build(widgets, window.content(), mainDockDesc());
grid.build(widgets, inventoryPane, gridDescFromView());
detailStack.build(widgets, detailPane, detailStackDesc());
closeButton.build(widgets, footerPane, closeButtonDesc());
```

The detail pane is a stack or scroll view, not several hardcoded rectangles.

### Menu Composition

```cpp
scrim.build(widgets, context.root, scrimDesc());
window.build(widgets, context.root, menuWindowDesc());
buttonStack.build(widgets, window.content(), menuStackDesc());
resume.build(widgets, buttonStack.root(), buttonDesc("Resume"));
settings.build(widgets, buttonStack.root(), buttonDesc("Settings"));
quit.build(widgets, buttonStack.root(), buttonDesc("Quit"));
```

Menu rows are stack children. Dropdowns are popup widgets or modal child views,
not per-frame y-coordinate rewrites.

### HUD Composition

```cpp
hudDock.build(widgets, context.root, hudDockDesc());
healthBar.build(widgets, hudDock.root(), healthDesc());
promptSlot.build(widgets, hudDock.root(), promptSlotDesc());
```

Primitive minimap/debug visualization may use raw nodes inside a dedicated
widget or composition section, but status meters and prompts should be widgets.

### Tooltip Composition

```cpp
tooltipPanel.build(widgets, context.root, tooltipPanelDesc());
tooltipStack.build(widgets, tooltipPanel.content(), tooltipStackDesc());
title.build(widgets, tooltipStack.root(), titleDesc());
body.build(widgets, tooltipStack.root(), bodyDesc());
```

The tooltip service or widget decides placement; individual feature panels do
not compute tooltip coordinates.

## Anti-Patterns

Do not manually compute widget spacing.

Use stack/grid/dock gaps and theme metrics. Manual spacing makes every feature
responsible for responsive behavior.

Do not create fullscreen retained roots for child feature views.

Use existing surfaces, layers, mounts, and frontend slots. Competing fullscreen
roots fight render order, focus, modal policy, and input blocking.

Do not compare raw handles for ordinary controls.

Buttons and widgets own their activation behavior. Compositions consume semantic
widget results.

Do not hardcode colors, font sizes, padding, or border radii.

Use `UiTheme`. Direct payload styling prevents skinning, state styling,
accessibility contrast passes, and editor-driven reuse.

Do not manually calculate button positions.

Use stacks, grids, docks, and constraints.

Do not bypass widgets for known controls.

Raw nodes are for widget internals, primitive visualization, and compatibility.

Do not bypass layout containers.

Canvas and anchors are compatibility paths, not the default model.

Do not bypass themes with feature-local presentation constants.

Feature-local theme builders are fine; presentation literals in composition code
are not.

Do not put gameplay rules in compositions.

Compositions translate UI intent into commands. Feature systems decide whether
the command is legal and apply state changes.

Do not use modals for ordinary child views.

Merchant, crafting, quest, and inventory handoff views are interaction views.
Confirmations and blocking prompts are modals.

## Runtime API Classification

| API / concept | Preferred status |
| --- | --- |
| `UiSystem::mountComposition(...)` | Preferred feature entry point |
| `UiSystem::createMount(...)` | Preferred for attaching child composition slots |
| Surface-aware `UiSystem` overloads | Preferred for runtime-owned UI |
| Default-surface overloads | Compatibility |
| `UiComposition::onEvent(...)` | Preferred interaction path |
| `UiSystem::dispatchResult()` | Compatibility / specialized tools |
| Core widgets in `UiWidget.h` | Preferred controls |
| `UiWidgetAssetDefinition` | Preferred reusable authored structure |
| `UiSystem::createNode(...)` in feature compositions | Low-level exception |
| `UiSystem::setPayload(...)` in feature compositions | Widget internals / compatibility |
| `UiLayoutMode::Canvas` | Compatibility / low-level primitive staging |
| `anchorMin/anchorMax` normalized rects | Compatibility |
| Direct `UiStyle` overrides | Compatibility |
| `UiTheme` style classes/tokens/metrics | Preferred |
| `UiBindingManager` / widget bind helpers | Preferred one-way sync |
| `UiModalManager` | Preferred modal owner |
| Manual input gates for UI blocking | Compatibility; use modal/frontend ownership for UI |

## Migration Guidance

Do not rewrite every feature immediately. Migrate when touching a feature for
real behavior or when a layout bug comes from the old model.

Migration order:

1. Move feature UI into a `UiComposition` if it is not already composition-owned.
2. Replace raw buttons/labels/panels with widgets.
3. Replace row coordinate loops with `UiStackWidget`.
4. Replace cell coordinate loops with `UiGrid` or a feature widget.
5. Move colors, text scale, padding, and gaps into `UiTheme`.
6. Replace handle comparisons with widget `onEvent(...)` and semantic commands.
7. Replace manual modal/fullscreen overlays with `UiModalManager` and frontend
   modal slots.
8. Convert reusable static structures into widget assets when they are stable.

Keep compatibility paths working until their owning features are migrated.

## Review Checklist

Before merging new UI, verify:

- Is the UI mounted through a composition?
- Does the feature use an existing surface/layer/mount slot?
- Are ordinary controls widgets?
- Are lists/stacks/grids expressed by layout containers?
- Are colors, type, padding, skins, and gaps theme-owned?
- Is interaction handled through retained events and widgets?
- Are commands emitted to feature logic instead of applying gameplay inside UI?
- Is navigation metadata present for focusable controls?
- Are semantics/accessibility names present where needed?
- Are modals actual modals?
- Are raw nodes limited to widget internals, tests, tools, or primitive
  visualization?

