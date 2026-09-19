#include "AnimationSceneFeature.h"
#include "EcsControllerContext.hpp"
#include "TransformSceneFeature.h"
#include "WorldTransformComponent.h"

#include <cmath>
#include <stdexcept>

#if __has_include("BuiltinExecutionGroups.h") || __has_include("SceneExecutionPolicy.h") ||                            \
                                                               __has_include("VNExecutionProfiles.h")
#error "Capability tests must not inherit runtime execution policy"
#endif

#ifdef TEST_RENDERING_INPUTS
#include "RendererSceneFeature.h"
#include "VNSystem.hpp"
#include "../rendering/material/TestMaterialResources.h"
#endif
#ifdef TEST_PHYSICS_INPUTS
#include "PhysicsSceneFeature.h"
#endif

namespace
{
    constexpr auto preparation = static_cast<EcsSystemGroup>(1ull << 32);
    constexpr auto animation   = static_cast<EcsSystemGroup>(1ull << 33);
    constexpr auto camera      = static_cast<EcsSystemGroup>(1ull << 34);
    constexpr auto particles   = static_cast<EcsSystemGroup>(1ull << 35);
    constexpr auto simulation  = static_cast<EcsSystemGroup>(1ull << 36);
    struct Metadata
    {
        int value;
    };

    void require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    class Scene final : public GtsScene
    {
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override {}
        void onUpdateSimulation(const EcsSimulationContext&) override {}
    };
} // namespace

int main()
{
    const EcsExecutionSelection defaults{
        "module-default", preparation | animation | camera | particles | simulation, Metadata{123}};
    ECSWorld world;
    require(!world.hasConfiguredDefaultExecutionSelection() && world.getCurrentExecutionSelection().id.empty(),
            "Bare module world remains neutral");
    gts::transform::installTransformFeature(world, defaults, preparation);
    gts::animation::installAnimationFeature(world, {"ignored", 0}, animation);
    const auto entity = world.createEntity();
    world.addComponent(entity, TransformComponent{});
    TransformAnimationComponent motion;
    motion.enableMode(TransformAnimationMode::Rotate);
    motion.rotationSpeed = 1.0f;
    world.addComponent(entity, motion);
    world.updateSimulation({world, 0.5f});
    require(std::abs(world.getComponent<TransformComponent>(entity).rotation.y - 0.5f) < 0.001f,
            "Animation must execute under the injected identity");
    world.updateControllers(EcsControllerContext{world});
    require(world.hasComponent<WorldTransformComponent>(entity), "Injected resolver must publish transforms");
    require(world.getLastControllerTimingSamples().size() == 1 &&
                world.getLastControllerTimingSamples()[0].group == preparation,
            "Resolver timing must retain the injected identity");
    world.pushExecutionSelection({"blocked", 0});
    world.updateSimulation({world, 0.5f});
    world.updateControllers(EcsControllerContext{world});
    require(world.getLastControllerTimingSamples().empty() &&
                std::abs(world.getComponent<TransformComponent>(entity).rotation.y - 0.5f) < 0.001f,
            "Injected groups remain subject to ordinary filtering");
    world.clear();
    require(world.getCurrentExecutionSelection().id == "module-default" &&
                world.getCurrentExecutionSelection().policy<Metadata>()->value == 123,
            "Default identity and opaque metadata must survive clear together");

#ifdef TEST_PHYSICS_INPUTS
    Scene                physicsScene;
    EcsControllerContext physicsContext{physicsScene.getWorld()};
    gts::physics::installPhysicsFeature(physicsScene, physicsContext, defaults, simulation);
    gts::physics::installPhysicsFeature(physicsScene, physicsContext, {"ignored", 0}, preparation);
    require(physicsScene.getWorld().getCurrentExecutionSelection().id == defaults.id &&
                physicsScene.getWorld().getSimulationSystemCount() == 1 &&
                physicsScene.getWorld().getControllerSystemCount() == 0,
            "Physics must forward supplied defaults without scheduling a resolver or duplicating installation");
    physicsScene.getWorld().updateSimulation({physicsScene.getWorld(), 0.01f});
#endif

#ifdef TEST_RENDERING_INPUTS
    Resources            resources;
    Scene                renderingScene;
    EcsControllerContext renderingContext{renderingScene.getWorld()};
    gts::rendering::controllerContext(renderingContext).resources = &resources;
    const gts::rendering::RendererExecutionInputs renderer{defaults, preparation, animation, camera, particles};
    gts::rendering::installRendererFeature(renderingScene, renderingContext, renderer);
    gts::rendering::installRendererFeature(renderingScene, renderingContext, renderer);
    renderingScene.getWorld().updateControllers(renderingContext);
    const auto& timings = renderingScene.getWorld().getLastControllerTimingSamples();
    require(timings.size() == 17, "Renderer installation count/order changed");
    for (size_t i = 0; i < timings.size(); ++i)
    {
        const auto expected = i < 9 ? preparation : i == 9 ? animation : i < 15 ? camera : particles;
        require(timings[i].group == expected && timings[i].instanceIndex == 0,
                "Renderer registration must use injected identities in the existing order");
    }
    renderingScene.unload(renderingContext);

    ECSWorld             narrativeWorld;
    UiSystem             ui(nullptr);
    EcsControllerContext narrativeContext{narrativeWorld};
    gts::ui::controllerContext(narrativeContext).ui = &ui;
    auto& session      = narrativeWorld.createSingleton<gts::vn::InteractionFrontendSessionComponent>();
    session.active     = true;
    session.mode       = gts::vn::InteractionFrontendMode::MerchantTrade;
    auto& presentation = narrativeWorld.createSingleton<gts::vn::VNExternalPresentationComponent>();
    gts::vn::VNExecutionInputs narrative{defaults,
                                         {"custom-overlay", toMask(camera), Metadata{456}},
                                         {"custom-fullscreen", toMask(particles), Metadata{789}}};
    gts::vn::VNSystem          vn(narrative);
    narrative.overlay.id = "changed-after-construction";
    vn.update(narrativeContext);
    require(narrativeWorld.getCurrentExecutionSelection().id == "custom-overlay" &&
                narrativeWorld.getCurrentExecutionSelection().policy<Metadata>()->value == 456,
            "VN must own and push supplied selections without interpreting their payload");
    presentation.active                 = true;
    presentation.suppressSceneRendering = true;
    presentation.markDirty();
    vn.update(narrativeContext);
    require(narrativeWorld.getCurrentExecutionSelection().id == "custom-fullscreen" &&
                narrativeWorld.getCurrentExecutionSelection().enabledSystems == toMask(particles) &&
                narrativeWorld.getExecutionSelectionDepth() == 2,
            "VN must replace its selection using the supplied fullscreen value");
    session.active = false;
    vn.update(narrativeContext);
    require(narrativeWorld.getCurrentExecutionSelection().id == defaults.id, "VN must restore the injected default");
#endif
}
