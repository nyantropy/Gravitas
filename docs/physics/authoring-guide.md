# Physics Authoring Guide

Use this guide when adding physics participation to a scene or feature.
Architecture details live in [architecture.md](architecture.md).

## Installing Physics

Install the physics feature from scene setup:

```cpp
gts::physics::installPhysicsFeature(*this, ctx, defaultSelection, simulationGroup);
```

This creates the scene-local `PhysicsWorld`, exposes
`gts::physics::controllerContext(ctx).physics` through `PhysicsControllerContext.h`, and
registers `PhysicsSystem` in fixed simulation space.

Pass the borrowed physics module from the controller setup context into a
simulation consumer, then register that consumer after the physics installer.
`EcsSimulationContext` does not carry the controller payload. Read the collision
list during simulation after `PhysicsSystem` updates it.

## Adding A Collider

Add these components to an entity:

- `TransformComponent`
- `PhysicsBodyComponent`
- `SphereColliderComponent`

The transform system resolves `WorldTransformComponent`; physics reads world
position from that resolved transform.

Use `PhysicsBodyComponent::dynamic = true` for moving/gameplay-active bodies.
Static/static pairs are skipped.

## Reading Collisions

Simulation systems can read collisions through `IGtsPhysicsModule`:

```cpp
const auto& collisions = physicsModule->getCollisions();
```

The game or feature system decides what a collision means. The physics module
only records entity pairs.

## Diagnostics

Install physics diagnostics separately when you need collider visualization.
Diagnostics emit shared debug-draw primitives and should not become gameplay
dependencies.

## Do Not

- Do not put gameplay rules inside `PhysicsSystem`.
- Do not make physics depend on rendering.
- Do not read or write renderer GPU components from physics.
- Do not assume collision response exists; consumers must handle their own
  gameplay effects.
- Do not use frame-local controller time for authoritative collision logic.

`defaultSelection` and `simulationGroup` are explicit composition inputs. Gravitas
application code supplies `SceneExecutionProfile::gameplay()` and
`gts::execution::groups::Physics` from `gravitas_runtime_execution`; physics itself
consumes only core execution values.
