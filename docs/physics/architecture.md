# Engine Physics Architecture

This document describes the current Gravitas physics module. It is intentionally
small: the engine currently provides sphere-collider collision detection and a
scene-local physics world, not a full rigid-body solver.

## Module Layout

- `modules/physics/components/PhysicsBodyComponent.h`: dynamic/static body
  descriptor.
- `modules/physics/components/SphereColliderComponent.h`: sphere collider
  descriptor.
- `modules/physics/core/CollisionEvent.h`: collision pair event data.
- `modules/physics/core/PhysicsWorld.h/.cpp`: scene-local collision storage,
  profile counters, and `IGtsPhysicsModule` implementation.
- `modules/physics/systems/PhysicsSystem.h/.cpp`: fixed-step collision update.
- `modules/physics/scene/PhysicsSceneFeature.h`: scene feature installer.

Physics diagnostics live separately under `modules/diagnostics/physics/` and
emit debug-draw primitives instead of making physics depend on rendering.

## Scene Installation

Scenes install physics through `gts::physics::installPhysicsFeature(scene, ctx)`.
The installer:

1. Marks the scene feature as installed.
2. Installs transform runtime support.
3. Creates a scene resource `PhysicsWorld`.
4. Exposes it through `scene.setPhysicsModule(...)` and `ctx.physics`.
5. Registers `PhysicsSystem` as an `EcsSystemGroup::Physics` simulation system.

The physics world is scene-local and is destroyed with the scene.

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
