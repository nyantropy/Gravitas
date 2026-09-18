# Runtime model instances

`modules/model/runtime` owns `gravitas_model_instances`. A `GtsModelInstance`
represents one mutable occurrence of a loaded/realized model in a world. This is
an engine runtime module downstream of CPU assets, material frontend realization,
and skeletal animation. It contains no importer, serialization, cooking or backend
calls. Its tests compile with rendering and Vulkan disabled.

## Terms and authoritative ownership

- **Resource**: what the model is — immutable `GtsModelResource`.
- **Realization**: prepared shared geometry and model associations — `GtsRealizedModel`.
- **Instance**: one mutable world occurrence — `GtsModelInstance`.
- **Presentation**: derived renderer-facing view/snapshot, currently temporary.
- **GPU resource**: backend realization, never authoritative animation state.

```text
ENGINE
├── GtsModelRegistry
│    └── GtsModelResource
└── GtsModelRealizationCache
     └── GtsRealizedModel

WORLD
├── MaterialRuntime
├── GtsModelMaterialRealization
│    └── GtsRealizedModelMaterials
├── GtsModelInstanceRuntime (creation facade; owns no instances)
└── ECS entities
     └── GtsModelInstance
          ├── model handle
          ├── shared realized-model reference
          ├── shared material association reference (runtime lifetime remains weak)
          └── skeleton occurrences
               ├── playback
               ├── pose
               └── CPU binding palettes

RENDERER / BACKEND
└── derived frame/GPU state
     ├── GPU mesh resources
     └── GPU palette buffers
```

Instances keep definitions/geometry alive even if the registry/cache is destroyed.
They do not own the registry, cache, world, MaterialRuntime or renderer. Shared
services/definitions do not refer back to instances. There are no ownership cycles
or mutable-instance registries.

## Creation and ECS ownership

Configure the world facade once with the existing engine geometry cache and
resource provider:

```cpp
auto& instances = modelInstances(world, geometryCache, resources);
auto result = instances.create(modelHandle);
// Subsequent entity creation can use modelInstances(world).create(modelHandle).
```

The facade calls the existing geometry cache and current world material service,
validates matching resource/realization/material identity, initializes default
poses and palettes, and returns a `unique_ptr<GtsModelInstance>` only on complete
success. Lower-layer warnings/errors survive in the creation result. The game
selects the model through `GtsModelRegistry`; it does not orchestrate geometry or
material realization. The cache/provider must outlive the configured world facade.
It is also directly constructible for a smaller explicitly scoped setup context.
Changing its cache/provider requires world teardown/reconfiguration, not implicit
rebinding of existing instances.

The current ECS requires copyable components: `addComponent` accepts a const
reference and archetype relocation copies component values. Consequently Yune's
entity component uses `shared_ptr<GtsModelInstance>` to retain a stable address
through those storage copies. This is a concrete ECS storage constraint, not a
shared animation model: every spawn consumes a fresh creation result. Copies of
an owner component refer to that same occurrence and must not be used to spawn
another independently animated entity. Once external setup temporaries are gone,
entity destruction releases that occurrence. `GtsModelInstance` itself is
non-copyable/non-movable; callers cannot accidentally copy its playback state.
There is no `shared_ptr<GtsSkeletonPose>`.

## Definition and occurrence views

`model()`, `geometry()` and `materials()` expose retained immutable shared
references. Geometry occurrences already provide model-node index, geometry
profile/ranges, skin binding and skeleton-use indices. `materialFor(occurrence,
primitive)` resolves the world binding; `paletteForOccurrence(occurrence)` returns
null for static geometry and the current CPU palette for skinned geometry.
`palette(bindingIndex)` uses the explicit resource binding index.

`GtsModelResource::skeletonUses()` and `skinBindings()` expose existing shared
associations without requiring callers to inspect canonical backing. Current
prepared cooked resources are static and return empty spans. Future animated
cooked support can supply these same resource queries; this task adds no formats.

Static instances have an empty skeleton-occurrence vector and no dummy pose or
palettes. Every declared skeleton use in a skinned model receives one runtime
occurrence, even when two uses reference compatible definitions. Each occurrence
borrows its definition from the retained resource and owns `GtsAnimationPlayback`
by value. That existing type already stores a pose, so the occurrence uses its
initialized `pose` field as the sole current pose; no duplicate pose cache exists.
It also owns explicit `(skinBindingIndex, GtsSkinPalette)` values for its bindings.
Each model binding is assigned by its canonical skeleton-use association, never by
names or compatibility inference. Multiple bindings consume the same evaluated pose.

