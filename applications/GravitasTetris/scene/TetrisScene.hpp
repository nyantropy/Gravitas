#pragma once

#include <cstdio>
#include <algorithm>
#include <stb_image.h>

#include "GtsScene.hpp"
#include "ECSWorld.hpp"

#include "TetrisInputComponent.hpp"
#include "TetrisInputSystem.hpp"
#include "TetrisGameSystem.hpp"
#include "TetrisBlockInitSystem.hpp"
#include "TetrisVisualSystem.hpp"
#include "TetrisCameraSystem.hpp"
#include "TetrisCameraControlSystem.hpp"
#include "TetrisCameraControlComponent.hpp"
#include "TetrisScoreComponent.hpp"
#include "TetrisScoreSystem.hpp"
#include "ScoreDisplayComponent.hpp"

#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "TransformComponent.h"
#include "MaterialComponent.h"
#include "ProceduralMeshComponent.h"
#include "WorldTextComponent.h"
#include "BitmapFontLoader.h"
#include "HierarchyHelper.h"

#include "Vertex.h"
#include "GraphicsConstants.h"
#include "AiRegistration.h"

class TetrisScene : public GtsScene
{
    BitmapFont scoreFont;

    // anchor entities for ui elements
    Entity holdGroupAnchor;
    Entity nextGroupAnchor;

    Entity spawnProceduralMeshEntity(const std::vector<Vertex>&   verts,
                                     const std::vector<uint32_t>& idxs)
    {
        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = {0.0f, 0.0f, 0.0f};
        ecsWorld.addComponent(e, tc);

        ProceduralMeshComponent mesh;
        mesh.useCustomGeometry = true;
        mesh.vertices          = verts;
        mesh.indices           = idxs;
        mesh.geometryVersion   = 1;
        ecsWorld.addComponent(e, mesh);

        MaterialComponent material;
        material.texturePath =
            GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
        ecsWorld.addComponent(e, material);

        return e;
    }

    void buildTetrisFrameMesh()
    {
        // Inner edges of the former cube walls (cubes were unit-cubes centered on
        // integer positions, so x=-1 spanned x=-1.5..-0.5, giving inner edge -0.5).
        constexpr float LEFT_X  = -0.5f;   // inner edge of left wall
        constexpr float RIGHT_X =  9.5f;   // inner edge of right wall
        constexpr float BOT_Y   = -0.5f;   // inner edge of floor
        constexpr float TOP_Y   = 19.5f;   // top of play area (no top wall in original)
        constexpr float T       =  0.06f;  // line half-thickness → total width 0.12 units
        constexpr float Z       =  0.0f;

        const glm::vec3 white = {1.0f, 1.0f, 1.0f};

        std::vector<Vertex>   verts;
        std::vector<uint32_t> idxs;

        auto addQuad = [&](float x0, float y0, float x1, float y1)
        {
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back({{x0, y0, Z}, white, {0.0f, 1.0f}});
            verts.push_back({{x1, y0, Z}, white, {1.0f, 1.0f}});
            verts.push_back({{x1, y1, Z}, white, {1.0f, 0.0f}});
            verts.push_back({{x0, y1, Z}, white, {0.0f, 0.0f}});
            idxs.push_back(base + 0); idxs.push_back(base + 1); idxs.push_back(base + 2);
            idxs.push_back(base + 0); idxs.push_back(base + 2); idxs.push_back(base + 3);
        };

        // Left wall: vertical strip at x = LEFT_X
        addQuad(LEFT_X - T, BOT_Y - T, LEFT_X + T, TOP_Y);
        // Right wall: vertical strip at x = RIGHT_X
        addQuad(RIGHT_X - T, BOT_Y - T, RIGHT_X + T, TOP_Y);
        // Floor: horizontal strip at y = BOT_Y, spans full width to cover corners
        addQuad(LEFT_X - T, BOT_Y - T, RIGHT_X + T, BOT_Y + T);

        spawnProceduralMeshEntity(verts, idxs);
    }

