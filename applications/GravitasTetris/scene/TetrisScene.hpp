#pragma once

#include <cstdio>

#include "GtsScene.hpp"
#include "ECSWorld.hpp"

#include "TetrisInputComponent.hpp"
#include "TetrisInputSystem.hpp"
#include "TetrisGameSystem.hpp"
#include "TetrisVisualSystem.hpp"
#include "TetrisCameraSystem.hpp"
#include "TetrisScoreComponent.hpp"
#include "TetrisScoreSystem.hpp"
#include "ScoreDisplayComponent.hpp"

#include "RenderDescriptionComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraOverrideComponent.h"
#include "TransformComponent.h"
#include "TextComponent.h"

#include "GraphicsConstants.h"

// the central scene for the game, most delegation logic happens here
class TetrisScene : public GtsScene
{
    // --- scoreboard state ---
    BitmapFont scoreFont;
    Entity     scoreEntity = Entity{ UINT32_MAX };

    // spawns a single cube with a given texture at a given coordinate.
    // Only sets logical description and position — no GPU resource calls.
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

        const std::string frameTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3(-1.0f, (float)y, 0.0f));

        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3((float)fieldWidth, (float)y, 0.0f));

        for (int x = -1; x <= fieldWidth; ++x)
            spawnCube(frameTexture, glm::vec3((float)x, -1.0f, 0.0f));
    }

    // Build the tetrisfont.png atlas mapping and spawn the scoreboard entity.
    //
    // Atlas: 1536 × 1024 pixels, three rows of glyphs:
    //   row 1 (y 377–471): S  C  O  R  E
    //   row 2 (y 499–580): 0  1  2  3  4  5  6
    //   row 3 (y 607–688): 7  8  9
    //
    // Glyph metrics are in world units; TextBuildSystem multiplies by scale:
    //   size = (1,1), bearing = (0,1), advance = 1.0, lineHeight = 1.2
    void buildScoreboard(SceneContext& ctx)
    {
        // Load the atlas texture through the normal resource path.
        scoreFont.atlasTexture = ctx.resources->requestTexture(
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/tetrisfont.png");
        scoreFont.lineHeight = 1.2f;

        // Atlas: 1536 x 1024 pixels (measured via pixel analysis of tetrisfont.png).
        // Each glyph entry uses pixel-exact bounding boxes converted to normalized UVs:
        //   uvMin = (x0 / 1536, y0 / 1024)
        //   uvMax = ((x1+1) / 1536, (y1+1) / 1024)   ← +1 to include the last pixel column/row
        constexpr float W = 1536.0f;
        constexpr float H = 1024.0f;

        auto px = [](float x0, float y0, float x1, float y1) -> GlyphInfo
        {
            return GlyphInfo{
                .uvMin   = { x0 / W,        y0 / H        },
                .uvMax   = { (x1 + 1) / W,  (y1 + 1) / H  },
                .size    = { 1.0f, 1.0f },
                .bearing = { 0.0f, 1.0f },
                .advance = 1.0f
            };
        };

        // Row 1 — letters (y: 377–471)
        scoreFont.glyphs['S'] = px( 411, 377,  505, 471);
        scoreFont.glyphs['C'] = px( 533, 377,  628, 471);
        scoreFont.glyphs['O'] = px( 646, 377,  742, 471);
        scoreFont.glyphs['R'] = px( 760, 377,  854, 471);
        scoreFont.glyphs['E'] = px( 873, 378,  967, 471);

        // Row 2 — digits 0–6 (y: 499–580)
        scoreFont.glyphs['0'] = px( 409, 499,  492, 580);
        scoreFont.glyphs['1'] = px( 524, 499,  599, 580);
        scoreFont.glyphs['2'] = px( 624, 499,  711, 580);
        scoreFont.glyphs['3'] = px( 730, 499,  819, 580);
        scoreFont.glyphs['4'] = px( 837, 498,  927, 580);
        scoreFont.glyphs['5'] = px( 944, 499, 1027, 580);
        scoreFont.glyphs['6'] = px(1053, 499, 1140, 580);

        // Row 3 — digits 7–9 (y: 607–688)
        scoreFont.glyphs['7'] = px( 408, 607,  491, 688);
        scoreFont.glyphs['8'] = px( 519, 607,  605, 688);
        scoreFont.glyphs['9'] = px( 625, 607,  711, 688);

        // Spawn the UI entity to the right of the field (field spans x = -1..10).
        // With scale = 0.5 each glyph occupies 0.5 world units; "00000000" is
        // 4 units wide and sits comfortably within the telephoto camera frustum.
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
    }

    void addSingletonComponents()
    {
        ecsWorld.createSingleton<TetrisInputComponent>();
        ecsWorld.createSingleton<TetrisScoreComponent>();
    }

public:
    // Write a new score value into the scoreboard and mark it for rebuild.
    // Call this from gameplay code whenever the score changes.
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

        addSingletonComponents();
        installRendererFeature();

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        ecsWorld.addSimulationSystem<TetrisGameSystem>();
        ecsWorld.addSimulationSystem<TetrisScoreSystem>(); // runs after game system to drain events same tick
        ecsWorld.addControllerSystem<TetrisVisualSystem>();

        // Registered after TetrisGameSystem so it runs on fully updated game state.
        // CameraBindingSystem is a controller and always follows all simulation,
        // so the perspective matrices are guaranteed to be ready before GPU upload.
        ecsWorld.addSimulationSystem<TetrisCameraSystem>();
    }

    void onUpdate(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
        ecsWorld.updateControllers(ctx);
    }
};
