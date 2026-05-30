#include "ToolWidgets.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <variant>
#include <vector>

#include "UiSystem.h"

namespace gts::tools
{
    namespace
    {
        constexpr float SliderLabelX = 0.014f;
        constexpr float SliderTrackX = 0.132f;
        constexpr float SliderTrackW = 0.160f;
        constexpr float SliderValueX = 0.304f;
        constexpr float SliderH = 0.026f;
        constexpr float SliderTrackH = 0.012f;
    }

    UiColor color(float r, float g, float b, float a)
    {
        return {r, g, b, a};
    }

    UiLayoutSpec fixedLayout(const ToolRect& rect)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Absolute;
        layout.widthMode = UiSizeMode::Fixed;
        layout.heightMode = UiSizeMode::Fixed;
        layout.offsetMin = {rect.x, rect.y};
        layout.fixedWidth = rect.width;
        layout.fixedHeight = rect.height;
        return layout;
    }

    UiHandle createContainer(UiSystem& ui, UiHandle parent, const ToolRect& rect)
    {
        UiHandle handle = ui.createNode(UiNodeType::Container, parent);
        ui.setLayout(handle, fixedLayout(rect));
        ui.setState(handle, UiStateFlags{
            .visible = true,
            .enabled = true,
            .interactable = false
        });
        return handle;
    }

    UiHandle createRect(UiSystem& ui,
                        UiHandle parent,
                        const ToolRect& rect,
                        UiColor inColor,
                        bool interactable)
    {
        UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, fixedLayout(rect));
        ui.setState(handle, UiStateFlags{
            .visible = true,
            .enabled = true,
            .interactable = interactable
        });
        ui.setPayload(handle, UiRectData{inColor});
        return handle;
    }

    UiHandle createText(UiSystem& ui,
                        UiHandle parent,
                        const ToolRect& rect,
                        BitmapFont* font,
                        const std::string& text,
                        UiColor inColor,
                        float scale)
    {
        UiHandle handle = ui.createNode(UiNodeType::Text, parent);
        ui.setLayout(handle, fixedLayout(rect));
        ui.setState(handle, UiStateFlags{
            .visible = true,
            .enabled = false,
            .interactable = false
        });
        ui.setPayload(handle, UiTextData{text, {}, inColor, scale});
        ui.setTextFont(handle, font);
        return handle;
    }

    ToolButton createButton(UiSystem& ui,
                            UiHandle parent,
                            const ToolRect& rect,
                            BitmapFont* font,
                            const std::string& text,
                            float textScale)
    {
        ToolButton button;
        button.text = text;
        button.rect = createRect(ui, parent, rect, color(0.105f, 0.125f, 0.145f, 0.95f), true);
        button.label = createText(ui,
                                  button.rect,
                                  {0.007f, 0.008f, std::max(0.0f, rect.width - 0.014f), 0.020f},
                                  font,
                                  text,
                                  color(0.90f, 0.94f, 0.98f),
                                  textScale);
        return button;
    }

    ToolSlider createSlider(UiSystem& ui,
                            UiHandle parent,
                            float y,
                            const std::string& name,
                            float minValue,
                            float maxValue,
                            bool wholeNumber,
                            BitmapFont* font)
    {
        ToolSlider slider;
        slider.minValue = minValue;
        slider.maxValue = maxValue;
        slider.wholeNumber = wholeNumber;
        slider.name = name;
        slider.label = createText(ui, parent, {SliderLabelX, y, 0.112f, SliderH}, font,
                                  name, color(0.72f, 0.79f, 0.85f), 0.0125f);
        slider.value = createText(ui, parent, {SliderValueX, y, 0.072f, SliderH}, font,
                                  "0", color(0.90f, 0.94f, 0.98f), 0.0125f);
        slider.track = createRect(ui, parent, {SliderTrackX, y + 0.008f, SliderTrackW, SliderTrackH},
                                  color(0.080f, 0.095f, 0.110f, 1.0f), true);
        slider.fill = createRect(ui, slider.track, {0.0f, 0.0f, 0.001f, SliderTrackH},
                                 color(0.25f, 0.62f, 0.92f, 1.0f));
        return slider;
    }

    void setRect(UiSystem& ui, UiHandle handle, const ToolRect& rect)
    {
        ui.setLayout(handle, fixedLayout(rect));
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
        data.text = text;
        ui.setPayload(handle, data);
    }

    void setTextColor(UiSystem& ui, UiHandle handle, UiColor inColor)
    {
        const UiNode* node = ui.findNode(handle);
        if (node == nullptr || node->type != UiNodeType::Text)
            return;

        UiTextData data = std::get<UiTextData>(node->payload);
        data.color = inColor;
        ui.setPayload(handle, data);
    }

    void setVisibleRecursive(UiSystem& ui, UiHandle handle, bool visible)
    {
        UiNode* node = ui.findNode(handle);
        if (node == nullptr)
            return;

        UiStateFlags state = node->state;
        state.visible = visible;
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
        const UiNode* node = ui.findNode(button.rect);
        const bool hovered = node != nullptr && node->state.hovered;
        const bool pressed = node != nullptr && node->state.pressed;
        const UiColor buttonColor = pressed
            ? color(0.19f, 0.28f, 0.34f, 1.0f)
            : (hovered ? color(0.14f, 0.19f, 0.23f, 1.0f) : color(0.105f, 0.125f, 0.145f, 0.95f));
        setRectColor(ui, button.rect, buttonColor);
        setText(ui, button.label, label);
    }

    void updateSlider(UiSystem& ui,
                      const ToolSlider& slider,
                      float value,
                      UiColor fillColor)
    {
        const float t = slider.maxValue <= slider.minValue
            ? 0.0f
            : std::clamp((value - slider.minValue) / (slider.maxValue - slider.minValue), 0.0f, 1.0f);
        setRect(ui, slider.fill, {0.0f, 0.0f, SliderTrackW * t, SliderTrackH});
        setRectColor(ui, slider.fill, fillColor);
        setText(ui, slider.value, formatValue(value, slider.wholeNumber));
    }

    float valueFromSliderPointer(UiSystem& ui,
                                 const ToolSlider& slider,
                                 float pointerX)
    {
        const UiNode* node = ui.findNode(slider.track);
        if (node == nullptr)
            return slider.minValue;

        const UiRect& bounds = node->computedLayout.bounds;
        const float t = bounds.width <= 0.0f
            ? 0.0f
            : std::clamp((pointerX - bounds.x) / bounds.width, 0.0f, 1.0f);
        float value = slider.minValue + (slider.maxValue - slider.minValue) * t;
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
}
