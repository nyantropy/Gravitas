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
#include "RetroFontAtlas.h"
#include "DebugCameraStateComponent.h"
#include "ECSControllerSystem.hpp"
#include "GtsAction.h"
#include "GtsKey.h"
#include "GraphicsConstants.h"
#include "EngineConfig.h"
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
    float     baseHeight     = 0.0f;
    uint32_t  textureVariant = 0;
    float     phaseOffset    = 0.0f;
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
    static constexpr uint32_t INITIAL_CUBE_COUNT = 3000;
    static constexpr uint32_t CUBE_BATCH_SIZE    = 1000;
    static constexpr uint32_t GRID_COLUMNS       = 100;
    static constexpr float    GRID_SPACING       = 2.15f;
    static constexpr float    CUBE_SCALE         = 0.9f;
    static constexpr uint32_t RENDERABLE_HEADROOM = 128;
    static constexpr float    GRID_Z_OFFSET      = -106.0f;

    BitmapFont overlayFont;
    UiHandle   overlayHandle = UI_INVALID_HANDLE;
    std::vector<Entity> cubeEntities;
    std::vector<std::string> texturePaths;

    void spawnCubes(uint32_t count);
    void removeCubes(uint32_t count);
    glm::vec3 computeCubePosition(uint32_t index) const;
    uint32_t maxCubeCount() const;
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
constexpr float STRESS_WAVE_AMPLITUDE = 0.7f;
constexpr float STRESS_WAVE_SPEED     = 0.9f;
constexpr float STRESS_PHASE_X_STEP   = 0.14f;
constexpr float STRESS_PHASE_Z_STEP   = 0.18f;

class StressMotionSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (!world.hasAny<StressSettingsComponent>())
            return;

        const float dt = ctx.time ? ctx.time->unscaledDeltaTime : 0.0f;
        accumulatedTime += dt;

        const auto& settings = world.getSingleton<StressSettingsComponent>();

        world.forEach<StressCubeComponent, TransformComponent>(
            [&](Entity, StressCubeComponent& cube, TransformComponent& tr)
        {
            if (!settings.movementEnabled)
            {
                tr.position = cube.staticPosition;
                tr.rotation = glm::vec3(0.0f);
                return;
            }

            const float wave = std::sin(accumulatedTime * STRESS_WAVE_SPEED + cube.phaseOffset);
            tr.position = cube.staticPosition;
            tr.position.y = cube.baseHeight + wave * STRESS_WAVE_AMPLITUDE;
            tr.rotation.x = 0.0f;
            tr.rotation.z = 0.0f;
            tr.rotation.y = wave * 0.08f;
        });
    }

private:
    float accumulatedTime = 0.0f;
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

    spawnCubes(INITIAL_CUBE_COUNT);
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

    if (ctx.inputSource->isKeyPressed(GtsKey::Digit1))
    {
        spawnCubes(CUBE_BATCH_SIZE);
        applyTextureMode();
    }

    if (ctx.inputSource->isKeyPressed(GtsKey::Digit2))
        removeCubes(CUBE_BATCH_SIZE);

    updateOverlay(ctx);
}

inline void GtsScene3::populateFrameStats(GtsFrameStats& stats) const
{
    stats.sceneEntityCount = static_cast<uint32_t>(cubeEntities.size());
}

inline glm::vec3 GtsScene3::computeCubePosition(uint32_t index) const
{
    const uint32_t gridX = index % GRID_COLUMNS;
    const uint32_t gridZ = index / GRID_COLUMNS;
    const float halfX = (static_cast<float>(GRID_COLUMNS) - 1.0f) * 0.5f;
    const float worldX = (static_cast<float>(gridX) - halfX) * GRID_SPACING;
    const float worldZ = GRID_Z_OFFSET + static_cast<float>(gridZ) * GRID_SPACING;
    return {worldX, 0.0f, worldZ};
}

inline uint32_t GtsScene3::maxCubeCount() const
{
    const uint32_t limit = EngineConfig::MAX_RENDERABLE_OBJECTS;
    return limit > RENDERABLE_HEADROOM ? limit - RENDERABLE_HEADROOM : limit;
}

inline void GtsScene3::spawnCubes(uint32_t count)
{
    const uint32_t currentCount = static_cast<uint32_t>(cubeEntities.size());
    const uint32_t targetCount = std::min(maxCubeCount(), currentCount + count);
    const std::string cubeMesh = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";

    cubeEntities.reserve(targetCount);

    for (uint32_t index = currentCount; index < targetCount; ++index)
    {
        const glm::vec3 staticPos = computeCubePosition(index);
        const uint32_t gridX = index % GRID_COLUMNS;
        const uint32_t gridZ = index / GRID_COLUMNS;

        Entity e = ecsWorld.createEntity();
        cubeEntities.push_back(e);

        TransformComponent tr;
        tr.position = staticPos;
        tr.scale    = glm::vec3(CUBE_SCALE);
        ecsWorld.addComponent(e, tr);

        StaticMeshComponent mesh;
        mesh.meshPath = cubeMesh;
        ecsWorld.addComponent(e, mesh);

        MaterialComponent mat;
        mat.texturePath = texturePaths.front();
        ecsWorld.addComponent(e, mat);

        StressCubeComponent cube;
        cube.staticPosition = staticPos;
        cube.baseHeight     = staticPos.y;
        cube.textureVariant = index % static_cast<uint32_t>(texturePaths.size());
        cube.phaseOffset    = static_cast<float>(gridX) * STRESS_PHASE_X_STEP
                            + static_cast<float>(gridZ) * STRESS_PHASE_Z_STEP;
        ecsWorld.addComponent(e, cube);

        ecsWorld.addComponent(e, BoundsComponent{});
    }
}

inline void GtsScene3::removeCubes(uint32_t count)
{
    const uint32_t removeCount = std::min<uint32_t>(count, static_cast<uint32_t>(cubeEntities.size()));
    for (uint32_t i = 0; i < removeCount; ++i)
    {
        const Entity e = cubeEntities.back();
        cubeEntities.pop_back();
        ecsWorld.destroyEntity(e);
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
    tr.position = glm::vec3(0.0f, 60.0f, 170.0f);
    ecsWorld.addComponent(camera, tr);
}

inline void GtsScene3::createOverlay(SceneContext& ctx)
{
    overlayFont = BitmapFontLoader::load(
        ctx.resources,
        GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png",
        RetroFontAtlas::ATLAS_W, RetroFontAtlas::ATLAS_H,
        RetroFontAtlas::CELL_W, RetroFontAtlas::CELL_H, RetroFontAtlas::ATLAS_COLS,
        std::string(RetroFontAtlas::CHAR_ORDER),
        RetroFontAtlas::LINE_HEIGHT, true);

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
                  "GTSSCENE3\nCUBES %u / %u\nMODE %s\n1 +1000  2 -1000\nF3 NEXT MODE\nF4 DEBUG CAM\nF8 OVERLAY",
                  static_cast<uint32_t>(cubeEntities.size()),
                  maxCubeCount(),
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
