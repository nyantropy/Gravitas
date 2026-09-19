# Engine Physics Architecture

This document describes the current Gravitas physics module. It is intentionally
small: the engine currently provides sphere-collider collision detection and a
scene-local physics world, not a full rigid-body solver.

## Module Layout

- `modules/physics/components/PhysicsBodyComponent.h`: dynamic/static body
  descriptor.
- `modules/physics/components/SphereColliderComponent.h`: sphere collider
  descriptor.
- `modules/physics/contracts/IGtsPhysicsModule.h`: public physics access interface.
- `modules/physics/contracts/CollisionEvent.h`: collision pair event data.
- `modules/physics/contracts/ScenePhysics.h`: optional/required borrowed scene access.
- `modules/physics/core/PhysicsWorld.h/.cpp`: scene-local collision storage,
  profile counters, and `IGtsPhysicsModule` implementation.
- `modules/physics/systems/PhysicsSystem.h/.cpp`: fixed-step collision update.
- `modules/physics/scene/PhysicsSceneFeature.h`: scene feature installer.

Physics diagnostics live separately under `modules/diagnostics/physics/` and
emit debug-draw primitives instead of making physics depend on rendering.
The `gravitas_diagnostics_physics` bridge is built only when both
`GTS_ENABLE_PHYSICS` and `GTS_ENABLE_DEBUGDRAW` are enabled. Generic debug drawing
can be built independently of physics.
`gravitas_physics_contracts` is an always-available header-only target depending
only on `gravitas_core`. Collision/accessor consumers link it without inheriting
physics implementation or transform. `gravitas_physics` consumes this contract
target; core does not. `PhysicsControllerContext.h` exposes optional borrowed
access via `gts::physics::controllerContext(ctx).physics`. Core's controller context
does not know about physics. Scene access is also physics-owned through
`gts::physics::findScenePhysics(scene)` and `requireScenePhysics(scene)` in
`ScenePhysics.h`. Both have const scene overloads returning const access.
Find returns null when uninstalled; require throws `std::logic_error`. Neither
installs physics or extends its lifetime.
`gravitas_physics` links `gravitas_transform` for transform contracts and
resolution; transform include directories are owned by that target. The
[transform architecture](../transform/architecture.md) describes its boundary.

## Scene Installation

Scenes install physics through `gts::physics::installPhysicsFeature(scene, ctx, defaultSelection, simulationGroup)`.
The installer:

1. Marks the scene feature as installed.
2. Installs transform lifetime ownership and dirty-tracking callbacks, without
   scheduling a transform controller. Physics resolves explicitly before queries;
   renderer installation places the presentation resolver after transform writers.
3. Creates a scene resource `PhysicsWorld`.
4. Creates a scene-owned `detail::ScenePhysicsBinding` borrowing that implementation
   and populates the call's physics-owned controller context. Later engine calls
   use `findScenePhysics` to obtain the same borrowed implementation pointer.
5. Registers `PhysicsSystem` in the supplied simulation group. Gravitas supplies
   its runtime-owned Physics identity; the module does not know that catalog.

The supplied default is forwarded to transform installation, preserving its
first-configured-default-wins behavior and original installation position.

The physics world is scene-local and is destroyed with the scene or on scene reset.
The binding owns no implementation and its destructor never dereferences the
borrowed object. Both are released by the existing generic scene-resource lifecycle;
there is no physics reset registry or physics state in `GtsScene`. Raw world clear
does not reset scene resources. Repeated installation remains idempotent until
scene reset clears installation bookkeeping. Context pointers must not outlive the
call/scene resource they borrow. Diagnostics continues reading the physics-owned
controller payload, without requiring scene access or changed scheduling.

## Data Model

An entity participates in physics when it has:

- `WorldTransformComponent`
- `PhysicsBodyComponent`
- `SphereColliderComponent`

`PhysicsBodyComponent::dynamic` controls broad collision policy. Static/static
pairs are skipped; any pair with at least one dynamic body can be tested.

Sphere collider position is taken from `WorldTransformComponent`. The current
physics module does not own transform integration, rigid-body velocity, mass,
forces, impulses, restitution, friction, penetration resolution, or character
controller behavior.

## Simulation Flow

`PhysicsSystem` runs in fixed simulation space. Each update:

1. Sets the current `ECSWorld` on `PhysicsWorld`.
2. Calls `PhysicsWorld::update(ctx.dt)` to clear/update world state.
3. Resolves world transforms through the transform resolver.
4. Collects entities with world transform, body, and sphere collider.
5. Runs a simple broad phase using per-axis radius overlap.
6. Runs sphere/sphere narrow phase for candidate pairs.
7. Records collisions into `PhysicsWorld`.
8. Updates profile counters.

Consumers read collision pairs through `IGtsPhysicsModule::getCollisions()`.

## Diagnostics And Profiling

`PhysicsWorld::ProfileStats` tracks frame/update counters, entity/collider
counts, broad-phase checks/candidates, collisions, debug collider/segment
counts, and timing for collection, broad phase, narrow phase, and debug render.

Physics debug rendering is a diagnostic bridge. It reads colliders and emits
debug-draw primitives; physics itself remains independent of rendering.

## Current Limitations

- Sphere colliders only.
- Collision detection only; no collision response solver.
- No broad-phase spatial partitioning.
- No rigid-body velocity, forces, mass, constraints, or shapes beyond spheres.
- No trigger/filter layer API beyond game-side interpretation of collision
  pairs.

These limitations are current state, not hidden design promises.
