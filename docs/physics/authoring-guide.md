# Physics Authoring Guide

Use this guide when adding physics participation to a scene or feature.
Architecture details live in [architecture.md](architecture.md).

## Installing Physics

Install the physics feature from scene setup:

```cpp
gts::physics::installPhysicsFeature(*this, ctx);
```

This creates the scene-local `PhysicsWorld`, exposes `ctx.physics`, and
registers `PhysicsSystem` in fixed simulation space.

Collision consumers that read `ctx.physics->getCollisions()` should run after
the physics feature's simulation system has been installed.

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
