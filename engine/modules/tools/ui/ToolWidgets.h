#pragma once

#include <string>

#include "BitmapFont.h"
#include "UiHandle.h"
#include "UiInteraction.h"
#include "UiNode.h"

class UiSystem;

namespace gts::tools
{
    struct ToolRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct ToolButton
    {
        UiHandle rect = UI_INVALID_HANDLE;
        UiHandle label = UI_INVALID_HANDLE;
        std::string text;
    };

    struct ToolSlider
    {
        UiHandle label = UI_INVALID_HANDLE;
        UiHandle value = UI_INVALID_HANDLE;
        UiHandle track = UI_INVALID_HANDLE;
        UiHandle fill = UI_INVALID_HANDLE;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool wholeNumber = false;
        std::string name;
    };

    UiColor color(float r, float g, float b, float a = 1.0f);
    UiLayoutSpec fixedLayout(const ToolRect& rect);

    UiHandle createContainer(UiSystem& ui, UiHandle parent, const ToolRect& rect);
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

    ToolButton createButton(UiSystem& ui,
                            UiHandle parent,
                            const ToolRect& rect,
                            BitmapFont* font,
                            const std::string& text,
                            float textScale);
    ToolSlider createSlider(UiSystem& ui,
                            UiHandle parent,
                            float y,
                            const std::string& name,
                            float minValue,
                            float maxValue,
                            bool wholeNumber,
                            BitmapFont* font);

    void setRect(UiSystem& ui, UiHandle handle, const ToolRect& rect);
    void setRectColor(UiSystem& ui, UiHandle handle, UiColor color);
    void setText(UiSystem& ui, UiHandle handle, const std::string& text);
    void setTextColor(UiSystem& ui, UiHandle handle, UiColor color);
    void setVisibleRecursive(UiSystem& ui, UiHandle handle, bool visible);

    bool wasClicked(const UiInteractionResult& interaction, UiHandle handle);
    bool isPressed(const UiInteractionResult& interaction, UiHandle handle);
    void updateButton(UiSystem& ui, const ToolButton& button, const std::string& label);
    void updateSlider(UiSystem& ui,
                      const ToolSlider& slider,
                      float value,
                      UiColor fillColor);
    float valueFromSliderPointer(UiSystem& ui,
                                 const ToolSlider& slider,
                                 float pointerX);
    std::string formatValue(float value, bool wholeNumber);
}
