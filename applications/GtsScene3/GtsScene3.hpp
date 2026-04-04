#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "GtsScene.hpp"

#include "Entity.h"
#include "BitmapFont.h"
#include "BitmapFontLoader.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DebugCameraStateComponent.h"
#include "ECSControllerSystem.hpp"
#include "GtsAction.h"
#include "GtsKey.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "UiLayout.h"
#include "UiNode.h"
#include "UiHandle.h"

enum class StressMode
{
    Static = 0,
    Moving,
    SameTexture,
    RandomTextures
};

struct StressCubeComponent
{
    glm::vec3 staticPosition = glm::vec3(0.0f);
    float     angleOffset    = 0.0f;
    float     radius         = 0.0f;
    float     baseHeight     = 0.0f;
    uint32_t  textureVariant = 0;
};

struct StressSettingsComponent
{
    StressMode mode             = StressMode::Static;
    bool       movementEnabled  = false;
    bool       randomTextures   = false;
};

class GtsScene3 : public GtsScene
{
public:
    GtsScene3() = default;

    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;
    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;
    void populateFrameStats(GtsFrameStats& stats) const override;

private:
    static constexpr int GRID_X = 50;
    static constexpr int GRID_Z = 60;
    static constexpr int CUBE_COUNT = GRID_X * GRID_Z;

    BitmapFont overlayFont;
    UiHandle   overlayHandle = UI_INVALID_HANDLE;
    std::vector<Entity> cubeEntities;
    std::vector<std::string> texturePaths;

    void spawnCubes();
    void spawnCamera(float aspectRatio);
    void createOverlay(SceneContext& ctx);
    void updateOverlay(SceneContext& ctx) const;
    void buildTextureSet();
    void applyTextureMode();
    void advanceMode();
    const char* describeCurrentScenario() const;
};

namespace
{
class StressMotionSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (!world.hasAny<StressSettingsComponent>())
            return;

        const auto& settings = world.getSingleton<StressSettingsComponent>();
        const float t = ctx.time
            ? static_cast<float>(ctx.time->frame) * ctx.time->unscaledDeltaTime
            : 0.0f;
        const float dt = ctx.time ? ctx.time->unscaledDeltaTime : 0.0f;

        world.forEach<StressCubeComponent, TransformComponent>(
            [&](Entity e, StressCubeComponent& cube, TransformComponent& tr)
        {
            if (!settings.movementEnabled)
            {
                tr.position = cube.staticPosition;
                tr.rotation = glm::vec3(0.0f);
                return;
            }

            const float angle = t + cube.angleOffset + static_cast<float>(e.id) * 0.1f;
            tr.position.x = std::sin(angle) * cube.radius;
            tr.position.z = std::cos(angle) * cube.radius;
            tr.position.y = cube.baseHeight;
            tr.rotation.y += dt;
        });
    }
};
}

inline void GtsScene3::onLoad(SceneContext& ctx, const GtsSceneTransitionData*)
{
    resetSceneWorld();
    cubeEntities.clear();

    buildTextureSet();
    ctx.actions->clearBindings(GtsAction::DebugLayerToggle);
    ctx.actions->bind(GtsAction::DebugLayerToggle, GtsKey::F8);
    installRendererFeature(ctx);
    ecsWorld.addControllerSystem<StressMotionSystem>();
    ecsWorld.createSingleton<StressSettingsComponent>();

    spawnCubes();
    spawnCamera(ctx.windowAspectRatio);
    createOverlay(ctx);
    applyTextureMode();
    updateOverlay(ctx);
}

inline void GtsScene3::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

inline void GtsScene3::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    if (ctx.inputSource->isKeyPressed(GtsKey::F3))
    {
        advanceMode();
        applyTextureMode();
    }

    updateOverlay(ctx);
}

inline void GtsScene3::populateFrameStats(GtsFrameStats& stats) const
{
    stats.sceneEntityCount = static_cast<uint32_t>(cubeEntities.size());
}

inline void GtsScene3::spawnCubes()
{
    cubeEntities.reserve(CUBE_COUNT);

    const float spacing = 2.2f;
    const float halfX = (static_cast<float>(GRID_X) - 1.0f) * 0.5f;
    const float halfZ = (static_cast<float>(GRID_Z) - 1.0f) * 0.5f;
    const std::string cubeMesh = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";

    for (int x = 0; x < GRID_X; ++x)
    {
        for (int z = 0; z < GRID_Z; ++z)
        {
            const float worldX = (static_cast<float>(x) - halfX) * spacing;
            const float worldZ = (static_cast<float>(z) - halfZ) * spacing;

            Entity e = ecsWorld.createEntity();
            cubeEntities.push_back(e);

            TransformComponent tr;
            tr.position = glm::vec3(worldX, 0.0f, worldZ);
            tr.scale    = glm::vec3(0.9f);
            ecsWorld.addComponent(e, tr);

            StaticMeshComponent mesh;
            mesh.meshPath = cubeMesh;
            ecsWorld.addComponent(e, mesh);

            MaterialComponent mat;
            mat.texturePath = texturePaths.front();
            ecsWorld.addComponent(e, mat);

            StressCubeComponent cube;
            cube.staticPosition = tr.position;
            cube.angleOffset    = static_cast<float>(x * GRID_Z + z) * 0.0314159f;
            cube.radius         = std::max(6.0f, glm::length(glm::vec2(worldX, worldZ)));
            cube.baseHeight     = tr.position.y;
            cube.textureVariant = static_cast<uint32_t>((x * GRID_Z + z) % static_cast<int>(texturePaths.size()));
            ecsWorld.addComponent(e, cube);

            ecsWorld.addComponent(e, BoundsComponent{});
        }
    }
}

