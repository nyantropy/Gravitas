# ECS Execution Ownership

The permanent source layers are core mechanisms, module capabilities and runtime
composition. All Gravitas-specific execution identities, labels, defaults and
coordinated recipes belong to `runtime/execution/`. Modules receive explicit typed
values and never include or link runtime. There is no execution-policy module.

## Core Mechanism

Core owns `EcsSystemGroup`, mask operations, timing records and
`EcsExecutionSelection`. The scheduler retains two ordered lists: fixed-step
simulation systems and per-frame controllers. Groups filter participation; they
never sort systems or establish phases. Filtering is evaluated before each system,
so selection changes affect later systems in the same pass. Structural commands
flush after each executed system. Masked systems produce no timing sample and
consume no timing instance index.

The single selection stack carries an ID, mask and one optional typed value payload.
Push, pop, guarded pop and clear preserve selection and presentation metadata
atomically. Core does not inspect that payload. No extra stack, component, registry,
service lookup or scheduler abstraction is introduced.

Bare core worlds have an unnamed, unfiltered default. The existing
`configureDefaultExecutionSelection` operation establishes a default once, replaces
only the bottom entry and preserves active overlays. Clear restores that configured
value and metadata. Core never chooses an engine preset.

## Authoritative Runtime Catalog

`runtime/execution/BuiltinExecutionGroups.h` defines the sole named catalog and
`ecsSystemGroupName`. The constants remain `gts::execution::groups::<Name>` values
of the opaque core type.

| Name | Bit | Value |
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

Labels retain exact spelling, including Ui and VN. Unknown or composite identities
still label as Unknown. Membership remains any-bit overlap; zero matches nothing.
Always has no bypass semantics. Numeric values never determine execution order.

`SceneExecutionProfile` and `SceneExecutionPolicy` are runtime-owned. Conversion
to `EcsExecutionSelection` copies ID/mask and the complete typed policy payload.
TimePolicy stays descriptive metadata and does not operate any clocks.

| Recipe | Runtime header | ID | Mask | Frame mode | Time metadata |
| --- | --- | --- | --- | --- | --- |
| gameplay() | SceneExecutionProfile.h | gameplay | 0xFFF | FullWorld | AllRunning |
| dialogueOverlay() | VNExecutionProfiles.h | dialogue_overlay | 0xF99 | FullWorld | GameplayPausedUiRunning |
| fullscreenDialogue() | VNExecutionProfiles.h | fullscreen_dialogue | 0xF81 | UiOnly | GameplayPausedDialogueRunning |
| pauseMenu() | SceneExecutionProfile.h | pause_menu | 0x981 | CachedWorldFrame | GameplayPausedUiRunning |

Value-initialized SceneExecutionProfile still has ID gameplay and mask zero.
Audio, pauseMenu, EverythingPaused and FrameBuildMode::None are preserved without
activating new behavior.

## Module Installation Inputs

Simple installers require the default `EcsExecutionSelection` and their opaque
group values as explicit arguments. No argument silently substitutes engine or
neutral policy. The supplied default is applied at the previous installation point
only if no default has been configured. A pre-existing default wins, and active
overlays remain intact.

- Transform runtime takes a default selection; resolver/complete installation also
  takes a resolver group. Runtime setup still schedules no controller.
- Transform animation takes a default and simulation group.
- Physics takes a default and simulation group, forwards the default to transform
  runtime, then registers its existing simulation system. Explicit transform
  resolution before collision queries is unchanged.
- Debug drawing takes a default and drawing group. The physics diagnostics bridge
  forwards both through the generic debug-draw installer.
- `RendererExecutionInputs` contains the default plus preparation, texture-animation,
  camera and particle identities. Renderer installation forwards preparation to
  the transform resolver and retains all existing controller order and scene guards.

`gts::execution::rendererExecutionInputs()` constructs the standard renderer value
at the runtime boundary. Standalone callers can supply their own values; module-only
tests use bits outside the Gravitas catalog. Engine context construction continues
calling `ensureExecutionPolicy(world)` before scene load/update. Standalone installers
receive the same gameplay value explicitly, retaining gameplay mask 0xFFF rather
than accidentally enabling all unknown bits.

## Rendering Presentation Contract

`modules/rendering/contracts/FrameBuildMode.h` owns the unchanged four enum values.
`RenderingRuntime` requires a non-null stateless `FrameBuildModeSelector`:

```cpp
using FrameBuildModeSelector = FrameBuildMode (*)(const EcsExecutionSelection&);
```

Runtime supplies `gts::execution::selectFrameBuildMode`. It projects the complete
runtime-owned payload to the rendering enum and returns FullWorld for selections
without that payload. Rendering invokes the function at the original read point,
after scene statistics contribution and before extraction. It never names or
interprets SceneExecutionPolicy or TimePolicy. A standalone renderer may explicitly
supply an always-FullWorld selector.

FullWorld extracts world/model data and can submit particles. UiOnly skips world
construction while retained UI continues. CachedWorldFrame reuses world data. None
skips world/UI construction but not backend submission or independent preview work.
Retained UI input, binding and animation work remains distinct from the ECS Ui bit.

## VN, Tools And Benchmarks

`VNExecutionInputs` contains prepared default, overlay and fullscreen selections.
VNSystem owns a copy, establishes the supplied default at the start of update, and
copies the selected template onto the core stack. Existing same-ID idempotence and
expected-ID guarded pop remain unchanged. Runtime's `gts::vn::executionInputs()`
constructs the standard recipes. VN determines when to transition, not which engine
capabilities belong in coordinated masks. Changes still affect later controllers
and rendering that frame, and simulation on the next tick.

`ToolExecutionInputs` contains external timing identity and preview installation
inputs. The tool runtime retains it across scene-system recreation; shell and preview
coordinator construction forward it into both preview worlds. Preview worlds own
copies across destroy/reinstall. They establish defaults only at successful ensure,
retaining resource-null behavior and their four-controller camera setup.
The seven external tool controllers still run directly in the original order,
flush commands and record supplied timing identities regardless of world filtering.

`BenchmarkExecutionInputs` contains renderer installation values and a non-null
stateless group-label function. The CPU harness consumes it without modifying
workload configuration or serialization. Runtime/application composition supplies
`ecsSystemGroupName`. Controller names, instance suffixes and timing keys are unchanged.

Engine pause still stops fixed simulation ticks while controllers/rendering continue.
A selection never pauses the engine clock. No transform, physics or main-loop ordering
changes accompany policy injection.

## Targets And Enforcement

Arrows mean depends on:

```text
gravitas_runtime -> gravitas_runtime_execution
                    |-> gravitas_core
                    |-> gravitas_rendering_execution_contracts
                    |     |-> gravitas_core
                    |     +-> gravitas_rendering_contracts
                    +-> gravitas_vn_execution_contracts -> gravitas_core

modules -> core and explicit module contracts
modules -X-> runtime
core -X-> modules/runtime
```

Rendering execution contracts contain only value inputs; rendering resource/frame-mode
contracts remain available without a backend. Runtime policy is available in reduced
configurations without rendering, VN, tools or physics implementations. Applications
and policy integration tests link the runtime target explicitly. Module-only tests
link capability targets and reject visibility of runtime headers.

The existing three-layer CMake checker is unchanged. It rejects upward links,
transitive wrappers/aliases, include-directory leaks and forbidden source includes.
Characterization covers the catalog, all presets, ordering/filtering/flush behavior,
stack/default semantics, pause, VN transitions, all frame modes and their read point,
transform/physics resolution, previews, tool resets and benchmark labels. Injected
non-Gravitas identities/defaults additionally prove modules consume supplied values.
