#pragma once

#include "UiCompositionTypes.h"
#include "UiHandle.h"
#include "UiInteraction.h"
#include "UiLayer.h"
#include "UiMountTypes.h"

namespace gts::vn
{
    struct VNFrontendStateComponent
    {
        bool mounted = false;
        UiSurfaceId surface = UI_DEFAULT_SURFACE;
        UiCompositionId composition = UI_INVALID_COMPOSITION;
        UiLayerId modalLayer = UI_INVALID_LAYER;
        UiMountId modalSlotMount = UI_INVALID_MOUNT;
        UiHandle modalSlotRoot = UI_INVALID_HANDLE;
    };
} // namespace gts::vn
