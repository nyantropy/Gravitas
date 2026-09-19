#include <cstdlib>
#include <iostream>

#include "DebugDrawPrimitives.h"
#include "DebugDrawRenderableComponent.h"
#include "DebugDrawSystem.hpp"
#include "DynamicMeshComponent.h"
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
    auto                            submit = [&]()
    {
        gts::debugdraw::line(world, {0, 0, 0}, {1, 0, 0}, gts::debugdraw::DebugDrawColor::Red, 0.1f);
    };
    submit();
    system.update(context);
    auto entities = world.getAllEntitiesWith<gts::debugdraw::DebugDrawRenderableComponent>();
    require(entities.size() == 1, "line did not produce one batch");
    const Entity batch = entities.front();
    const auto&  mesh  = world.getComponent<DynamicMeshComponent>(batch);
    require(mesh.vertices.size() == 8 && mesh.indices.size() == 36, "line box geometry changed");
    require(mesh.vertices.front().pos == glm::vec3(0, -0.05f, -0.05f), "line box orientation changed");
    require(mesh.vertices.front().color == glm::vec4(1.0f, 0.02f, 0.02f, 1.0f), "line color changed");
    const auto version = mesh.geometryVersion;
    require(gts::debugdraw::ensureQueue(world).empty(), "consumed queue was not cleared");

    submit();
    system.update(context);
    require(world.getComponent<DynamicMeshComponent>(batch).geometryVersion == version,
            "identical batch regenerated geometry");
    gts::debugdraw::line(world, {0, 0, 0}, {2, 0, 0}, gts::debugdraw::DebugDrawColor::Red, 0.1f);
    system.update(context);
    require(world.getComponent<DynamicMeshComponent>(batch).geometryVersion == version + 1,
            "changed batch did not regenerate geometry");
    system.update(context);
    require(!world.hasComponent<gts::debugdraw::DebugDrawRenderableComponent>(batch),
            "empty queue left its old renderable alive");

    const Entity collider = world.createEntity();
    world.addComponent(collider, WorldTransformComponent{});
    world.addComponent(collider, SphereColliderComponent{});
    PhysicsWorld physics(&world);
    context.physics = &physics;
    PhysicsDebugRenderer producer;
    producer.update(context);
    require(physics.getProfileStats().debugSegmentCount == 36, "sphere diagnostic segment count changed");
    system.update(context);
    entities = world.getAllEntitiesWith<gts::debugdraw::DebugDrawRenderableComponent>();
    require(entities.size() == 1, "physics diagnostics did not use one color batch");
    require(world.getComponent<DynamicMeshComponent>(entities.front()).vertices.size() == 36 * 8,
            "physics diagnostics generated the wrong geometry");
}
