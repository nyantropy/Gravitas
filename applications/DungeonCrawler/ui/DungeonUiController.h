#pragma once

#include "BitmapFont.h"
#include "SceneContext.h"
#include "UiHandle.h"

#include "DungeonUiState.h"

class DungeonUiController
{
public:
    void reset();
    void initialize(SceneContext& ctx, const DungeonUiState& state);
    void update(SceneContext& ctx, const DungeonUiState& state);

private:
    void buildDebugPanel(SceneContext& ctx);
    void buildMinimapPanel(SceneContext& ctx);
    void updateDebugPanel(SceneContext& ctx, const DungeonUiState& state);
    void updateMinimapPanel(SceneContext& ctx, const DungeonUiState& state);

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
};
