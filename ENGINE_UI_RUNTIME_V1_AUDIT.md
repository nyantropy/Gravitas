# Engine UI Runtime v1.0 Audit

Audit date: 2026-07-03

This document records the final architecture validation for the Gravitas UI
runtime before treating it as v1.0. The audit covered the engine runtime, the
migrated DungeonCrawler game UI, and the architecture and authoring documents.

## Result

The UI runtime is stable enough to be considered v1.0.

No missing foundational primitive was found. Future UI work should primarily
build widgets, compositions, feature UI, editor tooling, authored assets,
platform bridges, and performance improvements rather than adding new ownership
or interaction architecture.

## Engine Audit

| Subsystem | Status | Notes |
| --- | --- | --- |
| `UiSurface` | Consistent | Owns a complete surface-local UI universe: document, layers, focus, modals, mounts, input, navigation, drag/drop, animation, binding, accessibility, theme, and coordinate conversion. |
| `UiLayer` | Consistent | Owns coarse ordering, hit-test order, visibility, and input participation. Feature rows, cards, and panels should remain inside mounts/layout containers, not layers. |
| `UiInputDispatcher` | Consistent | Centralizes retained dispatch once per frame. `dispatchResult()` remains compatibility/readback for tools and older code; composition events are preferred. |
| `UiFocusManager` | Consistent | Owns hover, capture, active pointer owner, keyboard/text/navigation focus, focus scopes, and restoration. |
| `UiModalManager` | Consistent | Owns modal stack, layer blocking, cancel routing, and focus-scope restoration. |
| `UiMount` | Consistent | Owns retained subtree lifetime and removes associated binding, animation, navigation, drag/drop, focus, modal, and accessibility state through runtime cleanup paths. |
| `UiComposition` | Consistent | Owns feature-local retained structure, feature UI state, bindings, child composition mounting, and retained event handling. |
| Event propagation | Consistent | Capture, target, and bubble phases route through retained `UiEvent`. `consume()` stops propagation; `preventDefault()` blocks default behavior without stopping propagation. |
| Layout engine | Consistent | Stack, grid, dock, scroll, constraint, aspect, and overlay express structure and flow. Canvas/anchors remain compatibility and low-level escape hatches. Hidden children now behave consistently across layout containers. |
| `UiTheme` | Consistent | Owns semantic colors, metrics, typography, skins, style classes, and state styles. VN profiles seed theme data instead of bypassing it. |
| Widget framework | Consistent | Widgets own reusable retained subtrees and control behavior without owning surfaces, mounts, feature business logic, or renderer resources. Visibility APIs and event ownership are normalized across stock widgets. |
| Navigation graph | Consistent | Widgets and compositions register traversal metadata; focus manager owns resulting focus. Disabled, hidden, and destroyed nodes are pruned through runtime cleanup. |
| Drag/drop runtime | Consistent | Runtime owns armed/dragging/drop/cancel lifecycle, pointer capture, target resolution, and typed opaque payload routing. Inventory item grids consume this path. |
| Animation runtime | Consistent | Runtime owns node/property timelines, interruption, cancellation, and subtree pruning. |
| Data binding | Consistent | Runtime owns one-way source-to-retained-property synchronization and lifetime cleanup. Interaction prompt assets demonstrate game consumption. |
| Accessibility | Consistent | Runtime owns semantic nodes and live announcements. Platform screen reader bridges remain future adapters, not a foundational runtime gap. |
| Serialization | Consistent | Versioned JSON describes durable retained UI structure and behavior references; C++ still owns behavior and binding source resolution. |
| Widget assets | Consistent | Reusable authored widget definitions sit above serialization and below compositions. |
| Visual editor | Consistent | The editor is a tooling client of the same asset/runtime model. Editor UX can evolve without changing runtime architecture. |
| Asset runtime | Consistent | Runtime tracks UI asset consumption and reload participation. |
| Package runtime | Consistent | Packages group authored UI assets and dependencies without changing the composition model. |
| Localization | Consistent | Runtime resolves localized strings through package/resource data. Refresh and workflow improvements are evolutionary. |
| VN / Interaction Frontend | Consistent | `VNInteractionLayout` is the preferred semantic layout model. `VNLayoutProfile` rectangles are adapted as compatibility seeds. Merchant and future NPC views mount into the frontend interaction slot. |

## Game Audit

| Feature area | Adoption status | Compatibility paths retained |
| --- | --- | --- |
| VN / Yune dialogue | Preferred model | Yune uses semantic `VNInteractionLayout`. Legacy `VNLayoutProfile` support remains engine compatibility for old content. |
| Interaction Frontend | Preferred model | Interaction and modal slots are published by `VNFrontendStateComponent`; non-modal NPC views mount into the interaction slot. |
| Merchant | Migrated | Mounted into the Interaction Frontend and uses runtime navigation and item-grid drag/drop. Fixed slot/window geometry remains feature-owned compatibility layout. |
| Inventory / loot container | Migrated | Compositions and shared item grid consume runtime drag/drop. Grid sizing and some normalized rectangles remain feature-local layout compatibility. |
| Menus | Migrated | Composition/widget/navigation model is used. Some dropdown and page layout still uses normalized rectangles until richer widgets replace it. |
| Combat UI | Migrated | Combat panels, buttons, log, HP bars, and banner are composition-owned and widget-driven. World-space nameplate projection remains a specialized presentation path. |
| Dungeon HUD | Migrated with primitive exceptions | HUD shell is composition-owned. Minimap/debug visuals intentionally use raw retained primitives and direct payload colors because they are primitive visualizations. |
| Interaction prompt | Preferred model | Uses a serialized widget asset, binding resolver, composition lifecycle, and runtime input/event flow. |
| Skill tree | Migrated with primitive exceptions | Composition/widgets/navigation own the UI. Raw line nodes remain appropriate for graph-link visualization until a graph widget exists. |
| Quest UI | Future feature | No contradictory implementation was found. Future quest turn-in/log UI should use the Interaction Frontend or normal feature compositions. |
| Tool/editor panels | Runtime client | Engine tools use retained UI and authored assets as tooling clients. Editor-specific affordances remain evolutionary. |

