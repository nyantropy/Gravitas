#include "DungeonUiController.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "BitmapFontLoader.h"
#include "GraphicsConstants.h"
#include "GtsDebugOverlay.h"
#include "UiSystem.h"

namespace
{
    constexpr float UI_EDGE_MARGIN = 0.020f;
    constexpr int ICON_MARGIN_PX = 16;
    constexpr int ICON_SIZE_PX = 160;
    constexpr int ICON_TEXT_GAP_PX = 14;
    constexpr int ICON_TEXT_WIDTH_PX = 220;
    constexpr int ICON_TEXT_HEIGHT_PX = 32;
    constexpr const char* UI_ICON_PATH = "/pictures/Furina_Icon.png";

    constexpr float DEBUG_PANEL_WIDTH = 0.34f;
    constexpr float DEBUG_PANEL_PADDING = 0.016f;
    constexpr float DEBUG_LINE_HEIGHT = 0.030f;
    constexpr float DEBUG_LINE_GAP = 0.008f;

    constexpr float MINIMAP_PANEL_PADDING = 0.020f;
    constexpr float MINIMAP_HEADER_GAP = 0.012f;
    constexpr float MINIMAP_LABEL_HEIGHT = 0.032f;
    constexpr float MINIMAP_MAX_WIDTH = 0.32f;
    constexpr float MINIMAP_MAX_HEIGHT = 0.40f;
    constexpr float MINIMAP_MIN_MARKER_SIZE = 0.007f;

    UiColor uiColor(float r, float g, float b, float a = 1.0f)
    {
        return {r, g, b, a};
    }

    UiColor minimapColorForTile(TileType tileType)
    {
        switch (tileType)
        {
            case TileType::Wall:       return uiColor(0.12f, 0.13f, 0.15f);
            case TileType::Floor:      return uiColor(0.72f, 0.72f, 0.74f);
            case TileType::StairUp:    return uiColor(0.20f, 0.82f, 0.28f);
            case TileType::StairDown:  return uiColor(0.88f, 0.24f, 0.20f);
            case TileType::Treasure:   return uiColor(0.95f, 0.82f, 0.22f);
            case TileType::EnemySpawn: return uiColor(0.64f, 0.34f, 0.82f);
        }

        return uiColor(1.0f, 1.0f, 1.0f);
    }

    float toNormalizedWidth(int pixels, int viewportWidth)
    {
        if (viewportWidth <= 0) return 0.0f;
        return static_cast<float>(pixels) / static_cast<float>(viewportWidth);
    }

    float toNormalizedHeight(int pixels, int viewportHeight)
    {
        if (viewportHeight <= 0) return 0.0f;
        return static_cast<float>(pixels) / static_cast<float>(viewportHeight);
    }
}

void DungeonUiController::reset()
{
    debugRootHandle = UI_INVALID_HANDLE;
    debugBackgroundHandle = UI_INVALID_HANDLE;
    floorTextHandle = UI_INVALID_HANDLE;
    cameraTextHandle = UI_INVALID_HANDLE;
    mapModeTextHandle = UI_INVALID_HANDLE;
    minimapRootHandle = UI_INVALID_HANDLE;
    minimapBackgroundHandle = UI_INVALID_HANDLE;
    minimapGridHandle = UI_INVALID_HANDLE;
    minimapPlayerHandle = UI_INVALID_HANDLE;
    minimapLabelHandle = UI_INVALID_HANDLE;
    iconRootHandle = UI_INVALID_HANDLE;
    iconHandle = UI_INVALID_HANDLE;
    iconTextHandle = UI_INVALID_HANDLE;
    lastMinimapCellCount = 0;
}

uint32_t DungeonUiController::getLastMinimapCellCount() const
{
    return lastMinimapCellCount;
}

