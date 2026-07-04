# Engine UI Runtime v1.0 Audit

Audit date: 2026-07-04

This document records the engine-level architecture validation for the Gravitas
UI runtime before treating it as v1.0. It intentionally excludes
application-specific migration evidence, feature names, content, and business
rules. Application integration audits belong in application documentation.

## Result

The UI runtime is stable enough to be considered v1.0.

No missing foundational engine primitive was found. Future engine UI work should
primarily build widgets, compositions, authored assets, editor tooling, platform
bridges, presentation extensions, and performance improvements rather than
adding new ownership or interaction architecture.

## Engine Audit

| Subsystem | Status | Notes |
| --- | --- | --- |
| `UiSurface` | Consistent | Owns a complete surface-local UI universe: document, layers, focus, modals, mounts, input, navigation, drag/drop, animation, binding, accessibility, localization scope, active theme, and coordinate conversion. |
| `UiLayer` | Consistent | Owns coarse ordering, hit-test order, visibility, and input participation. Rows, cards, and child panels remain inside mounts and layout containers. |
| `UiInputDispatcher` | Consistent | Centralizes retained dispatch once per frame. `dispatchResult()` remains compatibility/readback for tools and legacy adapters; composition events are preferred. |
| `UiFocusManager` | Consistent | Owns hover, capture, active pointer owner, keyboard/text/navigation focus, focus scopes, and restoration. |
| `UiModalManager` | Consistent | Owns modal stack, layer blocking, cancel routing, and focus-scope restoration. |
| `UiMount` | Consistent | Owns retained subtree lifetime and removes associated binding, animation, navigation, drag/drop, focus, modal, and accessibility state through runtime cleanup paths. |
| `UiComposition` | Consistent | Owns feature-local retained structure, UI state, bindings, child composition mounting, and retained event handling. |
| Event propagation | Consistent | Capture, target, and bubble phases route through retained `UiEvent`. `consume()` stops propagation; `preventDefault()` blocks default behavior without stopping propagation. |
| Layout engine | Consistent | Stack, grid, dock, scroll, constraint, aspect, and overlay express structure and flow. Canvas/anchors remain compatibility and low-level escape hatches. Hidden children behave consistently across layout containers. |
| `UiTheme` | Consistent | Owns semantic colors, metrics, typography, skins, style classes, and state styles. Higher-level profiles seed theme data instead of bypassing it. |
| Widget framework | Consistent | Widgets own reusable retained subtrees and control behavior without owning surfaces, mounts, domain rules, or renderer resources. Visibility APIs and event ownership are normalized across stock widgets. |
| Navigation graph | Consistent | Widgets and compositions register traversal metadata; the focus manager owns resulting focus. Disabled, hidden, and destroyed nodes are pruned through runtime cleanup. |
| Drag/drop runtime | Consistent | Runtime owns armed/dragging/drop/cancel lifecycle, pointer capture, target resolution, and typed opaque payload routing without interpreting domain payload meaning. |
| Animation runtime | Consistent | Runtime owns node/property timelines, interruption, cancellation, and subtree pruning. |
| Data binding | Consistent | Runtime owns one-way source-to-retained-property synchronization and lifetime cleanup. |
| Accessibility | Consistent | Runtime owns semantic nodes and live announcements. Platform screen reader bridges remain future adapters, not a foundational runtime gap. |
| Serialization | Consistent | Versioned JSON describes durable retained UI structure and behavior references; C++ code still owns behavior and binding source resolution. |
| Widget assets | Consistent | Reusable authored widget definitions sit above serialization and below compositions. |
| Visual editor | Consistent | The editor is a tooling client of the same asset/runtime model. Editor UX can evolve without changing runtime architecture. |
| Asset runtime | Consistent | Runtime tracks UI asset consumption and reload participation. |
| Package runtime | Consistent | Packages group authored UI assets and dependencies without changing the composition model. |
| Localization | Consistent | Runtime resolves localized strings through package/resource data. Refresh and workflow improvements are evolutionary. |
| VN / Interaction Frontend | Consistent | `VNInteractionLayout` is the preferred semantic layout model. `VNLayoutProfile` rectangles are adapted as compatibility seeds. Non-modal child views mount into interaction slots; blocking views mount into modal slots. |