    Entity buildHoldBoxMesh()
    {
        // Box corners — line centers (T = half-thickness of each line strip).
        // Width is sized to give the I piece (4 cells wide) 0.5-unit padding on each
        // side, matching the 0.5-unit top/bottom padding already present for 2-tall pieces.
        //   I-piece local x-extents (anchor at -5): [-1.5, 2.5]  →  box local [-2, 3]
        //   which translates to world BOX_L = -7, BOX_R = -2.
        constexpr float BOX_L = -7.0f;  // left side x  (was -6.5; widened for I-piece padding)
        constexpr float BOX_R = -2.0f;  // right side x (was -2.5; widened for I-piece padding)
        constexpr float BOX_B = 15.0f;  // bottom y — 0.5 below piece bottom at 15.5
        constexpr float BOX_T = 18.0f;  // top y    — 0.5 above standard piece top at 17.5
        constexpr float T     =  0.06f; // same half-thickness as playfield frame
        constexpr float Z     =  0.0f;

        const glm::vec3 white = {1.0f, 1.0f, 1.0f};

        std::vector<Vertex>   verts;
        std::vector<uint32_t> idxs;

        auto addQuad = [&](float x0, float y0, float x1, float y1)
        {
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back({{x0, y0, Z}, white, {0.0f, 1.0f}});
            verts.push_back({{x1, y0, Z}, white, {1.0f, 1.0f}});
            verts.push_back({{x1, y1, Z}, white, {1.0f, 0.0f}});
            verts.push_back({{x0, y1, Z}, white, {0.0f, 0.0f}});
            idxs.push_back(base + 0); idxs.push_back(base + 1); idxs.push_back(base + 2);
            idxs.push_back(base + 0); idxs.push_back(base + 2); idxs.push_back(base + 3);
        };

        // Left side — vertical strip, full box height (corners absorbed by horizontal strips)
        addQuad(BOX_L - T, BOX_B - T, BOX_L + T, BOX_T + T);
        // Right side — vertical strip
        addQuad(BOX_R - T, BOX_B - T, BOX_R + T, BOX_T + T);
        // Bottom — horizontal strip spanning box width
        addQuad(BOX_L - T, BOX_B - T, BOX_R + T, BOX_B + T);
        // Top — horizontal strip spanning box width
        addQuad(BOX_L - T, BOX_T - T, BOX_R + T, BOX_T + T);

        return spawnProceduralMeshEntity(verts, idxs);
    }

    Entity buildNextLabel()
    {
        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        // Local offset from nextGroupAnchor at (13, 16, 0):
        // old world (11, 19, 0) − anchor (13, 16, 0) = (−2, 3, 0)
        tc.position = glm::vec3(-2.0f, 3.0f, 0.0f);
        ecsWorld.addComponent(e, tc);

        WorldTextComponent text;
        text.text  = "NEXT";
        text.font  = &scoreFont;
        text.scale = 1.0f;

        ecsWorld.addComponent(e, text);

        return e;
    }

    Entity buildHoldLabel()
    {
        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = glm::vec3(-1.5f, 3.0f, 0.0f);
        ecsWorld.addComponent(e, tc);

        WorldTextComponent text;
        text.text  = "HOLD";
        text.font  = &scoreFont;
        text.scale = 1.0f;

        ecsWorld.addComponent(e, text);

        return e;
    }

    void buildScoreboard(SceneContext& ctx)
    {
        const std::string atlasPath =
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png";

        scoreFont = BitmapFontLoader::load(
            ctx.resources, atlasPath,
            /*atlasW=*/64, /*atlasH=*/40,
            /*cellW=*/8,   /*cellH=*/8,  /*cols=*/8,
            /*charOrder=*/"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ",
            /*lineHeight=*/1.2f,
            /*pixelSampling=*/true);

        // Left sidebar: SPEED LV and LINES displayed below the HOLD piece preview.
        // AGENT CHANGE: y moved from 10.5→4.5 to share the same baseline as the right
        // stats panel (SCORE + BEST), creating a balanced horizontal band at the bottom.
        // With HOLD_DISPLAY_PIVOT y=16 the hold box bottom is at y=15.0, leaving
        // intentional white space between it and the stats — mirroring the right side
        // where four NEXT preview slots fill the equivalent vertical span.
        {
            Entity leftStats = ecsWorld.createEntity();

            TransformComponent tc;
            tc.position = glm::vec3(-5.5f, 4.5f, 0.0f);   // AGENT CHANGE: aligned with right stats at y=4.5
            ecsWorld.addComponent(leftStats, tc);

            WorldTextComponent text;
            text.text  = "LEVEL\n1\nLINES\n0000";
            text.font  = &scoreFont;
            text.scale = 0.6f;
    
            ecsWorld.addComponent(leftStats, text);
            ecsWorld.addComponent(leftStats, ScoreDisplayComponent{0});  // id=0: left panel
        }

        // Right sidebar: SCORE and BEST displayed below the NEXT preview queue.
        // The lowest preview piece centers at y=7; this panel sits below it at y=4.5.
        {
            Entity rightStats = ecsWorld.createEntity();

            TransformComponent tc;
            tc.position = glm::vec3(11.0f, 4.5f, 0.0f);
            ecsWorld.addComponent(rightStats, tc);

            WorldTextComponent text;
            text.text  = "SCORE\n00000000\nBEST\n00000000";
            text.font  = &scoreFont;
            text.scale = 0.6f;
    
            ecsWorld.addComponent(rightStats, text);
            ecsWorld.addComponent(rightStats, ScoreDisplayComponent{1});  // id=1: right panel
        }
    }

