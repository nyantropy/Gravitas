#pragma once

#include "BitmapFont.h"
#include "SceneContext.h"
#include "UiHandle.h"

#include "ui/DungeonUiState.h"

class DungeonUiController
{
public:
    void reset();
    void initialize(SceneContext& ctx, const DungeonUiState& state);
    void update(SceneContext& ctx, const DungeonUiState& state);
    uint32_t getLastMinimapCellCount() const;

private:
    void buildDebugPanel(SceneContext& ctx);
    void buildMinimapPanel(SceneContext& ctx);
    void buildIcon(SceneContext& ctx);
    void updateDebugPanel(SceneContext& ctx, const DungeonUiState& state);
    void updateMinimapPanel(SceneContext& ctx, const DungeonUiState& state);
    void updateIcon(SceneContext& ctx);

    BitmapFont uiFont;

    UiHandle debugRootHandle = UI_INVALID_HANDLE;
    UiHandle debugBackgroundHandle = UI_INVALID_HANDLE;
    UiHandle floorTextHandle = UI_INVALID_HANDLE;
    UiHandle cameraTextHandle = UI_INVALID_HANDLE;
    UiHandle mapModeTextHandle = UI_INVALID_HANDLE;

    UiHandle minimapRootHandle = UI_INVALID_HANDLE;
    UiHandle minimapBackgroundHandle = UI_INVALID_HANDLE;
    UiHandle minimapGridHandle = UI_INVALID_HANDLE;
    UiHandle minimapPlayerHandle = UI_INVALID_HANDLE;
    UiHandle minimapLabelHandle = UI_INVALID_HANDLE;
    UiHandle iconRootHandle = UI_INVALID_HANDLE;
    UiHandle iconHandle = UI_INVALID_HANDLE;
    UiHandle iconTextHandle = UI_INVALID_HANDLE;
    uint32_t lastMinimapCellCount = 0;
};
