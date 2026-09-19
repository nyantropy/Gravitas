# Transform Architecture

`engine/modules/transform/` owns local transforms, hierarchy, dirty propagation,
world-matrix resolution and publication. `gravitas_transform` is a static
library with a single public dependency, `gravitas_core`. Rendering, physics,
tools, debug drawing and transform animation consume that target. Its include
directories are exported only by the owning target; `gravitas_modules` retains
aggregate access for engine/game consumers.

## Public Boundary

- `TransformComponent.h`: authored local position, Euler rotation, scale and version.
- `WorldTransformComponent.h`: resolved world matrix and version for readers.
- `TransformDirtyHelpers.h`: dirty notification after authored mutation.
- `HierarchyComponent.h` and `TransformHierarchyHelpers.h`: hierarchy data and
  attachment/detachment, including preserve-local and preserve-world policies.
- `TransformMatrixHelpers.h`: shared matrix/transform operations.
- `TransformSceneFeature.h`: world/scene installation and reset declarations.
- `TransformWorldResolver.h`: explicit resolution used by physics as well as
  the transform controller; metrics remain available for existing consumers.
- `TransformInvalidationLifecycle.h`: queue, reset and published-transform
  callback registration declarations used by integration code.

The generic ECS authoring model and existing namespaces are unchanged. Physics
can resolve transforms before its collision query; rendering subscribes to
world-transform publication and reads the published components. Neither
consumer needs to implement hierarchy resolution itself.

## Implementation And Ownership

```text
components + hierarchy edits + dirty notification
                      ↓
invalidation queue → work collection → hierarchy batches → matrix resolution
                      ↓
WorldTransformComponent + publication callback
                      ↓
physics / rendering / tools / other readers
```

`runtime/TransformInvalidationLifecycle.cpp` holds the existing world-keyed
registries. Its internal state declaration is under `runtime/detail/` and is
not transitively included by public headers. `systems/TransformWorldResolver.cpp`
owns the resolution phases; its class retains the same value-owned scratch
vectors. `TransformSystem.cpp` adapts resolution to the ECS controller.
`scene/TransformSceneFeature.cpp` owns installation and reset implementation.
No new manager, allocation indirection or runtime ownership layer is introduced.

Authored/resolved components remain world-owned. Resolution scratch storage
remains resolver-owned. Metrics retain their existing static storage. The
structural refactor does not change matrix calculations, versions, callbacks,
system groups, registration order or reset behavior.

## Deferred Architectural Questions

The invalidation and publication registries remain static maps keyed by
`ECSWorld*`. Reset erases dirty state but does not remove published callbacks;
automatic cleanup for standalone worlds is not introduced here. World lifetime
ownership and callback removal need a separate decision.

Both `installTransformRuntime` and `installTransformResolver` still add a
`TransformSystem`. The combined installer still calls both, preserving the
existing duplicate registration and scheduling. Whether to consolidate those
registrations must be addressed separately, including physics timing and metrics.

## Verification

Transform hierarchy and transform-animation tests now link the owning targets
and are registered independently of Vulkan runtime tests. The physics
integration test checks that fixed-step physics resolves hierarchical placement
and observes parent dirty propagation without rendering. Rendering lifecycle
tests cover the existing publication bridge.
