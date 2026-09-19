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
- `TransformSceneFeature.h`: world/scene installation and unified runtime-state reset.
- `TransformWorldResolver.h`: explicit resolution used by physics as well as
  the transform controller; metrics remain available for existing consumers.
- `TransformInvalidationLifecycle.h`: queue and published-transform
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

`runtime/TransformInvalidationLifecycle.cpp` holds the world-keyed invalidation
and publication registries. Their internal state declaration is under `runtime/detail/` and is
not transitively included by public headers. `systems/TransformWorldResolver.cpp`
owns the resolution phases; its class retains the same value-owned scratch
vectors. `TransformSystem.cpp` adapts resolution to the ECS controller.
`scene/TransformSceneFeature.cpp` owns installation and reset implementation.
No new manager or service framework is introduced.

Authored/resolved components remain world-owned. Resolution scratch storage
remains resolver-owned. Metrics retain their existing static storage. The
runtime does not change matrix calculations, versions, dirty propagation,
publication order/deduplication, system groups or registration order.

## World Lifetime Contract

Runtime and resolver installation both establish one idempotent transform
world-state lifetime registration. Lazy dirty-state access and publication
callback registration establish the same registration, so explicit resolution
or authoring before scene installation cannot leave unowned registry entries.
No extra entity or ECS system is created for lifetime tracking.

The world owns a small, opt-in teardown callback list. Transform registers one
callback that erases **both** its invalidation entry (queued entity IDs and
deduplication flags) and its publication entry (ordered callback pointers).
`resetTransformSceneFeature(world)` uses that same complete release operation;
the previous dirty-only reset entry point is removed.

Teardown callbacks execute after `ECSWorld::clear()` has removed entities,
systems and event subscriptions. On destruction, their member is destroyed
last, after the rest of world storage. They may use only world identity to
release external state, must not access ECS storage, and must not throw.
The world clears the callback list after each teardown; subsequent installation
or state creation registers it for the next lifetime. Repeated registration
within one lifetime is deduplicated by function pointer.

Scenes no longer use an early transform reset hook before `world.clear()`.
This matters because hierarchy-removal callbacks can enqueue fresh dirty work
while entities are being cleared. Releasing at the end removes that work too.
Scene unload/reset and destruction without unload both release all transform
registry entries. `AssetPreviewWorld` and `ParticlePreviewWorld` use the same
contract through their existing installation, `world.clear()`, and world-member
destruction; their owners need no transform-specific teardown calls.

Destroyed-world addresses cannot retain transform dirty state or callbacks.
The registry holder also handles static-destruction ordering: if its maps have
already been destroyed, a later static-world teardown does not access them.
The registries remain implementation storage, not an application-facing service.

Resolver scratch vectors still belong to the resolver/system and are rebuilt
for each nonempty resolution. Static metrics and the detailed-metrics switch
carry no world identity. Rendering's geometry/material/invalidation registries
are separately owned; this contract releases the transform publication hook,
not unrelated rendering resources.

## Deferred Scheduling Question

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

`transform_world_lifetime` covers installation, complete reset/clear/destruction,
callback order and deduplication, simultaneous worlds, repeated scene cycles,
hierarchy-removal invalidation during teardown, lazy state creation and placement
construction at a reused address. It explicitly preserves duplicate controller
registration. `transform_preview_lifetime` covers both preview implementations,
including repeated `destroy`/`ensure` cycles and destructor-only abandonment.
