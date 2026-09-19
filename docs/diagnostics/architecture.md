# Diagnostics Architecture

`engine/modules/diagnostics/` owns profiling, generic debug visualization and
explicit feature bridges. It has separate targets:

- `gravitas_profiling`: always-available header-only telemetry schema and
  schema-specific accumulation, without rendering, physics or debug-draw dependencies.
- `gravitas_debugdraw`: static implementation of generic drawing; consumes
  `gravitas_core`, `gravitas_transform` and `gravitas_rendering`.
- `gravitas_diagnostics_physics`: collider visualization; consumes core,
  transform, physics and debug drawing. Base physics remains renderer-independent.

There is no monolithic diagnostics runtime or library.

## Profiling Ownership

`profiling/GtsFrameStats.h` and `profiling/ProfileAccumulator.h` retain the
existing telemetry fields, layouts and accumulation behavior. The schema includes
renderer and game counters and belongs to diagnostics rather than foundational
core. The modules parent registers profiling before rendering so its consumers
can link the leaf target directly. Optional scene participation belongs to
`profiling/ISceneFrameStats.h`, with default no-op `populateFrameStats` (const)
and `onFrameStats` operations. A participating scene inherits this interface
alongside `GtsScene`; ordinary scenes have no telemetry dependency. Core neither
declares nor links the schema or participation interface.

`RenderingRuntime` detects the interface and keeps the established order:
initial statistics → optional contribution → extraction/rendering statistics →
submission/final statistics → optional observation → accumulator. Backend results
with a mismatched frame index still fall back to the current submission statistics.
`GtsScene3` and the game's `DungeonTestScene` contribute their existing counters;
`RuntimeBenchmarkScene` observes final statistics once per rendered frame, retaining
warmup, GPU-timing availability, measurement and quit timing.

`scene_frame_stats_runtime` exercises the real rendering runtime with a CPU fake
graphics backend: participating/nonparticipating scenes, observation-only benchmark
behavior, contribution/extraction/submission order, final GPU/backend fields,
stale-result fallback and observation before accumulation. It also runs in reduced
rendering configurations without Vulkan or physics.

## Submission And Realization

```text
tool gizmos / bounds / camera helpers / physics visualization
                            ↓
DebugDrawPrimitives + world-owned DebugDrawQueueComponent
                            ↓
DebugDrawSystem: color batches, geometry signatures and line-box meshes
                            ↓
dynamic mesh + material descriptors + rendering invalidation
                            ↓
renderer-owned GPU resources
```

The primitive helpers and queue remain public. `DebugDrawSystem.hpp` provides
the system declaration and value-owned batch state; its `.cpp` owns geometry
construction, materials and rendering integration. Public headers no longer
pull in its rendering implementation. Copy/reset behavior of the system's
batch arrays and its normal ECS ordering are unchanged.

`DebugDrawRenderableComponent` stays in diagnostics: tool selection consumes
the marker to distinguish diagnostic entities. Queue consumption, batch cache
behavior, materials, colors and mesh output are unchanged.

## Tool Policy And Physics Bridge

Selected/all bounds, axes, camera frustum and pick-ray settings live in
`modules/tools/debugdraw/DebugDrawSettingsComponent.h`, together with the
existing `ensureSettings` helper. The `gts::debugdraw` namespace and public
type/helper names are preserved. Generic primitive submission does not include
or own tool display policy. Tools explicitly includes the settings header.

`EngineToolRuntime` continues to run its debug-drawing system after its existing
producers and reset scene-facing systems on scene transitions. The separate
debug-draw and physics scene installers are preserved unchanged.

`PhysicsDebugRenderer` reads collider/world-transform components, emits three
rings per sphere and reports its existing profiling counters. It remains a
consumer of generic debug drawing rather than part of base physics.

## Build Policy And Verification

`GTS_ENABLE_DEBUGDRAW` requires `GTS_ENABLE_RENDERING`, independently of
`GTS_ENABLE_PHYSICS`. It does not require a Vulkan backend. The diagnostics
parent registers the generic target when debug drawing is enabled, and registers
the physics bridge only when both debug drawing and physics are enabled.
`gravitas_modules` aggregates only targets that exist.

| Debug drawing | Physics | Diagnostics targets |
| --- | --- | --- |
| Off | Off or on | `gravitas_profiling` |
| On | Off | `gravitas_profiling`, `gravitas_debugdraw` |
| On | On | `gravitas_profiling`, `gravitas_debugdraw`, `gravitas_diagnostics_physics` |

Tools retains its separate physics dependency for entity-selection metadata.
A configuration with physics disabled must also disable tools; this does not
restrict generic debug drawing. CMake diagnoses missing rendering for debug
drawing and missing physics for tools explicitly.

`debugdraw_runtime` links only `gravitas_debugdraw` and characterizes line
geometry/color, queue consumption, unchanged/changed batch versions and
renderable removal. `physics_debugdraw_runtime` links the physics bridge and
checks collider visualization through the same generic drawing implementation.
It is registered only when the bridge exists.

Module smoke tests configure and build both debugdraw-without-physics and
debugdraw-with-physics configurations, with tools and Vulkan disabled. They also
execute the corresponding CPU tests. Configure-time test assertions check target
presence and module aggregation against the selected options. Negative tests
retain the actual rendering prerequisite and tools' own physics prerequisite.
The full tools and runtime smoke suites cover the default integration paths.

Debug-draw and physics-debug scene installers require a supplied default execution
selection and drawing group. Physics visualization forwards the values through the
generic debug-draw installer. Gravitas composition supplies its Tools identity;
diagnostics does not include the runtime catalog or manufacture gameplay defaults.
