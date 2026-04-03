#pragma once

#include <string>
#include <variant>
#include <vector>

#include "UiHandle.h"
#include "UiLayout.h"
#include "UiStyle.h"
#include "UiTypes.h"

struct UiContainerData
{
};

struct UiRectData
{
    UiColor color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct UiImageData
{
    std::string imageAsset;
    UiColor     tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    float       imageAspect = 1.0f;
};

struct UiTextData
{
    std::string text;
    std::string fontAsset;
    UiColor     color = {1.0f, 1.0f, 1.0f, 1.0f};
    float       scale = 1.0f;
};

using UiNodePayload = std::variant<UiContainerData, UiRectData, UiImageData, UiTextData>;

struct UiNode
{
    UiHandle              handle   = UI_INVALID_HANDLE;
    UiNodeType            type     = UiNodeType::Container;
    UiHandle              parent   = UI_INVALID_HANDLE;
    std::vector<UiHandle> children;

    UiLayoutSpec          layout;
    UiComputedLayout      computedLayout;
    UiStyle               style;
    UiStateFlags          state;
    UiNodePayload         payload = UiContainerData{};

    bool isVisible() const
    {
        return state.visible;
    }
};
