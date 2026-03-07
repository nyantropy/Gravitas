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
#include "RenderGpuComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraOverrideComponent.h"
#include "CameraControlOverrideComponent.h"
#include "TransformComponent.h"
#include "TextComponent.h"
#include "HierarchyHelper.h"

#include "Vertex.h"
#include "GraphicsConstants.h"

class TetrisScene : public GtsScene
{
    BitmapFont scoreFont;

    // anchor entities for ui elements
    Entity holdGroupAnchor;
    Entity nextGroupAnchor;

    Entity spawnProceduralMeshEntity(SceneContext& ctx,
                                     std::vector<Vertex>&   verts,
                                     std::vector<uint32_t>& idxs)
    {
        const std::string greyTex =
            GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

        mesh_id_type    meshID = ctx.resources->uploadProceduralMesh(0, verts, idxs);
        texture_id_type texID  = ctx.resources->requestTexture(greyTex);
        ssbo_id_type    slot   = ctx.resources->requestObjectSlot();

        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = {0.0f, 0.0f, 0.0f};
        ecsWorld.addComponent(e, tc);

        RenderGpuComponent rc{};
        rc.meshID         = meshID;
        rc.textureID      = texID;
        rc.objectSSBOSlot = slot;
        rc.dirty          = true;
        rc.readyToRender  = false;
        ecsWorld.addComponent(e, rc);

        return e;
    }

    void buildTetrisFrameMesh(SceneContext& ctx)
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

        spawnProceduralMeshEntity(ctx, verts, idxs);
    }

    Entity buildHoldBoxMesh(SceneContext& ctx)
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

        return spawnProceduralMeshEntity(ctx, verts, idxs);
    }

    Entity buildNextLabel()
    {
        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        // Local offset from nextGroupAnchor at (13, 16, 0):
        // old world (11, 19, 0) − anchor (13, 16, 0) = (−2, 3, 0)
        tc.position = glm::vec3(-2.0f, 3.0f, 0.0f);
        ecsWorld.addComponent(e, tc);

        TextComponent text;
        text.text  = "NEXT";
        text.font  = &scoreFont;
        text.scale = 1.0f;
        text.dirty = true;
        ecsWorld.addComponent(e, text);

        return e;
    }

    Entity buildHoldLabel()
    {
        Entity e = ecsWorld.createEntity();

        TransformComponent tc;
        tc.position = glm::vec3(-1.5f, 3.0f, 0.0f);
        ecsWorld.addComponent(e, tc);

        TextComponent text;
        text.text  = "HOLD";
        text.font  = &scoreFont;
        text.scale = 1.0f;
        text.dirty = true;
        ecsWorld.addComponent(e, text);

        return e;
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

            TextComponent text;
            text.text  = "LEVEL\n1\nLINES\n0000";
            text.font  = &scoreFont;
            text.scale = 0.6f;
            text.dirty = true;
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

            TextComponent text;
            text.text  = "SCORE\n00000000\nBEST\n00000000";
            text.font  = &scoreFont;
            text.scale = 0.6f;
            text.dirty = true;
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
    void onLoad(SceneContext& ctx) override
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

        buildTetrisFrameMesh(ctx);

        // Hold box outline — local offset converts world (0,0,0) to be relative
        // to holdGroupAnchor at (−5, 16, 0): local = (5, −16, 0).
        // Verify: anchor (−5,16) + local (5,−16) = world (0, 0) ✓
        Entity holdBoxEntity = buildHoldBoxMesh(ctx);
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
        installRendererFeature();

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        ecsWorld.addControllerSystem<TetrisCameraControlSystem>();
        // Pass anchors so TetrisGameSystem can parent hold and preview blocks at runtime.
        ecsWorld.addSimulationSystem<TetrisGameSystem>(holdGroupAnchor, nextGroupAnchor);
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
