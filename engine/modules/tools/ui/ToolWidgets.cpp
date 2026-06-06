#include "ToolWidgets.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <variant>
#include <vector>

#include "ToolTheme.h"
#include "UiSystem.h"

namespace gts::tools
{
    namespace
    {
        constexpr float SliderLabelX = 0.000f;
        constexpr float SliderTrackX = 0.390f;
        constexpr float SliderTrackW = 0.430f;
        constexpr float SliderValueX = 0.845f;
        constexpr float SliderValueW = 0.155f;
        constexpr float SliderH      = ToolTheme::sliderHeight;
        constexpr float SliderTrackY = 0.310f;
        constexpr float SliderTrackH = 0.350f;

        UiHandle createNodeWithState(
            UiSystem& ui, UiNodeType type, UiHandle parent, const UiLayoutSpec& layout, bool interactable)
        {
            UiHandle handle = ui.createNode(type, parent);
            ui.setLayout(handle, layout);
            ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = interactable});
            return handle;
        }
    } // namespace

    UiColor color(float r, float g, float b, float a)
    {
        return {r, g, b, a};
    }

    UiLayoutSpec fixedLayout(const ToolRect& rect)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Absolute;
        layout.widthMode    = UiSizeMode::Fixed;
        layout.heightMode   = UiSizeMode::Fixed;
        layout.offsetMin    = {rect.x, rect.y};
        layout.fixedWidth   = rect.width;
        layout.fixedHeight  = rect.height;
        return layout;
    }

    UiLayoutSpec relativeLayout(const ToolRect& rect)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode    = UiSizeMode::FromAnchors;
        layout.heightMode   = UiSizeMode::FromAnchors;
        layout.anchorMin    = {rect.x, rect.y};
        layout.anchorMax    = {rect.x + rect.width, rect.y + rect.height};
        return layout;
    }

    UiHandle createContainer(UiSystem& ui, UiHandle parent, const ToolRect& rect)
    {
        return createNodeWithState(ui, UiNodeType::Container, parent, fixedLayout(rect), false);
    }

    UiHandle createContainerRelative(UiSystem& ui, UiHandle parent, const ToolRect& rect)
    {
        return createNodeWithState(ui, UiNodeType::Container, parent, relativeLayout(rect), false);
    }

    UiHandle createRect(UiSystem& ui, UiHandle parent, const ToolRect& rect, UiColor inColor, bool interactable)
    {
        UiHandle handle = createNodeWithState(ui, UiNodeType::Rect, parent, fixedLayout(rect), interactable);
        ui.setPayload(handle, UiRectData{inColor});
        return handle;
    }

    UiHandle createRectRelative(UiSystem& ui, UiHandle parent, const ToolRect& rect, UiColor inColor, bool interactable)
    {
        UiHandle handle = createNodeWithState(ui, UiNodeType::Rect, parent, relativeLayout(rect), interactable);
        ui.setPayload(handle, UiRectData{inColor});
        return handle;
    }

    UiHandle createText(UiSystem&          ui,
                        UiHandle           parent,
                        const ToolRect&    rect,
                        BitmapFont*        font,
                        const std::string& text,
                        UiColor            inColor,
                        float              scale)
    {
        UiHandle     handle = createNodeWithState(ui, UiNodeType::Text, parent, fixedLayout(rect), false);
        UiStateFlags state  = ui.findNode(handle)->state;
        state.enabled       = false;
        ui.setState(handle, state);
        ui.setPayload(handle, UiTextData{text, {}, inColor, scale});
        ui.setTextFont(handle, font);
        return handle;
    }

    UiHandle createTextRelative(UiSystem&          ui,
                                UiHandle           parent,
                                const ToolRect&    rect,
                                BitmapFont*        font,
                                const std::string& text,
                                UiColor            inColor,
                                float              scale)
    {
        UiHandle     handle = createNodeWithState(ui, UiNodeType::Text, parent, relativeLayout(rect), false);
        UiStateFlags state  = ui.findNode(handle)->state;
        state.enabled       = false;
        ui.setState(handle, state);
        ui.setPayload(handle, UiTextData{text, {}, inColor, scale});
        ui.setTextFont(handle, font);
        return handle;
    }

    ToolButton createButton(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, BitmapFont* font, const std::string& text, float textScale)
    {
        ToolButton button;
        button.text = text;
        button.rect = createRect(ui, parent, rect, ToolTheme::button, true);
        button.label =
            createText(ui,
                       button.rect,
                       {0.004f, 0.004f, std::max(0.0f, rect.width - 0.008f), std::max(0.0f, rect.height - 0.008f)},
                       font,
                       text,
                       ToolTheme::text,
                       textScale);
        setTextAlignment(ui, button.label, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
        return button;
    }

    ToolButton createButtonRelative(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, BitmapFont* font, const std::string& text, float textScale)
    {
        ToolButton button;
        button.text  = text;
        button.rect  = createRectRelative(ui, parent, rect, ToolTheme::button, true);
        button.label = createTextRelative(
            ui, button.rect, {0.040f, 0.120f, 0.920f, 0.760f}, font, text, ToolTheme::text, textScale);
        setTextAlignment(ui, button.label, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
        return button;
    }

    ToolSlider createSlider(UiSystem&          ui,
                            UiHandle           parent,
                            float              y,
                            const std::string& name,
                            float              minValue,
                            float              maxValue,
                            bool               wholeNumber,
                            BitmapFont*        font)
    {
        ToolSlider slider;
        slider.minValue    = minValue;
        slider.maxValue    = maxValue;
        slider.wholeNumber = wholeNumber;
        slider.name        = name;
        slider.label       = createTextRelative(ui,
                                                parent,
                                                {SliderLabelX, y, 0.360f, SliderH},
                                                font,
                                                name,
                                                ToolTheme::mutedText,
                                                ToolTheme::smallTextScale);
        slider.value       = createTextRelative(ui,
                                                parent,
                                                {SliderValueX, y, SliderValueW, SliderH},
                                                font,
                                                "0",
                                                ToolTheme::text,
                                                ToolTheme::smallTextScale);
        setTextAlignment(ui, slider.value, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
        slider.track =
            createRectRelative(ui,
                               parent,
                               {SliderTrackX, y + SliderTrackY * SliderH, SliderTrackW, SliderTrackH * SliderH},
                               color(0.055f, 0.063f, 0.071f, 1.0f),
                               true);
        slider.fill = createRectRelative(ui, slider.track, {0.0f, 0.0f, 0.001f, 1.0f}, ToolTheme::accent);
        return slider;
    }

    void setRect(UiSystem& ui, UiHandle handle, const ToolRect& rect)
    {
        ui.setLayout(handle, fixedLayout(rect));
    }

    void setRelativeRect(UiSystem& ui, UiHandle handle, const ToolRect& rect)
    {
        ui.setLayout(handle, relativeLayout(rect));
    }

    void setRectColor(UiSystem& ui, UiHandle handle, UiColor inColor)
    {
        ui.setPayload(handle, UiRectData{inColor});
    }

    void setText(UiSystem& ui, UiHandle handle, const std::string& text)
    {
        const UiNode* node = ui.findNode(handle);
        if (node == nullptr || node->type != UiNodeType::Text)
            return;

        UiTextData data = std::get<UiTextData>(node->payload);
        data.text       = text;
        ui.setPayload(handle, data);
    }

    void setTextColor(UiSystem& ui, UiHandle handle, UiColor inColor)
    {
        const UiNode* node = ui.findNode(handle);
        if (node == nullptr || node->type != UiNodeType::Text)
            return;

        UiTextData data = std::get<UiTextData>(node->payload);
        data.color      = inColor;
        ui.setPayload(handle, data);
    }

    void
    setTextAlignment(UiSystem& ui, UiHandle handle, UiHorizontalAlign horizontalAlign, UiVerticalAlign verticalAlign)
    {
        const UiNode* node = ui.findNode(handle);
        if (node == nullptr || node->type != UiNodeType::Text)
            return;

        UiTextData data      = std::get<UiTextData>(node->payload);
        data.horizontalAlign = horizontalAlign;
        data.verticalAlign   = verticalAlign;
        ui.setPayload(handle, data);
    }

    void setVisibleRecursive(UiSystem& ui, UiHandle handle, bool visible)
    {
        UiNode* node = ui.findNode(handle);
        if (node == nullptr)
            return;

        UiStateFlags state = node->state;
        state.visible      = visible;
        ui.setState(handle, state);

        const std::vector<UiHandle> children = node->children;
        for (UiHandle child : children)
            setVisibleRecursive(ui, child, visible);
    }

    bool wasClicked(const UiInteractionResult& interaction, UiHandle handle)
    {
        return handle != UI_INVALID_HANDLE && interaction.clicked == handle;
    }

    bool isPressed(const UiInteractionResult& interaction, UiHandle handle)
    {
        return handle != UI_INVALID_HANDLE && interaction.pressed == handle;
    }

    void updateButton(UiSystem& ui, const ToolButton& button, const std::string& label)
    {
        const UiNode* node    = ui.findNode(button.rect);
        const bool    hovered = node != nullptr && node->state.hovered;
        const bool    pressed = node != nullptr && node->state.pressed;
        const UiColor buttonColor =
            pressed ? ToolTheme::buttonPressed : (hovered ? ToolTheme::buttonHover : ToolTheme::button);
        setRectColor(ui, button.rect, buttonColor);
        setText(ui, button.label, label);
    }

    void updateToggleButton(UiSystem& ui, const ToolButton& button, const std::string& label, bool enabled)
    {
        const UiNode* node        = ui.findNode(button.rect);
        const bool    hovered     = node != nullptr && node->state.hovered;
        const bool    pressed     = node != nullptr && node->state.pressed;
        UiColor       buttonColor = enabled ? ToolTheme::toggleActive : ToolTheme::button;
        if (hovered)
            buttonColor = enabled ? color(0.145f, 0.260f, 0.195f, 1.0f) : ToolTheme::buttonHover;
        if (pressed)
            buttonColor = ToolTheme::buttonPressed;

        setRectColor(ui, button.rect, buttonColor);
        setText(ui, button.label, label + (enabled ? " ON" : " OFF"));
    }

    void updateSlider(UiSystem& ui, const ToolSlider& slider, float value, UiColor fillColor)
    {
        const float t = slider.maxValue <= slider.minValue
                            ? 0.0f
                            : std::clamp((value - slider.minValue) / (slider.maxValue - slider.minValue), 0.0f, 1.0f);
        setRelativeRect(ui, slider.fill, {0.0f, 0.0f, t, 1.0f});
        setRectColor(ui, slider.fill, fillColor);
        setText(ui, slider.value, formatValue(value, slider.wholeNumber));
    }

    float valueFromSliderPointer(UiSystem& ui, const ToolSlider& slider, float pointerX)
    {
        const UiNode* node = ui.findNode(slider.track);
        if (node == nullptr)
            return slider.minValue;

        const UiRect& bounds = node->computedLayout.bounds;
        const float   t = bounds.width <= 0.0f ? 0.0f : std::clamp((pointerX - bounds.x) / bounds.width, 0.0f, 1.0f);
        float         value = slider.minValue + (slider.maxValue - slider.minValue) * t;
        if (slider.wholeNumber)
            value = std::round(value);
        return value;
    }

    std::string formatValue(float value, bool wholeNumber)
    {
        std::ostringstream out;
        if (wholeNumber)
        {
            out << static_cast<int>(std::round(value));
        }
        else
        {
            out << std::fixed << std::setprecision(value >= 10.0f ? 1 : 2) << value;
        }
        return out.str();
    }
} // namespace gts::tools
