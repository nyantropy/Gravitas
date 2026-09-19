# Debug Visualization Architecture

`engine/modules/diagnostics/` owns generic debug visualization and explicit
feature bridges. Its parent CMake file registers separate targets:

- `gravitas_debugdraw`: static implementation of generic drawing; consumes
  `gravitas_core`, `gravitas_transform` and `gravitas_rendering`.
- `gravitas_diagnostics_physics`: collider visualization; consumes core,
  transform, physics and debug drawing. Base physics remains renderer-independent.

There is no monolithic diagnostics runtime or library.

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

The existing `GTS_ENABLE_DEBUGDRAW` requirement for both rendering and physics
is preserved, including the rejection test when physics is disabled. Whether
generic debug drawing should support a physics-free configuration is a separate
architectural question; tools also has its own physics dependency.

The CPU diagnostic test characterizes line geometry/color, queue consumption,
unchanged/changed batch versions, renderable removal and collider visualization.
Tools tests, engine runtime smoke tests and module-configuration builds exercise
the existing integration paths.
