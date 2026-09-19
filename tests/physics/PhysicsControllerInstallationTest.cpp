#include "PhysicsSceneFeature.h"
#include "PhysicsControllerContext.h"
#include <utility>

class Scene : public GtsScene
{
    public:
    void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData*) override
    {
        gts::physics::installPhysicsFeature(*this, ctx);
    }
    void onUpdateSimulation(const EcsSimulationContext&) override {}
};

int main()
{
    Scene scene;
    {
        EcsControllerContext load{scene.getWorld()};
        if (gts::physics::controllerContext(std::as_const(load)).physics)
            return 1;
        scene.onLoad(load, nullptr);
        auto* physics = gts::physics::controllerContext(std::as_const(load)).physics;
        if (!physics || physics != scene.getPhysicsModule())
            return 1;
        gts::physics::installPhysicsFeature(scene, load);
        if (gts::physics::controllerContext(std::as_const(load)).physics != physics)
            return 1;
        scene.unload(load);
    }
    EcsControllerContext fresh{scene.getWorld()};
    if (scene.getPhysicsModule() || gts::physics::controllerContext(std::as_const(fresh)).physics)
        return 1;
    scene.onLoad(fresh, nullptr);
    if (!gts::physics::controllerContext(std::as_const(fresh)).physics)
        return 1;
    scene.unload(fresh);
}