void DungeonUiController::initialize(SceneContext& ctx, const DungeonUiState& state)
{
    reset();

    uiFont = BitmapFontLoader::load(
        ctx.resources,
        GtsDebugOverlay::DEBUG_FONT_PATH,
        GtsDebugOverlay::ATLAS_W,  GtsDebugOverlay::ATLAS_H,
        GtsDebugOverlay::CELL_W,   GtsDebugOverlay::CELL_H,
        GtsDebugOverlay::ATLAS_COLS,
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ :/",
        GtsDebugOverlay::LINE_HEIGHT,
        true);

    buildDebugPanel(ctx);
    buildMinimapPanel(ctx);
    buildIcon(ctx);
    update(ctx, state);
}

void DungeonUiController::update(SceneContext& ctx, const DungeonUiState& state)
{
    updateDebugPanel(ctx, state);
    updateMinimapPanel(ctx, state);
    updateIcon(ctx);
}

void DungeonUiController::buildDebugPanel(SceneContext& ctx)
{
    // Dungeon HUD panels use explicit child offsets for all internal spacing.
    // Root containers define outer bounds only so the background rect fills the full panel.
    debugRootHandle = ctx.ui->createNode(UiNodeType::Container);
    debugBackgroundHandle = ctx.ui->createNode(UiNodeType::Rect, debugRootHandle);
    floorTextHandle = ctx.ui->createNode(UiNodeType::Text, debugRootHandle);
    cameraTextHandle = ctx.ui->createNode(UiNodeType::Text, debugRootHandle);
    mapModeTextHandle = ctx.ui->createNode(UiNodeType::Text, debugRootHandle);

    UiLayoutSpec rootLayout;
    rootLayout.positionMode = UiPositionMode::Absolute;
    rootLayout.widthMode = UiSizeMode::Fixed;
    rootLayout.heightMode = UiSizeMode::Fixed;
    rootLayout.offsetMin = {UI_EDGE_MARGIN, UI_EDGE_MARGIN};
    rootLayout.fixedWidth = DEBUG_PANEL_WIDTH;
    rootLayout.fixedHeight = DEBUG_PANEL_PADDING * 2.0f
        + DEBUG_LINE_HEIGHT * 3.0f
        + DEBUG_LINE_GAP * 2.0f;
    ctx.ui->setLayout(debugRootHandle, rootLayout);
    ctx.ui->setState(debugRootHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});

    UiLayoutSpec backgroundLayout;
    backgroundLayout.positionMode = UiPositionMode::Anchored;
    backgroundLayout.widthMode = UiSizeMode::FromAnchors;
    backgroundLayout.heightMode = UiSizeMode::FromAnchors;
    backgroundLayout.anchorMin = {0.0f, 0.0f};
    backgroundLayout.anchorMax = {1.0f, 1.0f};
    ctx.ui->setLayout(debugBackgroundHandle, backgroundLayout);
    ctx.ui->setState(debugBackgroundHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(debugBackgroundHandle, UiRectData{uiColor(0.03f, 0.04f, 0.06f, 0.82f)});

    const float lineOffsets[3] = {
        DEBUG_PANEL_PADDING,
        DEBUG_PANEL_PADDING + DEBUG_LINE_HEIGHT + DEBUG_LINE_GAP,
        DEBUG_PANEL_PADDING + (DEBUG_LINE_HEIGHT + DEBUG_LINE_GAP) * 2.0f
    };
    const UiHandle textHandles[3] = {floorTextHandle, cameraTextHandle, mapModeTextHandle};

    for (int i = 0; i < 3; ++i)
    {
        UiLayoutSpec textLayout;
        textLayout.positionMode = UiPositionMode::Absolute;
        textLayout.widthMode = UiSizeMode::Fixed;
        textLayout.heightMode = UiSizeMode::Fixed;
        textLayout.offsetMin = {DEBUG_PANEL_PADDING, lineOffsets[i]};
        ctx.ui->setLayout(textHandles[i], textLayout);
        ctx.ui->setState(textHandles[i], UiStateFlags{.visible = true, .enabled = false, .interactable = false});
        ctx.ui->setTextFont(textHandles[i], &uiFont);
    }
}

