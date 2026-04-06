#include "scene/BattleScene.h"

#include <limits>
#include <memory>

#include "components/BattleResultTransitionData.h"
#include "components/BattleTransitionData.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "GlmConfig.h"
#include "GtsKey.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"

void BattleScene::onLoad(SceneContext& ctx, const GtsSceneTransitionData* data)
{
    constexpr Entity invalidEntity{std::numeric_limits<entity_id_type>::max()};

    resetSceneWorld();
    actionManager = InputActionManager<DungeonAction>{};
    actionManager.bind(DungeonAction::BattleExit, GtsKey::Q);
    enemyEntity = invalidEntity;
    floorEntity = invalidEntity;
    cameraEntity = invalidEntity;
    returnSceneName = "dungeon_test";
    returnFloorIndex = -1;
    returnPlayerTile = {-1, -1};
    defeatedEnemyTile = {-1, -1};

    if (const auto* battleData = dynamic_cast<const BattleTransitionData*>(data))
    {
        returnSceneName = battleData->returnSceneName;
        returnFloorIndex = battleData->floorIndex;
        returnPlayerTile = battleData->playerTile;
        defeatedEnemyTile = battleData->enemySpawnTile;
    }

    floorEntity = ecsWorld.createEntity();

    ProceduralMeshComponent floorMesh;
    floorMesh.width = 8.0f;
    floorMesh.height = 8.0f;

    MaterialComponent floorMaterial;
    floorMaterial.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
    floorMaterial.doubleSided = true;
    floorMaterial.tint = {0.32f, 0.34f, 0.38f, 1.0f};

    TransformComponent floorTransform;
    floorTransform.position = {0.0f, 0.0f, 0.0f};
    floorTransform.rotation.x = -glm::half_pi<float>();

    BoundsComponent floorBounds;
    floorBounds.min = {-4.0f, -4.0f, -0.02f};
    floorBounds.max = { 4.0f,  4.0f,  0.02f};

    ecsWorld.addComponent(floorEntity, floorMesh);
    ecsWorld.addComponent(floorEntity, floorMaterial);
    ecsWorld.addComponent(floorEntity, floorTransform);
    ecsWorld.addComponent(floorEntity, floorBounds);

    enemyEntity = ecsWorld.createEntity();

    StaticMeshComponent enemyMesh;
    enemyMesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";

    MaterialComponent enemyMaterial;
    enemyMaterial.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png";
    enemyMaterial.tint = {0.25f, 0.45f, 1.0f, 1.0f};

    TransformComponent enemyTransform;
    enemyTransform.position = {0.0f, 0.5f, 0.0f};
    enemyTransform.scale = {1.0f, 1.0f, 1.0f};

    BoundsComponent enemyBounds;
    enemyBounds.min = {-0.5f, -0.5f, -0.5f};
    enemyBounds.max = { 0.5f,  0.5f,  0.5f};

    ecsWorld.addComponent(enemyEntity, enemyMesh);
    ecsWorld.addComponent(enemyEntity, enemyMaterial);
    ecsWorld.addComponent(enemyEntity, enemyTransform);
    ecsWorld.addComponent(enemyEntity, enemyBounds);

    cameraEntity = ecsWorld.createEntity();

    TransformComponent cameraTransform;
    cameraTransform.position = {0.0f, 3.0f, 5.5f};

    CameraDescriptionComponent camera;
    camera.active = true;
    camera.fov = glm::radians(70.0f);
    camera.aspectRatio = ctx.windowAspectRatio;
    camera.nearClip = 0.05f;
    camera.farClip = 100.0f;
    camera.target = {0.0f, 0.75f, 0.0f};

    ecsWorld.addComponent(cameraEntity, cameraTransform);
    ecsWorld.addComponent(cameraEntity, camera);

    installRendererFeature(ctx);
}

void BattleScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

void BattleScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    actionManager.update(*ctx.inputSource);

    if (!actionManager.isActionPressed(DungeonAction::BattleExit))
        return;

    auto result = std::make_shared<BattleResultTransitionData>();
    result->floorIndex = returnFloorIndex;
    result->playerTile = returnPlayerTile;
    result->defeatedEnemyTile = defeatedEnemyTile;
    ctx.engineCommands->requestChangeScene(returnSceneName, result);
}
