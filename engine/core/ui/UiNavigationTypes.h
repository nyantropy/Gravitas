#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "UiHandle.h"
#include "UiInteraction.h"

enum class UiNavigationRole : uint8_t
{
    Generic = 0,
    Button,
    Checkbox,
    Slider,
    List,
    Tree,
    TextBox,
    Menu,
    Tab,
    Window,
    Toolbar,
    Graph,
    Viewport
};

static constexpr int UI_NAVIGATION_AUTO_TAB_INDEX = -1;
static constexpr size_t UI_NAVIGATION_DIRECTION_COUNT = 7;

inline size_t uiNavigationDirectionIndex(UiNavigationDirection direction)
{
    return static_cast<size_t>(direction);
}

struct UiNavigationNodeDesc
{
    bool focusable = true;
    bool enabled = true;
    UiNavigationRole role = UiNavigationRole::Generic;
    uint32_t scope = 0;
    std::string group;
    int tabIndex = UI_NAVIGATION_AUTO_TAB_INDEX;
    bool wrapNavigation = false;
    bool activateOnSubmit = true;
    std::array<UiHandle, UI_NAVIGATION_DIRECTION_COUNT> neighbors = {
        UI_INVALID_HANDLE,
        UI_INVALID_HANDLE,
        UI_INVALID_HANDLE,
        UI_INVALID_HANDLE,
        UI_INVALID_HANDLE,
        UI_INVALID_HANDLE,
        UI_INVALID_HANDLE
    };
};

struct UiNavigationMoveResult
{
    bool requested = false;
    bool moved = false;
    bool blocked = false;
    bool wrapped = false;
    UiNavigationDirection direction = UiNavigationDirection::None;
    UiInputDeviceId deviceId = UI_PRIMARY_INPUT_DEVICE;
    UiHandle from = UI_INVALID_HANDLE;
    UiHandle to = UI_INVALID_HANDLE;
};

struct UiNavigationSubmitResult
{
    bool requested = false;
    bool submitted = false;
    bool blocked = false;
    UiInputDeviceId deviceId = UI_PRIMARY_INPUT_DEVICE;
    UiHandle target = UI_INVALID_HANDLE;
};