void DungeonUiController::buildMinimapPanel(SceneContext& ctx)
{
    minimapRootHandle = ctx.ui->createNode(UiNodeType::Container);
    minimapBackgroundHandle = ctx.ui->createNode(UiNodeType::Rect, minimapRootHandle);
    minimapGridHandle = ctx.ui->createNode(UiNodeType::Grid, minimapRootHandle);
    minimapPlayerHandle = ctx.ui->createNode(UiNodeType::Rect, minimapRootHandle);
    minimapLabelHandle = ctx.ui->createNode(UiNodeType::Text, minimapRootHandle);

    UiLayoutSpec rootLayout;
    rootLayout.positionMode = UiPositionMode::Absolute;
    rootLayout.widthMode = UiSizeMode::Fixed;
    rootLayout.heightMode = UiSizeMode::Fixed;
    rootLayout.offsetMin = {1.0f - UI_EDGE_MARGIN - MINIMAP_MAX_WIDTH, UI_EDGE_MARGIN};
    rootLayout.fixedWidth = MINIMAP_MAX_WIDTH;
    rootLayout.fixedHeight = MINIMAP_MAX_HEIGHT;
    rootLayout.clipMode = UiClipMode::ClipChildren;
    ctx.ui->setLayout(minimapRootHandle, rootLayout);
    ctx.ui->setState(minimapRootHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});

    UiLayoutSpec backgroundLayout;
    backgroundLayout.positionMode = UiPositionMode::Anchored;
    backgroundLayout.widthMode = UiSizeMode::FromAnchors;
    backgroundLayout.heightMode = UiSizeMode::FromAnchors;
    backgroundLayout.anchorMin = {0.0f, 0.0f};
    backgroundLayout.anchorMax = {1.0f, 1.0f};
    ctx.ui->setLayout(minimapBackgroundHandle, backgroundLayout);
    ctx.ui->setState(minimapBackgroundHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(minimapBackgroundHandle, UiRectData{uiColor(0.05f, 0.06f, 0.08f, 0.88f)});

    UiLayoutSpec gridLayout;
    gridLayout.positionMode = UiPositionMode::Absolute;
    gridLayout.widthMode = UiSizeMode::Fixed;
    gridLayout.heightMode = UiSizeMode::Fixed;
    gridLayout.offsetMin = {MINIMAP_PANEL_PADDING,
                            MINIMAP_PANEL_PADDING + MINIMAP_LABEL_HEIGHT + MINIMAP_HEADER_GAP};
    ctx.ui->setLayout(minimapGridHandle, gridLayout);
    ctx.ui->setState(minimapGridHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});

    UiLayoutSpec playerLayout;
    playerLayout.positionMode = UiPositionMode::Absolute;
    playerLayout.widthMode = UiSizeMode::Fixed;
    playerLayout.heightMode = UiSizeMode::Fixed;
    ctx.ui->setLayout(minimapPlayerHandle, playerLayout);
    ctx.ui->setState(minimapPlayerHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(minimapPlayerHandle, UiRectData{uiColor(0.14f, 0.86f, 1.0f, 1.0f)});

    UiLayoutSpec labelLayout;
    labelLayout.positionMode = UiPositionMode::Absolute;
    labelLayout.widthMode = UiSizeMode::Fixed;
    labelLayout.heightMode = UiSizeMode::Fixed;
    labelLayout.offsetMin = {MINIMAP_PANEL_PADDING, MINIMAP_PANEL_PADDING};
    ctx.ui->setLayout(minimapLabelHandle, labelLayout);
    ctx.ui->setState(minimapLabelHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setTextFont(minimapLabelHandle, &uiFont);
}

void DungeonUiController::buildIcon(SceneContext& ctx)
{
    iconRootHandle = ctx.ui->createNode(UiNodeType::Container);
    iconHandle = ctx.ui->createNode(UiNodeType::Image, iconRootHandle);
    iconTextHandle = ctx.ui->createNode(UiNodeType::Text, iconRootHandle);

    ctx.ui->setState(iconRootHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setState(iconHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(iconHandle, UiImageData{
        GraphicsConstants::ENGINE_RESOURCES + UI_ICON_PATH,
        uiColor(1.0f, 1.0f, 1.0f, 1.0f),
        1.0f
    });

    ctx.ui->setState(iconTextHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setTextFont(iconTextHandle, &uiFont);
    ctx.ui->setPayload(iconTextHandle, UiTextData{
        "NYANIKORE",
        {},
        uiColor(0.97f, 0.98f, 1.0f, 1.0f),
        0.022f
    });
}

void DungeonUiController::updateDebugPanel(SceneContext& ctx, const DungeonUiState& state)
{
    ctx.ui->setPayload(floorTextHandle, UiTextData{
        "FLOOR: " + std::to_string(state.currentFloorIndex + 1)
            + " / " + std::to_string(state.totalFloorCount),
        {},
        uiColor(1.0f, 1.0f, 1.0f, 1.0f),
        GtsDebugOverlay::FONT_SCALE
    });
    ctx.ui->setPayload(cameraTextHandle, UiTextData{
        state.debugCameraActive ? "CAM: DEBUG" : "CAM: PLAYER",
        {},
        uiColor(1.0f, 1.0f, 1.0f, 1.0f),
        GtsDebugOverlay::FONT_SCALE
    });

    const char* minimapMode = state.minimapRevealMode == MinimapRevealMode::FullReveal
        ? "FULL"
        : "EXPLORED";
    ctx.ui->setPayload(mapModeTextHandle, UiTextData{
        std::string("MAP: ") + minimapMode,
        {},
        uiColor(1.0f, 1.0f, 1.0f, 1.0f),
        GtsDebugOverlay::FONT_SCALE
    });
}

void DungeonUiController::updateMinimapPanel(SceneContext& ctx, const DungeonUiState& state)
{
    if (!state.activeFloor || !state.visibility) return;

    const GeneratedFloor& floor = *state.activeFloor;
    if (floor.width <= 0 || floor.height <= 0) return;
    lastMinimapCellCount = static_cast<uint32_t>(floor.width * floor.height);

    const int viewportWidth = std::max(1, ctx.windowPixelWidth);
    const int viewportHeight = std::max(1, ctx.windowPixelHeight);

    const int panelPaddingXPx = std::max(1, static_cast<int>(std::floor(MINIMAP_PANEL_PADDING * viewportWidth)));
    const int panelPaddingYPx = std::max(1, static_cast<int>(std::floor(MINIMAP_PANEL_PADDING * viewportHeight)));
    const int headerGapPx = std::max(1, static_cast<int>(std::floor(MINIMAP_HEADER_GAP * viewportHeight)));
    const int labelHeightPx = std::max(1, static_cast<int>(std::floor(MINIMAP_LABEL_HEIGHT * viewportHeight)));
    const int maxWidthPx = std::max(1, static_cast<int>(std::floor(MINIMAP_MAX_WIDTH * viewportWidth)));
    const int maxHeightPx = std::max(1, static_cast<int>(std::floor(MINIMAP_MAX_HEIGHT * viewportHeight)));

    const int availableGridWidthPx =
        std::max(1, maxWidthPx - panelPaddingXPx * 2);
    const int availableGridHeightPx =
        std::max(1, maxHeightPx - panelPaddingYPx * 2 - labelHeightPx - headerGapPx);
    const int cellSizePx = std::max(
        1,
        static_cast<int>(std::floor(std::min(
            static_cast<float>(availableGridWidthPx) / static_cast<float>(floor.width),
            static_cast<float>(availableGridHeightPx) / static_cast<float>(floor.height)))));

    const int gridWidthPx = floor.width * cellSizePx;
    const int gridHeightPx = floor.height * cellSizePx;
    const int containerWidthPx = gridWidthPx + panelPaddingXPx * 2;
    const int containerHeightPx = labelHeightPx + gridHeightPx + panelPaddingYPx * 2 + headerGapPx;

    const float gridWidth = toNormalizedWidth(gridWidthPx, viewportWidth);
    const float gridHeight = toNormalizedHeight(gridHeightPx, viewportHeight);
    const float containerWidth = toNormalizedWidth(containerWidthPx, viewportWidth);
    const float containerHeight = toNormalizedHeight(containerHeightPx, viewportHeight);

    UiLayoutSpec rootLayout;
    rootLayout.positionMode = UiPositionMode::Absolute;
    rootLayout.widthMode = UiSizeMode::Fixed;
    rootLayout.heightMode = UiSizeMode::Fixed;
    rootLayout.offsetMin = {1.0f - UI_EDGE_MARGIN - containerWidth, UI_EDGE_MARGIN};
    rootLayout.fixedWidth = containerWidth;
    rootLayout.fixedHeight = containerHeight;
    rootLayout.clipMode = UiClipMode::ClipChildren;
    ctx.ui->setLayout(minimapRootHandle, rootLayout);

    UiLayoutSpec gridLayout;
    gridLayout.positionMode = UiPositionMode::Absolute;
    gridLayout.widthMode = UiSizeMode::Fixed;
    gridLayout.heightMode = UiSizeMode::Fixed;
    gridLayout.offsetMin = {
        toNormalizedWidth(panelPaddingXPx, viewportWidth),
        toNormalizedHeight(panelPaddingYPx + labelHeightPx + headerGapPx, viewportHeight)
    };
    gridLayout.fixedWidth = gridWidth;
    gridLayout.fixedHeight = gridHeight;
    ctx.ui->setLayout(minimapGridHandle, gridLayout);

    UiLayoutSpec labelLayout;
    labelLayout.positionMode = UiPositionMode::Absolute;
    labelLayout.widthMode = UiSizeMode::Fixed;
    labelLayout.heightMode = UiSizeMode::Fixed;
    labelLayout.offsetMin = {
        toNormalizedWidth(panelPaddingXPx, viewportWidth),
        toNormalizedHeight(panelPaddingYPx, viewportHeight)
    };
    labelLayout.fixedWidth = gridWidth;
    labelLayout.fixedHeight = toNormalizedHeight(labelHeightPx, viewportHeight);
    ctx.ui->setLayout(minimapLabelHandle, labelLayout);

    UiGridData gridData;
    gridData.columns = floor.width;
    gridData.rows = floor.height;
    gridData.hiddenColor = uiColor(0.0f, 0.0f, 0.0f, 1.0f);
    const int cellInsetPx = std::clamp(static_cast<int>(std::floor(cellSizePx * 0.08f)), 1, std::max(1, cellSizePx / 3));
    gridData.cellInset = std::min(
        toNormalizedWidth(cellInsetPx, viewportWidth),
        toNormalizedHeight(cellInsetPx, viewportHeight));
    gridData.cells.reserve(static_cast<size_t>(floor.width * floor.height));

    for (int z = 0; z < floor.height; ++z)
    {
        for (int x = 0; x < floor.width; ++x)
        {
            gridData.cells.push_back(UiGridCellData{
                minimapColorForTile(floor.get(x, z)),
                state.visibility->isVisible(state.currentFloorIndex, x, z)
            });
        }
    }

    ctx.ui->setPayload(minimapGridHandle, gridData);

    const int markerMinPx = std::max(
        1,
        static_cast<int>(std::floor(std::min(
            MINIMAP_MIN_MARKER_SIZE * viewportWidth,
            MINIMAP_MIN_MARKER_SIZE * viewportHeight))));
    const int markerMaxPx = std::max(1, cellSizePx - cellInsetPx * 2);
    const int markerSizePx = std::clamp(
        static_cast<int>(std::floor(cellSizePx * 0.72f)),
        std::min(markerMinPx, markerMaxPx),
        markerMaxPx);
    const int markerOffsetPx = std::max(0, (cellSizePx - markerSizePx) / 2);

    UiLayoutSpec playerLayout;
    playerLayout.positionMode = UiPositionMode::Absolute;
    playerLayout.widthMode = UiSizeMode::Fixed;
    playerLayout.heightMode = UiSizeMode::Fixed;
    playerLayout.offsetMin = {
        toNormalizedWidth(
            panelPaddingXPx + state.playerTile.x * cellSizePx + markerOffsetPx,
            viewportWidth),
        toNormalizedHeight(
            panelPaddingYPx + labelHeightPx + headerGapPx + state.playerTile.y * cellSizePx + markerOffsetPx,
            viewportHeight)
    };
    playerLayout.fixedWidth = toNormalizedWidth(markerSizePx, viewportWidth);
    playerLayout.fixedHeight = toNormalizedHeight(markerSizePx, viewportHeight);
    ctx.ui->setLayout(minimapPlayerHandle, playerLayout);
    ctx.ui->setState(minimapPlayerHandle, UiStateFlags{
        .visible = state.visibility->isVisible(state.currentFloorIndex, state.playerTile.x, state.playerTile.y),
        .enabled = false,
        .interactable = false
    });

    const char* revealModeText = state.minimapRevealMode == MinimapRevealMode::FullReveal
        ? "FULL"
        : "FOG";
    ctx.ui->setPayload(minimapLabelHandle, UiTextData{
        "MAP F" + std::to_string(state.currentFloorIndex + 1) + " " + revealModeText,
        {},
        uiColor(0.95f, 0.96f, 1.0f, 1.0f),
        0.020f
    });
}

void DungeonUiController::updateIcon(SceneContext& ctx)
{
    const int viewportWidth = std::max(1, ctx.windowPixelWidth);
    const int viewportHeight = std::max(1, ctx.windowPixelHeight);
    const float iconWidth = toNormalizedWidth(ICON_SIZE_PX, viewportWidth);
    const float iconHeight = toNormalizedHeight(ICON_SIZE_PX, viewportHeight);
    const float textGap = toNormalizedWidth(ICON_TEXT_GAP_PX, viewportWidth);
    const float textWidth = toNormalizedWidth(ICON_TEXT_WIDTH_PX, viewportWidth);
    const float textHeight = toNormalizedHeight(ICON_TEXT_HEIGHT_PX, viewportHeight);
    const float rootWidth = iconWidth + textGap + textWidth;
    const float rootHeight = std::max(iconHeight, textHeight);

    UiLayoutSpec rootLayout;
    rootLayout.positionMode = UiPositionMode::Absolute;
    rootLayout.widthMode = UiSizeMode::Fixed;
    rootLayout.heightMode = UiSizeMode::Fixed;
    rootLayout.offsetMin = {
        toNormalizedWidth(ICON_MARGIN_PX, viewportWidth),
        1.0f - rootHeight - toNormalizedHeight(ICON_MARGIN_PX, viewportHeight)
    };
    rootLayout.fixedWidth = rootWidth;
    rootLayout.fixedHeight = rootHeight;
    ctx.ui->setLayout(iconRootHandle, rootLayout);

    UiLayoutSpec iconLayout;
    iconLayout.positionMode = UiPositionMode::Absolute;
    iconLayout.widthMode = UiSizeMode::Fixed;
    iconLayout.heightMode = UiSizeMode::Fixed;
    iconLayout.offsetMin = {0.0f, 0.0f};
    iconLayout.fixedWidth = iconWidth;
    iconLayout.fixedHeight = iconHeight;
    ctx.ui->setLayout(iconHandle, iconLayout);

    UiLayoutSpec textLayout;
    textLayout.positionMode = UiPositionMode::Absolute;
    textLayout.widthMode = UiSizeMode::Fixed;
    textLayout.heightMode = UiSizeMode::Fixed;
    textLayout.offsetMin = {
        iconWidth + textGap,
        (rootHeight - textHeight) * 0.5f
    };
    textLayout.fixedWidth = textWidth;
    textLayout.fixedHeight = textHeight;
    ctx.ui->setLayout(iconTextHandle, textLayout);
}
