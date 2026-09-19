#include "SceneExecutionPolicy.h"
#include "PhysicsSceneFeature.h"
#include "PhysicsControllerContext.h"
#include "ScenePhysics.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

class Scene : public GtsScene
{
    public:
    void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData*) override
    {
        gts::physics::installPhysicsFeature(
            *this, ctx, SceneExecutionProfile::gameplay(), gts::execution::groups::Physics);
    }
    void onUpdateSimulation(const EcsSimulationContext&) override {}
};

static_assert(std::is_same_v<decltype(gts::physics::findScenePhysics(std::declval<Scene&>())), IGtsPhysicsModule*>);
static_assert(
    std::is_same_v<decltype(gts::physics::findScenePhysics(std::declval<const Scene&>())), const IGtsPhysicsModule*>);
static_assert(std::is_same_v<decltype(gts::physics::requireScenePhysics(std::declval<Scene&>())), IGtsPhysicsModule&>);
static_assert(std::is_same_v<decltype(gts::physics::requireScenePhysics(std::declval<const Scene&>())),
                             const IGtsPhysicsModule&>);

void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

int main()
{
    Scene scene;
    require(!gts::physics::findScenePhysics(scene), "Uninstalled scene must have no physics");
    for (bool constant : {false, true})
    {
        bool failed = false;
        try
        {
            if (constant)
                gts::physics::requireScenePhysics(std::as_const(scene));
            else
                gts::physics::requireScenePhysics(scene);
        }
        catch (const std::logic_error&)
        {
            failed = true;
        }
        require(failed, "Missing required physics must fail explicitly");
    }

    Scene                other;
    EcsControllerContext otherLoad{other.getWorld()};
    other.onLoad(otherLoad, nullptr);
    auto* otherPhysics = gts::physics::findScenePhysics(other);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        EcsControllerContext load{scene.getWorld()};
        require(!gts::physics::controllerContext(std::as_const(load)).physics, "Fresh context must remain optional");
        scene.onLoad(load, nullptr);
        auto* physics = gts::physics::controllerContext(std::as_const(load)).physics;
        require(physics && physics == gts::physics::findScenePhysics(scene), "Installation must publish scene physics");
        require(physics == scene.findSceneResource<PhysicsWorld>(), "Scene must own the implementation");
        require(physics == &gts::physics::requireScenePhysics(scene), "Required access must return same object");
        require(physics == gts::physics::findScenePhysics(std::as_const(scene)),
                "Const lookup must return same object");
        require(physics == &gts::physics::requireScenePhysics(std::as_const(scene)), "Const required access");
        require(physics != otherPhysics, "Worlds must remain isolated");
        gts::physics::installPhysicsFeature(
            scene, load, SceneExecutionProfile::gameplay(), gts::execution::groups::Physics);
        require(gts::physics::controllerContext(std::as_const(load)).physics == physics &&
                    scene.getWorld().getSimulationSystemCount() == 1,
                "Repeated installation must remain idempotent");
        scene.unload(load);
        // load's pointers are borrowed for the call, not retained across unload.
        require(!scene.findSceneResource<PhysicsWorld>() && !gts::physics::findScenePhysics(scene),
                "Reset must remove both implementation and access binding");
        require(gts::physics::findScenePhysics(other) == otherPhysics, "Reset must not affect another scene");
    }
    {
        Scene                temporary;
        EcsControllerContext load{temporary.getWorld()};
        temporary.onLoad(load, nullptr);
    }
    require(gts::physics::findScenePhysics(other) == otherPhysics, "Direct destruction must not affect another scene");
    other.unload(otherLoad);
}