inline void GtsScene3::spawnCamera(float aspectRatio)
{
    Entity camera = ecsWorld.createEntity();

    CameraDescriptionComponent desc;
    desc.active      = true;
    desc.fov         = glm::radians(70.0f);
    desc.aspectRatio = aspectRatio;
    desc.nearClip    = 0.1f;
    desc.farClip     = 1000.0f;
    desc.target      = glm::vec3(0.0f, 0.0f, 0.0f);
    ecsWorld.addComponent(camera, desc);

    TransformComponent tr;
    tr.position = glm::vec3(0.0f, 30.0f, 95.0f);
    ecsWorld.addComponent(camera, tr);
}

inline void GtsScene3::createOverlay(SceneContext& ctx)
{
    overlayFont = BitmapFontLoader::load(
        ctx.resources,
        GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png",
        64, 40, 8, 8, 8,
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ []/:.-+",
        1.2f, true);

    overlayHandle = ctx.ui->createNode(UiNodeType::Text);
    UiLayoutSpec overlayLayout;
    overlayLayout.positionMode = UiPositionMode::Absolute;
    overlayLayout.widthMode    = UiSizeMode::Fixed;
    overlayLayout.heightMode   = UiSizeMode::Fixed;
    overlayLayout.offsetMin    = {0.01f, 0.01f};
    ctx.ui->setLayout(overlayHandle, overlayLayout);
    ctx.ui->setState(overlayHandle, UiStateFlags{
        .visible = true,
        .enabled = false,
        .interactable = false
    });
    ctx.ui->setTextFont(overlayHandle, &overlayFont);
}

inline void GtsScene3::updateOverlay(SceneContext& ctx) const
{
    if (overlayHandle == UI_INVALID_HANDLE || !ecsWorld.hasAny<StressSettingsComponent>())
        return;

    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "GTSSCENE3\nCUBES %d\nMODE %s\nF3 NEXT MODE\nF4 DEBUG CAM\nF8 OVERLAY",
                  CUBE_COUNT,
                  describeCurrentScenario());

    ctx.ui->setPayload(overlayHandle, UiTextData{
        buf,
        {},
        {1.0f, 1.0f, 1.0f, 1.0f},
        0.028f
    });
}

inline void GtsScene3::buildTextureSet()
{
    texturePaths = {
        GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/cyan_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/orange_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/purple_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/red_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png"
    };
}

inline void GtsScene3::applyTextureMode()
{
    if (!ecsWorld.hasAny<StressSettingsComponent>())
        return;

    const auto& settings = ecsWorld.getSingleton<StressSettingsComponent>();
    const std::string& sharedTexture = texturePaths.front();

    for (Entity e : cubeEntities)
    {
        if (!ecsWorld.hasComponent<MaterialComponent>(e) || !ecsWorld.hasComponent<StressCubeComponent>(e))
            continue;

        auto& mat = ecsWorld.getComponent<MaterialComponent>(e);
        const auto& cube = ecsWorld.getComponent<StressCubeComponent>(e);
        const std::string& targetTexture = settings.randomTextures
            ? texturePaths[cube.textureVariant % texturePaths.size()]
            : sharedTexture;

        if (mat.texturePath != targetTexture)
            mat.texturePath = targetTexture;
    }
}

inline void GtsScene3::advanceMode()
{
    auto& settings = ecsWorld.getSingleton<StressSettingsComponent>();

    switch (settings.mode)
    {
        case StressMode::Static:
            settings.mode = StressMode::Moving;
            settings.movementEnabled = true;
            settings.randomTextures  = false;
            break;
        case StressMode::Moving:
            settings.mode = StressMode::SameTexture;
            settings.movementEnabled = false;
            settings.randomTextures  = true;
            break;
        case StressMode::SameTexture:
            settings.mode = StressMode::RandomTextures;
            settings.movementEnabled = true;
            settings.randomTextures  = true;
            break;
        case StressMode::RandomTextures:
            settings.mode = StressMode::Static;
            settings.movementEnabled = false;
            settings.randomTextures  = false;
            break;
    }
}

inline const char* GtsScene3::describeCurrentScenario() const
{
    if (!ecsWorld.hasAny<StressSettingsComponent>())
        return "STATIC SAME";

    switch (ecsWorld.getSingleton<StressSettingsComponent>().mode)
    {
        case StressMode::Static:         return "STATIC SAME";
        case StressMode::Moving:         return "MOVING SAME";
        case StressMode::SameTexture:    return "STATIC RANDOM";
        case StressMode::RandomTextures: return "MOVING RANDOM";
    }

    return "STATIC SAME";
}
