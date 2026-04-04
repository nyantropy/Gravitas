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
    bool operator==(const UiContainerData&) const = default;
};

struct UiRectData
{
    UiColor color = {1.0f, 1.0f, 1.0f, 1.0f};

    bool operator==(const UiRectData&) const = default;
};

struct UiImageData
{
    std::string imageAsset;
    UiColor     tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    float       imageAspect = 1.0f;

    bool operator==(const UiImageData&) const = default;
};

struct UiTextData
{
    std::string text;
    std::string fontAsset;
    UiColor     color = {1.0f, 1.0f, 1.0f, 1.0f};
    float       scale = 1.0f;

    bool operator==(const UiTextData&) const = default;
};

struct UiGridCellData
{
    UiColor color   = {0.0f, 0.0f, 0.0f, 1.0f};
    bool    visible = true;

    bool operator==(const UiGridCellData&) const = default;
};

struct UiGridData
{
    int                       columns       = 0;
    int                       rows          = 0;
    std::vector<UiGridCellData> cells;
    UiColor                   hiddenColor   = {0.0f, 0.0f, 0.0f, 1.0f};
    float                     cellInset     = 0.0f;

    bool operator==(const UiGridData&) const = default;

    const UiGridCellData* cellAt(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= columns || y >= rows) return nullptr;
        const size_t index = static_cast<size_t>(y * columns + x);
        if (index >= cells.size()) return nullptr;
        return &cells[index];
    }
};

using UiNodePayload = std::variant<UiContainerData, UiRectData, UiImageData, UiTextData, UiGridData>;

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
