# ECS Execution Policy Ownership

The scheduler remains two ordered lists: fixed-step simulation systems and
per-frame controllers. Groups are filters, never phases. Filtering is evaluated
before each system against the current selection; queued structural commands
flush after each executed system. Masked systems produce no timing sample and
consume no timing instance index.

## Ownership Blocker

Engine-wide execution vocabulary and presentation coordination belong in
`runtime/execution/` under the permanent [source-layer rule](../modules/ownership.md).
They currently remain in `modules/execution/`: transform/animation/rendering and
other standalone installers configure gameplay defaults, VN creates coordinated
profiles, tools register categories, and rendering benchmarks interpret labels.
Moving the existing policy target would make those modules depend on runtime.
Preserving their current APIs requires deferring that relocation until explicit
policy inputs can be supplied at composition/standalone entry points. No upward
include or target link is introduced as a workaround.

## Fixed Catalog

`modules/execution/BuiltinExecutionGroups.h` owns the coordinated catalog and
`ecsSystemGroupName`. No dynamic group registration is involved.

| Category | Bit | Value |
| --- | --- | --- |
| Always | 0 | 0x001 |
| Gameplay | 1 | 0x002 |
| Physics | 2 | 0x004 |
| Camera | 3 | 0x008 |
| RenderPrep | 4 | 0x010 |
| Particles | 5 | 0x020 |
| Animation | 6 | 0x040 |
| Audio | 7 | 0x080 |
| Ui | 8 | 0x100 |
| Dialogue | 9 | 0x200 |
| VN | 10 | 0x400 |
| Tools | 11 | 0x800 |

Identities are `gts::execution::groups::<Category>` values of core's opaque
`EcsSystemGroup` type. All labels remain exact, including `Ui` and `VN`; unknown
identities still label as `Unknown`. Membership remains any-bit overlap, zero
matches nothing, and `Always` has no special bypass semantics. Numeric group
values do not establish ordering.

## One Coherent Stack

Core's `EcsExecutionSelection` owns an ID, mask and optional typed value payload.
Runtime `SceneExecutionProfile` converts to this value with a
`SceneExecutionPolicy{frameBuildMode, timePolicy}` payload. `ECSWorld` has exactly
one active selection stack; push/pop/guarded-pop replace or restore mask and
presentation together. The separate default value is reset configuration, not a
second active stack. No component/entity or global registry is introduced.

Callers use `world.pushExecutionSelection(profile)` and
`world.popExecutionSelection(expectedId)`. The bottom entry cannot be popped.
Guard mismatch changes nothing. Selection queries return const borrowed access;
copying a selection copies its payload. Editing the source profile afterward
cannot mutate a stored entry. Lower entries are not intersected with the top.

Rendering uses `gts::execution::sceneExecutionPolicy(world)`. A neutral selection
without presentation metadata has default FullWorld/AllRunning interpretation.
The type-erased storage in core is accessed by exact type only; core never imports
or interprets presentation fields.

## Defaults And Presets

Core-only worlds have an unnamed, unfiltered default and no engine vocabulary.
`gts::execution::ensureExecutionPolicy(world)` supplies the old engine gameplay
selection once. Engine context construction does this before scene load/update.
Standalone transform runtime/resolver, animation, debug-draw and rendering
geometry/camera/particle installers also do it; direct VN updates cover standalone VN worlds. Preview and
benchmark worlds use these same installers. `configureDefaultExecutionSelection`
never replaces an explicitly configured default or an active overlay. World clear
restores the configured default without adding entities or systems.

A module-aware caller constructing a raw world without any installer can call
`ensureExecutionPolicy` explicitly. In particular, the engine default remains
`0xFFF`, not an all-64-bits mask: unknown bits are disabled under gameplay.

| Recipe | Owner | ID | Mask | Frame mode | Time metadata |
| --- | --- | --- | --- | --- | --- |
| gameplay() | execution | gameplay | 0xFFF | FullWorld | AllRunning |
| dialogueOverlay() | VN contracts | dialogue_overlay | 0xF99 | FullWorld | GameplayPausedUiRunning |
| fullscreenDialogue() | VN contracts | fullscreen_dialogue | 0xF81 | UiOnly | GameplayPausedDialogueRunning |
| pauseMenu() | execution | pause_menu | 0x981 | CachedWorldFrame | GameplayPausedUiRunning |

Value-initialized `SceneExecutionProfile{}` still has ID gameplay but mask zero.
Audio participation, pauseMenu, TimePolicy::EverythingPaused and FrameBuildMode::None
are retained. TimePolicy is descriptive only; no clock or pause behavior is inferred.

## Runtime Semantics

Engine pause still stops fixed simulation ticks and preserves the accumulator
remainder. Controllers/rendering continue; unscaled time and input PausePolicy
retain their established behavior. A scene selection does not pause the engine
clock. Physics still resolves transforms explicitly before its collision query,
and the presentation TransformSystem remains at its original registration point.

VN changes selections during its controller update: later systems can observe the
new mask immediately; earlier systems and simulation ticks are not rerun. Rendering
sees the current policy that frame. VN uses guarded pop so another owner's overlay
cannot be removed accidentally. Native VN input blocking remains distinct.

FullWorld builds/extracts world/model data and can submit particles. UiOnly skips
world construction while retained UI continues. CachedWorldFrame reuses world
commands/material/model data. None skips world/UI construction but does not skip
backend submission or independent preview handling. UI input dispatch remains
before simulation, while retained UI bindings/animation/extraction are rendering
work rather than ECS Ui-group execution.

World-registered Tools systems remain maskable. The seven external engine tool
controllers still run directly in their original order and use Tools only as a
timing label; they bypass world filtering.

## Build Boundary And Verification

`gravitas_execution_policy` is always available and links only `gravitas_core`.
`gravitas_vn_execution_contracts` links only that policy contract. Rendering,
transform, physics, diagnostics, tools and their installers depend on the leaf
policy contract; rendering never depends on VN. No optional backend dependency
or aggregate-module include workaround is introduced.

Characterization covers ordered/interleaved systems, both execution lists,
mid-pass changes, command flushing, zero/composite/unknown masks, every fixed
identity and preset, stack reset and guarded pop, engine pause, VN transitions,
frame-build modes, external tool order, preview defaults and benchmark labels.
Existing physics-before-query and transform resolver placement tests remain in
place. Core-only tests use neutral identities and reject feature policy headers.
