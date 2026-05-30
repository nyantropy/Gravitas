#pragma once

#include <string>

#include "BitmapFont.h"
#include "UiHandle.h"
#include "UiNode.h"

class UiSystem;

namespace gts::ui
{
    struct ToolRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    UiColor color(float r, float g, float b, float a = 1.0f);
    UiLayoutSpec fixedLayout(const ToolRect& rect);

    UiHandle createRect(UiSystem& ui,
                        UiHandle parent,
                        const ToolRect& rect,
                        UiColor color,
                        bool interactable = false);

    UiHandle createText(UiSystem& ui,
                        UiHandle parent,
                        const ToolRect& rect,
                        BitmapFont* font,
                        const std::string& text,
                        UiColor color,
                        float scale);

    void setRect(UiSystem& ui, UiHandle handle, const ToolRect& rect);
    void setRectColor(UiSystem& ui, UiHandle handle, UiColor color);
    void setText(UiSystem& ui, UiHandle handle, const std::string& text);
    void setTextColor(UiSystem& ui, UiHandle handle, UiColor color);
}