    void mainCamera(float aspectRatio)
    {
        Entity camera = ecsWorld.createEntity();

        CameraDescriptionComponent desc;
        desc.active      = true;
        desc.fov         = glm::radians(60.0f);
        desc.aspectRatio = aspectRatio;
        desc.nearClip    = 1.0f;
        desc.farClip     = 1000.0f;
        ecsWorld.addComponent(camera, desc);

        ecsWorld.addComponent(camera, TransformComponent{});
        ecsWorld.addComponent(camera, CameraControlOverrideComponent{});
        ecsWorld.addComponent(camera, TetrisCameraControlComponent{});
    }

    void addSingletonComponents()
    {
        ecsWorld.createSingleton<TetrisInputComponent>();
        ecsWorld.createSingleton<TetrisScoreComponent>();
    }

public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override
    {
        // ── Invisible anchor for the Hold UI group ────────────────────────────
        // World position matches HOLD_DISPLAY_PIVOT so child local offsets are
        // relative to that point.  Move holdGroupAnchor to reposition the whole
        // Hold UI: box outline, held piece blocks, and label.
        holdGroupAnchor = ecsWorld.createEntity();
        {
            TransformComponent tc;
            tc.position = glm::vec3(
                float(HoldController::HOLD_DISPLAY_PIVOT.x),
                float(HoldController::HOLD_DISPLAY_PIVOT.y),
                0.0f);
            ecsWorld.addComponent(holdGroupAnchor, tc);
        }

        // ── Invisible anchor for the Next UI group ────────────────────────────
        // World position matches NEXT_DISPLAY_PIVOT (first preview slot origin).
        // Move nextGroupAnchor to reposition the whole Next UI: label and all
        // preview piece slots.
        nextGroupAnchor = ecsWorld.createEntity();
        {
            TransformComponent tc;
            tc.position = glm::vec3(
                float(TetrisGameSystem::NEXT_DISPLAY_PIVOT.x),
                float(TetrisGameSystem::NEXT_DISPLAY_PIVOT.y),
                0.0f);
            ecsWorld.addComponent(nextGroupAnchor, tc);
        }

        buildTetrisFrameMesh();

        // Hold box outline — local offset converts world (0,0,0) to be relative
        // to holdGroupAnchor at (−5, 16, 0): local = (5, −16, 0).
        // Verify: anchor (−5,16) + local (5,−16) = world (0, 0) ✓
        Entity holdBoxEntity = buildHoldBoxMesh();
        ecsWorld.getComponent<TransformComponent>(holdBoxEntity).position = { 5.0f, -16.0f, 0.0f };
        setParent(ecsWorld, holdBoxEntity, holdGroupAnchor);

        mainCamera(ctx.windowAspectRatio);
        buildScoreboard(ctx);

        // NEXT label — local offset: world (11,19) − anchor (13,16) = (−2, 3)
        if (TetrisGameSystem::QUEUE_SIZE > 0)
        {
            Entity nextLabelEntity = buildNextLabel();
            setParent(ecsWorld, nextLabelEntity, nextGroupAnchor);
        }

        // HOLD label — local offset: world (−5.5,19) − anchor (−5,16) = (−0.5, 3)
        {
            Entity holdLabelEntity = buildHoldLabel();
            setParent(ecsWorld, holdLabelEntity, holdGroupAnchor);
        }

        addSingletonComponents();

        // Registration order is critical for correctness:
        //
        // 1. TetrisBlockInitSystem — runs first; attaches TransformComponent,
        //    StaticMeshComponent, and MaterialComponent to any newly spawned block
        //    entity. Newly spawned entities have only TetrisBlockComponent; without
        //    this system running before StaticMeshBindingSystem, the binding system
        //    would skip them on their first frame and produce a one-frame texture flash.
        //
        // 2. TetrisVisualSystem — runs second; keeps texture paths and transform
        //    positions in sync for ghost, held, and next-preview blocks. It can
        //    assume all components are already attached.
        //
        // 3. installRendererFeature — registers StaticMeshBindingSystem (and others).
        //    By the time it runs, both visual systems have already written the
        //    correct mat.texturePath, so binding always sees the right state.
        ecsWorld.addControllerSystem<TetrisBlockInitSystem>();
        ecsWorld.addControllerSystem<TetrisVisualSystem>();

        installRendererFeature(ctx);

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        // Register TetrisGameSystem before the AI so we can pass a direct reference.
        // Simulation-phase order is unchanged: TetrisGameSystem remains first.
        TetrisGameSystem& gameSystem =
            ecsWorld.addSimulationSystem<TetrisGameSystem>(holdGroupAnchor, nextGroupAnchor);
        RegisterAi(ecsWorld, gameSystem);
        ecsWorld.addControllerSystem<TetrisCameraControlSystem>();
        ecsWorld.addSimulationSystem<TetrisScoreSystem>();
        ecsWorld.addControllerSystem<TetrisCameraSystem>();
    }

    void onUpdateSimulation(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
    }

    void onUpdateControllers(SceneContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);
    }
};
