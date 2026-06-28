#pragma once

#include <string>

#include "BitmapFont.h"
#include "UiHandle.h"
#include "UiInteraction.h"
#include "UiNode.h"
#include "UiTypes.h"

class UiSystem;

namespace gts::tools
{
    struct ToolRect
    {
        float x      = 0.0f;
        float y      = 0.0f;
        float width  = 0.0f;
        float height = 0.0f;
    };

    struct ToolButton
    {
        UiHandle    rect  = UI_INVALID_HANDLE;
        UiHandle    label = UI_INVALID_HANDLE;
        std::string text;
    };

    struct ToolSlider
    {
        UiHandle    label       = UI_INVALID_HANDLE;
        UiHandle    value       = UI_INVALID_HANDLE;
        UiHandle    track       = UI_INVALID_HANDLE;
        UiHandle    fill        = UI_INVALID_HANDLE;
        float       minValue    = 0.0f;
        float       maxValue    = 1.0f;
        bool        wholeNumber = false;
        std::string name;
    };

    struct ToolTextField
    {
        UiHandle    label = UI_INVALID_HANDLE;
        UiHandle    rect  = UI_INVALID_HANDLE;
        UiHandle    value = UI_INVALID_HANDLE;
        std::string name;
    };

    struct ToolPanelFrame
    {
        UiHandle background = UI_INVALID_HANDLE;
        UiHandle accent     = UI_INVALID_HANDLE;
        UiHandle title      = UI_INVALID_HANDLE;
        UiHandle subtitle   = UI_INVALID_HANDLE;
    };

    struct ToolSectionHeader
    {
        UiHandle rect    = UI_INVALID_HANDLE;
        UiHandle label   = UI_INVALID_HANDLE;
        UiHandle summary = UI_INVALID_HANDLE;
    };

    struct ToolStatusRow
    {
        UiHandle label = UI_INVALID_HANDLE;
        UiHandle value = UI_INVALID_HANDLE;
    };

    UiColor      color(float r, float g, float b, float a = 1.0f);
    UiLayoutSpec fixedLayout(const ToolRect& rect);
    UiLayoutSpec relativeLayout(const ToolRect& rect);

    UiHandle createContainer(UiSystem& ui, UiHandle parent, const ToolRect& rect);
    UiHandle createContainerRelative(UiSystem& ui, UiHandle parent, const ToolRect& rect);
    UiHandle createRect(UiSystem& ui, UiHandle parent, const ToolRect& rect, UiColor color, bool interactable = false);
    UiHandle
    createRectRelative(UiSystem& ui, UiHandle parent, const ToolRect& rect, UiColor color, bool interactable = false);
    UiHandle createImageRelative(UiSystem& ui,
                                 UiHandle parent,
                                 const ToolRect& rect,
                                 const UiImageData& image = {},
                                 bool interactable = false);
    UiHandle createLineRelative(UiSystem& ui,
                                UiHandle parent,
                                UiVec2 start,
                                UiVec2 end,
                                UiColor color,
                                float thickness = 0.002f);
    UiHandle createCircleRelative(UiSystem& ui,
                                  UiHandle parent,
                                  const ToolRect& rect,
                                  UiColor color,
                                  int segments = 24,
                                  bool interactable = false);
    UiHandle createText(UiSystem&          ui,
                        UiHandle           parent,
                        const ToolRect&    rect,
                        BitmapFont*        font,
                        const std::string& text,
                        UiColor            color,
                        float              scale);
    UiHandle createTextRelative(UiSystem&          ui,
                                UiHandle           parent,
                                const ToolRect&    rect,
                                BitmapFont*        font,
                                const std::string& text,
                                UiColor            color,
                                float              scale);

    ToolButton createButton(UiSystem&          ui,
                            UiHandle           parent,
                            const ToolRect&    rect,
                            BitmapFont*        font,
                            const std::string& text,
                            float              textScale);
    ToolButton createButtonRelative(UiSystem&          ui,
                                    UiHandle           parent,
                                    const ToolRect&    rect,
                                    BitmapFont*        font,
                                    const std::string& text,
                                    float              textScale);
    ToolSlider createSlider(UiSystem&          ui,
                            UiHandle           parent,
                            float              y,
                            const std::string& name,
                            float              minValue,
                            float              maxValue,
                            bool               wholeNumber,
                            BitmapFont*        font);
    ToolTextField createTextField(UiSystem&          ui,
                                  UiHandle           parent,
                                  float              y,
                                  const std::string& name,
                                  BitmapFont*        font);
    ToolTextField createSearchFieldRelative(UiSystem&          ui,
                                            UiHandle           parent,
                                            const ToolRect&    rect,
                                            const std::string& name,
                                            BitmapFont*        font);
    ToolPanelFrame createPanelFrameRelative(UiSystem&          ui,
                                            UiHandle           parent,
                                            const ToolRect&    rect,
                                            BitmapFont*        font,
                                            const std::string& title,
                                            const std::string& subtitle);
    ToolSectionHeader createSectionHeaderRelative(UiSystem&          ui,
                                                  UiHandle           parent,
                                                  const ToolRect&    rect,
                                                  BitmapFont*        font,
                                                  const std::string& label,
                                                  const std::string& summary);
    ToolStatusRow createStatusRowRelative(UiSystem&          ui,
                                          UiHandle           parent,
                                          const ToolRect&    rect,
                                          BitmapFont*        font,
                                          const std::string& label,
                                          const std::string& value);

    void setRect(UiSystem& ui, UiHandle handle, const ToolRect& rect);
    void setRelativeRect(UiSystem& ui, UiHandle handle, const ToolRect& rect);
    void setRectColor(UiSystem& ui, UiHandle handle, UiColor color);
    void setText(UiSystem& ui, UiHandle handle, const std::string& text);
    void setTextColor(UiSystem& ui, UiHandle handle, UiColor color);
    void
    setTextAlignment(UiSystem& ui, UiHandle handle, UiHorizontalAlign horizontalAlign, UiVerticalAlign verticalAlign);
    void setVisibleRecursive(UiSystem& ui, UiHandle handle, bool visible);

    bool        wasClicked(const UiInteractionResult& interaction, UiHandle handle);
    bool        isPressed(const UiInteractionResult& interaction, UiHandle handle);
    void        updateButton(UiSystem& ui, const ToolButton& button, const std::string& label);
    void        updateToggleButton(UiSystem& ui, const ToolButton& button, const std::string& label, bool enabled);
    void        updateSlider(UiSystem& ui, const ToolSlider& slider, float value, UiColor fillColor);
    void        updateTextField(UiSystem& ui, const ToolTextField& field, const std::string& value, bool focused);
    void        updatePanelFrame(UiSystem& ui, const ToolPanelFrame& frame, const std::string& title, const std::string& subtitle);
    void        updateSectionHeader(UiSystem& ui, const ToolSectionHeader& header, const std::string& label, const std::string& summary, bool active);
    void        updateStatusRow(UiSystem& ui, const ToolStatusRow& row, const std::string& value);
    float       valueFromSliderPointer(UiSystem& ui, const ToolSlider& slider, float pointerX);
    std::string formatValue(float value, bool wholeNumber);
} // namespace gts::tools
