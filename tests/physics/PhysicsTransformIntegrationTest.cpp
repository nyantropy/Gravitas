#include <cstdlib>
#include <iostream>

#include "ECSWorld.hpp"
#include "PhysicsBodyComponent.h"
#include "PhysicsSystem.h"
#include "PhysicsWorld.h"
#include "SphereColliderComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformSceneFeature.h"
#include "WorldTransformComponent.h"

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << std::endl;
            std::exit(1);
        }
    }
} // namespace

int main()
{
    ECSWorld world;
    gts::transform::installTransformRuntime(world);
    PhysicsWorld         physics(&world);
    PhysicsSystem        system(&physics);
    EcsSimulationContext context{world, 1.0f / 60.0f};

    const Entity       parent = world.createEntity();
    TransformComponent placement;
    placement.position.x = 10.0f;
    world.addComponent(parent, placement);
    const Entity child = world.createEntity();
    world.addComponent(child, TransformComponent{});
    world.addComponent(child, PhysicsBodyComponent{});
    world.addComponent(child, SphereColliderComponent{});
    require(gts::transform::attachToParent(world, child, parent), "parenting failed");
    const Entity other = world.createEntity();
    world.addComponent(other, placement);
    world.addComponent(other, PhysicsBodyComponent{});
    world.addComponent(other, SphereColliderComponent{});

    system.update(context);
    require(physics.getCollisions().size() == 1, "physics did not resolve hierarchical transforms");
    require(world.getComponent<WorldTransformComponent>(child).matrix[3].x == 10.0f,
            "physics published the wrong world transform");

    world.getComponent<TransformComponent>(parent).position.x = 20.0f;
    gts::transform::markDirty(world, parent);
    system.update(context);
    require(physics.getCollisions().empty(), "physics used stale child placement");
    require(world.getComponent<WorldTransformComponent>(child).matrix[3].x == 20.0f,
            "dirty parent did not update child placement");
    gts::transform::resetTransformSceneFeature(world);
}