## Compatibility Classification

Preferred paths for new UI:

- `UiComposition` mounted through `UiSystem`.
- reusable widgets and widget assets.
- layout containers with theme metrics.
- `UiTheme` style classes, skins, semantic colors, and typography.
- retained `UiEvent` handling and widget callbacks.
- `UiNavigationGraph`, `UiModalManager`, `UiDragDropManager`, bindings, and accessibility semantics.
- `VNInteractionLayout` for dialogue and NPC interaction frontend layout.

Compatibility paths that may remain indefinitely:

- raw retained nodes inside widgets, renderer/tool adapters, tests, and primitive visualizations.
- canvas/anchor rectangles for low-level bridging, fullscreen fill, edge pinning, and old content.
- `dispatchResult()` readback for tools, tests, and compatibility code.
- world/camera projection code until first-class world-space UI is added.
- `VNLayoutProfile` legacy rectangles, internally normalized into `VNInteractionLayout`.

Compatibility paths to migrate opportunistically:

- normalized `rect(...)` helpers in ordinary game panels.
- feature-local manual row, dropdown, and grid positioning where Stack/Grid/Dock/Scroll can express the same structure.
- direct payload colors and text scales for ordinary controls.
- whole-view sync code that could become binding sources.
- feature-owned inventory/merchant grid layout once richer runtime grid/list widgets exist.

Historical debt that should not be copied:

- handle-bundle UI architecture outside widgets.
- per-frame dispatch polling for button behavior.
- fullscreen retained roots that bypass existing shells or slots.
- gameplay systems mutating retained UI directly.
- new coordinate rectangles for VN or interaction frontend layout.

## Documentation Audit

The authoring guide, engine architecture, runtime architecture investigation,
and game architecture now describe the same model:

`UiSurface -> UiLayer -> UiMount -> UiComposition -> UiWidget / UiWidgetAsset -> retained UiNode -> renderer`

The main correction from this audit was documentation drift. Older text still
described semantic VN layout as future work and described Yune's profile as a
compatibility seed. The current model is that `VNInteractionLayout` is preferred
and `VNLayoutProfile` is compatibility-only.

## Performance Review

No architecture-level performance blocker was found.

Acceptable current costs:

- layout invalidation is retained-tree scoped and explicit enough for current UI scale.
- command extraction and visual rebuild are straightforward retained UI costs.
- text updates, typewriter dialogue, and binding refresh are clear optimization targets but not architectural gaps.
- theme resolution is centralized and can be cached further without changing authoring rules.
- widget synchronization is simple and predictable; more bindings can reduce manual snapshot updates over time.

Evolutionary performance work:

- virtualized list/grid/tree widgets.
- finer dirty-region visual rebuilds.
- cached text measurement and wrapping for stable-width labels.
- lower-allocation command extraction.
- binding-source batching.
- animation update profiling for large surfaces.

## Future Work

Foundational work:

- none known from this audit.

Evolutionary work:

- richer grid, list, tree, dropdown, tab, tooltip, and inspector widgets.
- world-space UI projection as a runtime presentation extension.
- more authored widget packages and editor workflows.
- platform accessibility bridge adapters.
- localization refresh/hot reload and authoring workflow improvements.
- additional theme assets, skins, and animation polish.
- broader use of `UiBindingManager` in game UI.
- runtime performance profiling and targeted optimization.

## Scores

| Area | Score | Reason |
| --- | --- | --- |
| Architecture | 9/10 | Ownership boundaries are clear and no foundational primitive is missing. World-space UI remains an extension area. |
| Consistency | 8.5/10 | Engine behavior is coherent; remaining inconsistency is mostly old game layout style, now classified as compatibility. |
| Maintainability | 8/10 | Feature UI is composition-owned and auditable. Manual rect/grid code still adds local maintenance cost in older panels. |
| Authoring ergonomics | 8/10 | New authors have one recommended path. Ergonomics will improve further with richer stock widgets and less need for custom grids. |
| Performance | 7.5/10 | Current costs are acceptable and understandable. Virtualization, text caching, and finer invalidation are future optimization work. |
| Extensibility | 9/10 | New features can be built as widgets, compositions, assets, or packages without new engine ownership concepts. |
| Game integration | 8/10 | Major game UI is migrated. Remaining raw primitive and normalized-rect paths are mostly specialized or opportunistic migration work. |

## Recommendation

Treat the Gravitas UI Runtime as UI Runtime v1.0.

Future development should primarily consist of building game features with the
runtime, expanding the widget library, improving authored asset and editor
workflows, adding presentation extensions such as world-space UI, and optimizing
known hot paths. A new foundational runtime phase should require evidence of a
real ownership, interaction, layout, or lifecycle gap that cannot be solved by
widgets, compositions, themes, assets, or tooling.
