#include "CollisionEvent.h"
#include "IGtsPhysicsModule.h"
#include "PhysicsControllerContext.h"
#include "ECSWorld.hpp"
#include <utility>

#include <type_traits>

#if __has_include("PhysicsWorld.h") || __has_include("TransformSystem.hpp")
#error "Physics contracts must not expose physics or transform implementation"
#endif

static_assert(std::is_same_v<entity_id_type, uint32_t>);
static_assert(sizeof(CollisionEvent) == 2 * sizeof(Entity));

class ContractPhysics final : public IGtsPhysicsModule
{
public:
    void update(float) override { collisions.push_back({Entity{1}, Entity{2}}); }
    const std::vector<CollisionEvent>& getCollisions() const override { return collisions; }
    void clearCollisions() override { collisions.clear(); }

private:
    std::vector<CollisionEvent> collisions;
};

class PhysicsController : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        if (auto* physics = gts::physics::controllerContext(ctx).physics)
            physics->update(0.1f);
    }
};

int main()
{
    ECSWorld world;
    EcsControllerContext context{world};
    world.addControllerSystem<PhysicsController>(EcsSystemGroup::Always);
    world.updateControllers(context);
    if (gts::physics::controllerContext(std::as_const(context)).physics != nullptr)
        return 1;
    ContractPhysics implementation;
    gts::physics::controllerContext(context).physics = &implementation;
    IGtsPhysicsModule& physics = *gts::physics::controllerContext(std::as_const(context)).physics;
    world.updateControllers(context);
    if (physics.getCollisions().size() != 1 || physics.getCollisions()[0].a != Entity{1} ||
        physics.getCollisions()[0].b != Entity{2})
        return 1;
    physics.clearCollisions();
    return physics.getCollisions().empty() ? 0 : 1;
}