## Compatibility Classification

Preferred paths for new UI:

- `UiComposition` mounted through `UiSystem`.
- reusable widgets and widget assets.
- layout containers with theme metrics.
- `UiTheme` style classes, skins, semantic colors, and typography.
- retained `UiEvent` handling and widget callbacks.
- `UiNavigationGraph`, `UiModalManager`, `UiDragDropManager`, bindings, and
  accessibility semantics.
- `VNInteractionLayout` for VN and interaction frontend layout.

Compatibility paths that may remain indefinitely:

- raw retained nodes inside widgets, renderer/tool adapters, tests, and
  primitive visualizations.
- canvas/anchor rectangles for low-level bridging, fullscreen fill, edge
  pinning, and old content.
- `dispatchResult()` readback for tools, tests, and compatibility code.
- projection adapters until first-class projection surfaces are added.
- `VNLayoutProfile` legacy rectangles, internally normalized into
  `VNInteractionLayout`.

Compatibility paths to migrate opportunistically:

- normalized rectangle helpers in ordinary panels.
- feature-local manual row, dropdown, and grid positioning where layout
  containers can express the same structure.
- direct payload colors and text scales for ordinary controls.
- whole-view sync code that could become binding sources.
- custom grid/list implementations once richer runtime widgets exist.

Historical debt that should not be copied:

- handle-bundle UI architecture outside widgets.
- per-frame dispatch polling for button behavior.
- fullscreen retained roots that bypass existing shells or slots.
- domain systems mutating retained UI directly.
- new coordinate rectangles for VN or interaction frontend layout.

## Documentation Audit

The engine authoring guide, engine architecture, runtime architecture document,
and v1.0 audit now describe the same engine model:

`UiSurface -> UiLayer -> UiMount -> UiComposition -> UiWidget / UiWidgetAsset -> retained UiNode -> renderer`

The main correction from this audit was separation-of-concerns drift. Earlier
engine UI documents mixed engine runtime rules with application migration
evidence. Engine documents now describe runtime ownership, generic extension
points, compatibility paths, and authoring rules. Application-specific adoption
status belongs in application documentation.

## Performance Review

No architecture-level performance blocker was found.

Acceptable current costs:

- layout invalidation is retained-tree scoped and explicit enough for current
  UI scale.
- command extraction and visual rebuild are straightforward retained UI costs.
- text updates, progressive text display, and binding refresh are clear
  optimization targets but not architectural gaps.
- theme resolution is centralized and can be cached further without changing
  authoring rules.
- widget synchronization is simple and predictable; more bindings can reduce
  manual snapshot updates over time.

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
- projection surfaces as a runtime presentation extension.
- more authored widget packages and editor workflows.
- platform accessibility bridge adapters.
- localization refresh/hot reload and authoring workflow improvements.
- additional theme assets, skins, and animation polish.
- broader use of `UiBindingManager`.
- runtime performance profiling and targeted optimization.

## Scores

| Area | Score | Reason |
| --- | --- | --- |
| Architecture | 9/10 | Ownership boundaries are clear and no foundational primitive is missing. Projection surfaces remain an extension area. |
| Consistency | 8.5/10 | Engine behavior is coherent; remaining ambiguity is mostly compatibility API surface, now classified. |
| Maintainability | 8/10 | Composition/widget ownership is auditable. Older compatibility paths still require discipline. |
| Authoring ergonomics | 8/10 | New authors have one recommended path. Ergonomics will improve further with richer stock widgets. |
| Performance | 7.5/10 | Current costs are acceptable and understandable. Virtualization, text caching, and finer invalidation are future optimization work. |
| Extensibility | 9/10 | New UI can be built as widgets, compositions, assets, or packages without new engine ownership concepts. |
| Integration model | 8.5/10 | Runtime extension points are clear; applications can validate adoption in their own documentation. |

## Recommendation

Treat the Gravitas UI Runtime as UI Runtime v1.0.

Future engine development should primarily consist of expanding the widget
library, improving authored asset and editor workflows, adding presentation
extensions, integrating platform bridges, and optimizing known hot paths. A new
foundational runtime phase should require evidence of a real ownership,
interaction, layout, or lifecycle gap that cannot be solved by widgets,
compositions, themes, assets, or tooling.
