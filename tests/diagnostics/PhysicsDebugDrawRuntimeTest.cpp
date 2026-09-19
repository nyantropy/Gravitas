#include <cstdlib>
#include <iostream>

#include "DebugDrawRenderableComponent.h"
#include "DebugDrawSystem.hpp"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "PhysicsDebugRenderer.h"
#include "PhysicsWorld.h"
#include "SphereColliderComponent.h"
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
    ECSWorld                        world;
    EcsControllerContext            context{world};
    gts::debugdraw::DebugDrawSystem system;
    const Entity                    collider = world.createEntity();
    world.addComponent(collider, WorldTransformComponent{});
    world.addComponent(collider, SphereColliderComponent{});
    PhysicsWorld physics(&world);
    context.physics = &physics;
    PhysicsDebugRenderer producer;
    producer.update(context);
    require(physics.getProfileStats().debugSegmentCount == 36, "sphere diagnostic segment count changed");
    system.update(context);
    const auto entities = world.getAllEntitiesWith<gts::debugdraw::DebugDrawRenderableComponent>();
    require(entities.size() == 1, "physics diagnostics did not use one color batch");
    require(world.getComponent<DynamicMeshComponent>(entities.front()).vertices.size() == 36 * 8,
            "physics diagnostics generated the wrong geometry");
}
