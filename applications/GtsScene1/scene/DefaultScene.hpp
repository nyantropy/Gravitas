#include "SceneExecutionPolicy.h"
#pragma once

#include <string>

#include "ECSWorld.hpp"
#include "GtsScene.hpp"

#include "StaticMeshComponent.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "CameraDescriptionComponent.h"
#include "RendererSceneFeature.h"
#include "TransformComponent.h"
#include "TransformAnimationComponent.h"

#include "AnimationSceneFeature.h"

#include "GraphicsConstants.h"

// a run of the mill default scene in the engine, for testing purposes
class DefaultScene : public GtsScene
{
    private:
        Entity controlledCube;
        MaterialInstanceHandle materialForTexture(const std::string& texturePath)
        {
            auto& materials = gts::rendering::materialRuntime(ecsWorld);
            MaterialInstance instance;
            if (const MaterialInstance* defaultMaterial = materials.getInstance(materials.defaultMaterial()))
                instance.definition = defaultMaterial->definition;
            instance.baseColorTexture = MaterialTextureBinding::assetPath(texturePath);
            instance.renderState.blendMode = MaterialBlendMode::Alpha;
            instance.renderState.alphaMode =
                alphaModeForBlendMode(MaterialBlendMode::Alpha, instance.baseColor.a, true);
            return materials.createInstance(instance);
        }

        void addMaterialReference(Entity entity, const std::string& texturePath)
        {
            ecsWorld.addComponent<MaterialReferenceComponent>(
                entity,
                MaterialReferenceComponent{materialForTexture(texturePath)});
        }

    public:
        void firstCube()
        {
            controlledCube = ecsWorld.createEntity();

            StaticMeshComponent mesh;
            mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.gmesh";
            ecsWorld.addComponent<StaticMeshComponent>(controlledCube, mesh);

            addMaterialReference(controlledCube,
                                 GraphicsConstants::ENGINE_RESOURCES + "/textures/engine_demo_moss_floor.png");

            TransformComponent tc;
            ecsWorld.addComponent<TransformComponent>(controlledCube, tc);

            TransformAnimationComponent anim;
            anim.enableMode(TransformAnimationMode::Rotate);
            anim.rotationEulerFactors = glm::vec3(0.5f, 1.0f, 0.2f);
            anim.rotationSpeed = glm::radians(90.0f);
            ecsWorld.addComponent<TransformAnimationComponent>(controlledCube, anim);
        }

        void secondCube()
        {
            Entity cube2 = ecsWorld.createEntity();

            StaticMeshComponent mesh;
            mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.gmesh";
            ecsWorld.addComponent<StaticMeshComponent>(cube2, mesh);

            addMaterialReference(cube2,
                                 GraphicsConstants::ENGINE_RESOURCES + "/textures/engine_demo_cool_stone.png");

            TransformComponent tc2;
            tc2.position = glm::vec3(2.0f, 2.0f, 2.0f);
            ecsWorld.addComponent<TransformComponent>(cube2, tc2);

            TransformAnimationComponent anim2;
            anim2.enableMode(TransformAnimationMode::Translate);
            anim2.enableMode(TransformAnimationMode::Rotate);
            anim2.translationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
            anim2.translationAmplitude = 1.0f;
            anim2.translationSpeed = 2.0f;
            anim2.rotationEulerFactors = glm::vec3(0.0f, 0.0f, 1.0f);
            anim2.rotationSpeed = glm::radians(30.0f);
            ecsWorld.addComponent<TransformAnimationComponent>(cube2, anim2);
        }

        void thirdCube()
        {
            Entity cube3 = ecsWorld.createEntity();

            StaticMeshComponent mesh;
            mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.gmesh";
            ecsWorld.addComponent<StaticMeshComponent>(cube3, mesh);

            addMaterialReference(cube3,
                                 GraphicsConstants::ENGINE_RESOURCES + "/textures/engine_demo_arcane_stone.png");

            TransformComponent tc3;
            tc3.position = glm::vec3(-2.0f, -2.0f, -2.0f);
            ecsWorld.addComponent<TransformComponent>(cube3, tc3);

            TransformAnimationComponent anim3;
            anim3.enableMode(TransformAnimationMode::Scale);
            anim3.scaleAmplitude = glm::vec3(0.5f, 0.5f, 0.5f);
            anim3.scaleSpeed = 2.0f;
            ecsWorld.addComponent<TransformAnimationComponent>(cube3, anim3);
        }

        // scene-level camera setup — no GPU resource calls
        void mainCamera()
        {
            Entity camera = ecsWorld.createEntity();

            CameraDescriptionComponent desc;
            desc.active = true;
            ecsWorld.addComponent(camera, desc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld.addComponent(camera, ct);
        }

        void addSystems(const EcsControllerContext& ctx)
        {
            gts::rendering::installRendererFeature(*this, ctx, gts::execution::rendererExecutionInputs());
            gts::animation::installAnimationFeature(
                *this, SceneExecutionProfile::gameplay(), gts::execution::groups::Animation);
        }

        void onLoad(EcsControllerContext& ctx,
                    const GtsSceneTransitionData* data = nullptr) override
        {
            addSystems(ctx);
            firstCube();
            secondCube();
            thirdCube();
            mainCamera();
        }

        void onUpdateSimulation(const EcsSimulationContext& ctx) override
        {
            ecsWorld.updateSimulation(ctx);
        }

        void onUpdateControllers(const EcsControllerContext& ctx) override
        {
            ecsWorld.updateControllers(ctx);
        }
};
