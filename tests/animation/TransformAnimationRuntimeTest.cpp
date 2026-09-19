#include "SceneExecutionPolicy.h"
#include <cmath>
#include <cstdio>

#include "AnimationSceneFeature.h"
#include "EcsControllerContext.hpp"
#include "TransformAnimationComponent.h"
#include "TransformHierarchyHelpers.h"
#include "TransformSceneFeature.h"
#include "WorldTransformComponent.h"

namespace
{
    bool require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
        }
        return condition;
    }

    bool near(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return glm::length(lhs - rhs) < 0.0001f;
    }

    void tick(ECSWorld& world, float dt)
    {
        world.updateSimulation(EcsSimulationContext{world, dt});
    }

    Entity
    makeAnimated(ECSWorld& world, const TransformComponent& transform, const TransformAnimationComponent& animation)
    {
        const Entity entity = world.createEntity();
        world.addComponent(entity, transform);
        world.addComponent(entity, animation);
        return entity;
    }

    bool modesUseInitialLocalTransform()
    {
        ECSWorld world;
        gts::animation::installAnimationFeature(
            world, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        TransformComponent initial;
        initial.position = {2.0f, 3.0f, 4.0f};
        initial.rotation = {0.1f, 0.2f, 0.3f};
        initial.scale    = {2.0f, 3.0f, 4.0f};
        TransformAnimationComponent animation;
        animation.enableMode(TransformAnimationMode::Translate);
        animation.enableMode(TransformAnimationMode::Rotate);
        animation.enableMode(TransformAnimationMode::Scale);
        animation.translationAxis      = {0.0f, 1.0f, 0.0f};
        animation.translationAmplitude = 2.0f;
        animation.translationSpeed     = glm::half_pi<float>();
        animation.rotationEulerFactors = {0.5f, 1.0f, 0.2f};
        animation.rotationSpeed        = 2.0f;
        animation.scaleAmplitude       = {0.1f, 0.2f, 0.3f};
        animation.scaleSpeed           = glm::half_pi<float>();
        const Entity entity            = makeAnimated(world, initial, animation);

        tick(world, 0.0f);
        auto& transform = world.getComponent<TransformComponent>(entity);
        bool  ok        = require(near(transform.scale, initial.scale), "zero time retains initial scale") &&
                          require(transform.version == initial.version, "unchanged pose does not mark dirty");
        tick(world, 1.0f);
        ok &= require(near(transform.position, {2.0f, 5.0f, 4.0f}), "translation reaches positive peak") &&
              require(near(transform.rotation, {1.1f, 2.2f, 0.7f}), "rotation uses Euler factors") &&
              require(near(transform.scale, {2.1f, 3.2f, 4.3f}), "scale reaches positive peak") &&
              require(transform.version == initial.version + 1, "combined modes mark dirty once");

        auto& runtime = world.getComponent<TransformAnimationComponent>(entity);
        runtime.disableMode(TransformAnimationMode::Rotate);
        tick(world, 2.0f);
        ok &= require(near(transform.position, {2.0f, 1.0f, 4.0f}), "translation reaches negative peak") &&
              require(near(transform.scale, {1.9f, 2.8f, 3.7f}), "scale oscillates around initial scale") &&
              require(near(transform.rotation, {1.1f, 2.2f, 0.7f}), "disabled mode retains its last value");
        return ok;
    }

    bool disabledAndStationaryEntitiesDoNotInvalidate()
    {
        ECSWorld world;
        gts::animation::installAnimationFeature(
            world, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        TransformAnimationComponent animation;
        animation.enableMode(TransformAnimationMode::Translate);
        animation.enableMode(TransformAnimationMode::Rotate);
        animation.enableMode(TransformAnimationMode::Scale);
        animation.scaleAmplitude      = glm::vec3(0.5f);
        const Entity stationary       = makeAnimated(world, {}, animation);
        animation.enabled             = false;
        animation.rotationSpeed       = 1.0f;
        const Entity disabled         = makeAnimated(world, {}, animation);
        const Entity noModes          = makeAnimated(world, {}, {});
        const Entity missingTransform = world.createEntity();
        world.addComponent(missingTransform, animation);
        tick(world, 1.0f);
        const auto& disabledAnimation = world.getComponent<TransformAnimationComponent>(disabled);
        return require(world.getComponent<TransformComponent>(stationary).version == 1,
                       "zero speeds and amplitudes do not invalidate") &&
               require(world.getComponent<TransformComponent>(disabled).version == 1,
                       "disabled animation does not invalidate") &&
               require(!disabledAnimation.initialized && disabledAnimation.time == 0.0f,
                       "disabled animation does not initialize or advance") &&
               require(world.getComponent<TransformComponent>(noModes).version == 1,
                       "no active modes do not invalidate") &&
               require(!world.getComponent<TransformAnimationComponent>(missingTransform).initialized,
                       "animation without a transform is ignored");
    }

    class AnimationTestScene : public GtsScene
    {
        public:
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData* = nullptr) override {}
        void onUpdateSimulation(const EcsSimulationContext&) override {}
    };

    bool sceneInstallationAndPauseResume()
    {
        AnimationTestScene scene;
        gts::animation::installAnimationFeature(
            scene, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        gts::animation::installAnimationFeature(
            scene, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        auto&                       world = scene.getWorld();
        if (!require(world.getCurrentExecutionSelection().id == "gameplay" &&
                     world.getCurrentExecutionSelection().enabledSystems == 0xfff,
                     "Standalone animation installs the runtime default"))
            return false;
        TransformAnimationComponent animation;
        animation.enableMode(TransformAnimationMode::Rotate);
        animation.rotationSpeed = 1.0f;
        Entity entity           = makeAnimated(world, {}, animation);
        tick(world, 1.0f);
        bool ok = require(near(world.getComponent<TransformComponent>(entity).rotation, {0.0f, 1.0f, 0.0f}),
                          "repeated scene installation advances once");
        world.pushExecutionSelection(SceneExecutionProfile::pauseMenu());
        tick(world, 1.0f);
        ok &= require(world.getComponent<TransformAnimationComponent>(entity).time == 1.0f,
                      "pause profile masks the animation system");
        world.popExecutionSelection();
        tick(world, 1.0f);
        ok &= require(near(world.getComponent<TransformComponent>(entity).rotation, {0.0f, 2.0f, 0.0f}),
                      "resume continues from retained animation time");
        world.getComponent<TransformAnimationComponent>(entity).enabled = false;
        tick(world, 1.0f);
        world.getComponent<TransformAnimationComponent>(entity).enabled = true;
        tick(world, 1.0f);
        ok &= require(near(world.getComponent<TransformComponent>(entity).rotation, {0.0f, 3.0f, 0.0f}),
                      "component enable resumes without recapturing the initial transform");

        EcsControllerContext ctx{world};
        scene.unload(ctx);
        gts::animation::installAnimationFeature(
            scene, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        entity = makeAnimated(world, {}, animation);
        tick(world, 1.0f);
        ok &= require(world.getComponent<TransformAnimationComponent>(entity).time == 1.0f,
                      "scene unload allows installation into the new world lifetime");
        return ok;
    }

    bool animatedLocalTransformResolvesThroughParent()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(
            world, SceneExecutionProfile::gameplay(), gts::execution::groups::RenderPrep);
        gts::animation::installAnimationFeature(
            world, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        const Entity       parent = world.createEntity();
        TransformComponent parentTransform;
        parentTransform.position   = {10.0f, 0.0f, 0.0f};
        parentTransform.rotation.z = glm::half_pi<float>();
        world.addComponent(parent, parentTransform);
        TransformAnimationComponent animation;
        animation.enableMode(TransformAnimationMode::Translate);
        animation.translationAxis      = {1.0f, 0.0f, 0.0f};
        animation.translationAmplitude = 2.0f;
        animation.translationSpeed     = glm::half_pi<float>();
        const Entity child             = makeAnimated(world, {}, animation);
        if (!require(gts::transform::attachToParent(world, child, parent), "attach animated child"))
        {
            return false;
        }
        world.updateControllers(EcsControllerContext{world});
        tick(world, 1.0f);
        world.updateControllers(EcsControllerContext{world});
        return require(near(world.getComponent<TransformComponent>(child).position, {2.0f, 0.0f, 0.0f}),
                       "animation writes parent-local position") &&
               require(
                   near(glm::vec3(world.getComponent<WorldTransformComponent>(child).matrix[3]), {10.0f, 2.0f, 0.0f}),
                   "dirty publication resolves animation through parent rotation and translation");
    }
} // namespace

int main()
{
    bool ok = modesUseInitialLocalTransform();
    ok &= disabledAndStationaryEntitiesDoNotInvalidate();
    ok &= sceneInstallationAndPauseResume();
    ok &= animatedLocalTransformResolvesThroughParent();
    return ok ? 0 : 1;
}
