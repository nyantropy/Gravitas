#pragma once

#include <cstdio>
#include <algorithm>
#include <stb_image.h>

#include "GtsScene.hpp"
#include "ECSWorld.hpp"

#include "TetrisInputComponent.hpp"
#include "TetrisInputSystem.hpp"
#include "TetrisGameSystem.hpp"
#include "TetrisVisualSystem.hpp"
#include "TetrisCameraSystem.hpp"
#include "TetrisCameraControlSystem.hpp"
#include "TetrisCameraControlComponent.hpp"
#include "TetrisScoreComponent.hpp"
#include "TetrisScoreSystem.hpp"
#include "ScoreDisplayComponent.hpp"

#include "RenderDescriptionComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraOverrideComponent.h"
#include "CameraControlOverrideComponent.h"
#include "TransformComponent.h"
#include "TextComponent.h"

#include "GraphicsConstants.h"

class TetrisScene : public GtsScene
{
    BitmapFont scoreFont;
    Entity     scoreEntity = Entity{ UINT32_MAX };

    Entity spawnCube(const std::string& texturePath, const glm::vec3& position)
    {
        Entity e = ecsWorld.createEntity();

        RenderDescriptionComponent desc;
        desc.meshPath    = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
        desc.texturePath = texturePath;
        ecsWorld.addComponent<RenderDescriptionComponent>(e, desc);

        TransformComponent tc;
        tc.position = position;
        ecsWorld.addComponent<TransformComponent>(e, tc);

        return e;
    }

    void buildTetrisFrame()
    {
        constexpr int fieldWidth  = 10;
        constexpr int fieldHeight = 20;

        const std::string frameTexture =
            GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3(-1.0f, (float)y, 0.0f));

        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3((float)fieldWidth, (float)y, 0.0f));

        for (int x = -1; x <= fieldWidth; ++x)
            spawnCube(frameTexture, glm::vec3((float)x, -1.0f, 0.0f));
    }

    void buildNextLabel()
    {
        if (TetrisGameSystem::QUEUE_SIZE <= 0)
            return;

        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = glm::vec3(11.5f, 14.5f, 0.0f);
        ecsWorld.addComponent(e, tc);

        TextComponent text;
        text.text  = "NEXT";
        text.font  = &scoreFont;
        text.scale = 1.0f;
        text.dirty = true;
        ecsWorld.addComponent(e, text);
    }

    void buildHoldLabel()
    {
        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = glm::vec3(-4.0f, 17.5f, 0.0f);
        ecsWorld.addComponent(e, tc);

        TextComponent text;
        text.text  = "HOLD";
        text.font  = &scoreFont;
        text.scale = 1.0f;
        text.dirty = true;
        ecsWorld.addComponent(e, text);
    }

    void buildScoreboard(SceneContext& ctx)
    {
        const std::string atlasPath =
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png";

        scoreFont.atlasTexture = ctx.resources->requestPixelTexture(atlasPath);
        scoreFont.lineHeight   = 1.2f;

        constexpr int ATLAS_W = 64;
        constexpr int ATLAS_H = 40;

        constexpr int CELL_W = 8;
        constexpr int CELL_H = 8;
        constexpr int COLS   = 8;

        constexpr char charOrder[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        constexpr int  charCount   = sizeof(charOrder) - 1;

        constexpr float fW = float(ATLAS_W);
        constexpr float fH = float(ATLAS_H);

        for (int i = 0; i < charCount; ++i)
        {
            const int col = i % COLS;
            const int row = i / COLS;

            const int ax0 = col * CELL_W;
            const int ay0 = row * CELL_H;
            const int ax1 = ax0 + CELL_W - 1;
            const int ay1 = ay0 + CELL_H - 1;

            scoreFont.glyphs[charOrder[i]] = GlyphInfo{
                .uvMin   = { ax0 / fW,        ay0 / fH },
                .uvMax   = { (ax1 + 1) / fW, (ay1 + 1) / fH },
                .size    = { 1.0f, 1.0f },
                .bearing = { 0.0f, 1.0f },
                .advance = 1.0f
            };
        }

        scoreEntity = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = glm::vec3(11.5f, 18.0f, 0.0f);
        ecsWorld.addComponent(scoreEntity, tc);

        TextComponent text;
        text.text  = "SCORE\n00000000";
        text.font  = &scoreFont;
        text.scale = 1.0f;
        text.dirty = true;
        ecsWorld.addComponent(scoreEntity, text);
        ecsWorld.addComponent(scoreEntity, ScoreDisplayComponent{});
    }

    void mainCamera()
    {
        Entity camera = ecsWorld.createEntity();

        CameraDescriptionComponent desc;
        desc.active      = true;
        desc.fov         = glm::radians(60.0f);
        desc.aspectRatio = 800.0f / 800.0f;
        desc.nearClip    = 1.0f;
        desc.farClip     = 1000.0f;
        ecsWorld.addComponent(camera, desc);

        ecsWorld.addComponent(camera, TransformComponent{});
        ecsWorld.addComponent(camera, CameraOverrideComponent{});
        ecsWorld.addComponent(camera, CameraControlOverrideComponent{});
        ecsWorld.addComponent(camera, TetrisCameraControlComponent{});
    }

    void addSingletonComponents()
    {
        ecsWorld.createSingleton<TetrisInputComponent>();
        ecsWorld.createSingleton<TetrisScoreComponent>();
    }

public:
    void updateScoreDisplay(int score)
    {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%08d", score);

        auto& text = ecsWorld.getComponent<TextComponent>(scoreEntity);
        text.text  = "SCORE\n" + std::string(buf);
        text.dirty = true;
    }

    void onLoad(SceneContext& ctx) override
    {
        buildTetrisFrame();
        mainCamera();
        buildScoreboard(ctx);
        buildNextLabel();
        buildHoldLabel();

        addSingletonComponents();
        installRendererFeature();

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        ecsWorld.addControllerSystem<TetrisCameraControlSystem>();
        ecsWorld.addSimulationSystem<TetrisGameSystem>();
        ecsWorld.addSimulationSystem<TetrisScoreSystem>();
        ecsWorld.addControllerSystem<TetrisVisualSystem>();
        ecsWorld.addSimulationSystem<TetrisCameraSystem>();
    }

    void onUpdate(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
        ecsWorld.updateControllers(ctx);
    }
};