## Playback and update semantics

- `play(use, resourceScopedClip)` rejects foreign-model and structurally incompatible
  clips. Repeating the active clip is idempotent. A changed clip samples time zero
  and publishes its pose and all binding palettes together.
- `stop(use)` returns that occurrence to its default pose, clears clip/time and
  publishes default palettes. Playback speed/looping policy remains configured.
- `setPlaybackPolicy(use, speed, looping)` accepts finite, nonnegative speed.
- `updateAnimations(deltaSeconds)` accepts a supplied finite, nonnegative delta.
  It advances/evaluates each active occurrence exactly once and then builds that
  occurrence's binding palettes. Inactive default-pose occurrences need no evaluation.
- Looping, speed, non-looping endpoint and zero-duration semantics reuse
  `advanceGtsAnimationPlayback` unchanged. No wall clock is consulted.

Commands and updates return `GtsModelInstanceStatus` diagnostics. A changed-clip
command evaluates its initial pose immediately; an ensuing frame update advances
it normally. No per-frame repeated `play` evaluation happens for an unchanged clip.
An update stages *all* occurrence state. Time, pose and every binding palette are
committed only if every occurrence succeeds. Late pose/time/palette failures leave
the entire instance at its previous state. This deliberately simple transaction
copies current mutable state while staging; optimization is deferred.

## Stable pose-occurrence references

`skeletonOccurrence(use)` returns `GtsSkeletonOccurrenceReference`. Its identity
is the stable runtime occurrence, and its weak owner token makes `get()` return
null after instance destruction. Updates commit into the existing occurrence
objects without replacing their addresses. The reference exposes only a const
occurrence/pose view and does not extend mutable-state or definition lifetime.
Consumers must reacquire nested pose/palette views after updates and may not retain
raw pointers beyond owner destruction. Access is synchronous/main-thread.

This is the future seam for additional bindings consuming one body-owned pose.
No cross-model pose binding, clothing ownership, alternate pose driver or shared
playback clock is implemented here.

## Material reset and transform ownership

`worldMaterialsValid()` distinguishes world material validity from definition
validity. Material lookup returns an invalid handle after runtime reset/destruction.
Animation may continue independently because its model definitions remain valid.
`rebindMaterials(service)` obtains a matching new material set without reimporting
or preparing geometry and without resetting playback/pose/palettes. It does not
silently refresh old renderer presentation snapshots.

World placement remains exclusively in the entity's `TransformComponent` and the
existing renderer placement fields. Authored hierarchy remains in `GtsModelResource`.
Neither is copied into the instance or baked into geometry/palettes. Later static
extraction will compose entity and model hierarchy transforms. Skinned palettes
retain `pose.modelTransforms[node] * inverseBind`, with no world or mesh-node
post-transform added. Existing Yune anchor/facing conventions remain unchanged.

## Yune and the temporary renderer adapter

`YuneCharacterAsset` has been removed/renamed to `YuneModelConfiguration`. It now
contains the content handle, Idle/SlowWalk scoped references and temporary
world-scoped presentation setup. It no longer owns shared realized geometry,
material sets or animation state. A weak material-set identity detects reuse of
presentation snapshots in the wrong/expired world scope.

Setup creates a short-lived generic instance to derive the existing shared
by-value presentation, then each entity creates its own authoritative instance.
This setup-only extra default-pose initialization remains temporary. Game code
chooses `moving ? slowWalk : idle`, calls `play` and `updateAnimations`, and obtains
an immutable palette snapshot through `GtsModelInstancePresentation`. All generic
pose/palette evaluation lives in the model runtime. The snapshot adapter orders
palettes by explicit binding index for the unchanged renderer contract.

The existing rendering presentation, extraction, GPU ownership, material snapshots,
static/skinned pipelines and update timing remain unchanged. Expired materials
cause the snapshot adapter to fail explicitly rather than hide invalid handles.
The next renderer-extraction task should eliminate setup geometry copies and
snapshot authority, including defining refresh after material rebinding.

## Verification

`GtsModelInstanceTest` is a headless CPU/frontend test covering canonical and cooked
static models, unanimated default-pose skinning, shared definition/geometry/material
lifetime, independent worlds/occurrences, registry/cache destruction, ECS relocation
and destruction, material reset/rebind, stable reference expiry, scoped clip
validation, switching/idempotence/loop/speed/zero duration, multiple uses/bindings,
and all-or-nothing failure on late time overflow and palette multiplication overflow.
Yune's real-source integration test now exercises generic instances while retaining
Idle/SlowWalk, geometry, material and world-placement regressions.
