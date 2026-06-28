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
        constexpr float SliderTrackX = 0.385f;
        constexpr float SliderTrackW = 0.430f;
        constexpr float SliderValueX = 0.835f;
        constexpr float SliderValueW = 0.165f;
        constexpr float SliderH      = ToolTheme::sliderHeight;
        constexpr float SliderTrackY = 0.310f;
        constexpr float SliderTrackH = 0.350f;
        constexpr float FieldLabelX  = 0.000f;
        constexpr float FieldRectX   = 0.330f;
        constexpr float FieldRectW   = 0.670f;
        constexpr float FieldH       = 0.030f;
        constexpr float SearchLabelX = 0.030f;
        constexpr float SearchValueX = 0.260f;

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

    UiHandle createImageRelative(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, const UiImageData& image, bool interactable)
    {
        UiHandle handle = createNodeWithState(ui, UiNodeType::Image, parent, relativeLayout(rect), interactable);
        ui.setPayload(handle, image);
        return handle;
    }

    UiHandle createLineRelative(
        UiSystem& ui, UiHandle parent, UiVec2 start, UiVec2 end, UiColor inColor, float thickness)
    {
        UiHandle handle =
            createNodeWithState(ui, UiNodeType::Line, parent, relativeLayout({0.0f, 0.0f, 1.0f, 1.0f}), false);
        UiStateFlags state = ui.findNode(handle)->state;
        state.enabled      = false;
        ui.setState(handle, state);
        ui.setPayload(handle, UiLineData{start, end, inColor, thickness});
        return handle;
    }

    UiHandle createCircleRelative(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, UiColor inColor, int segments, bool interactable)
    {
        UiHandle handle = createNodeWithState(ui, UiNodeType::Circle, parent, relativeLayout(rect), interactable);
        ui.setPayload(handle, UiCircleData{inColor, segments});
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
        button.rect = createRect(ui, parent, rect, ToolTheme::buttonSecondary, true);
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
        button.rect  = createRectRelative(ui, parent, rect, ToolTheme::buttonSecondary, true);
        button.label = createTextRelative(
            ui, button.rect, {0.060f, 0.120f, 0.880f, 0.760f}, font, text, ToolTheme::text, textScale);
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
                               ToolTheme::sliderTrack,
                               true);
        slider.fill = createRectRelative(ui, slider.track, {0.0f, 0.0f, 0.001f, 1.0f}, ToolTheme::accent);
        return slider;
    }

    ToolTextField createTextField(UiSystem& ui, UiHandle parent, float y, const std::string& name, BitmapFont* font)
    {
        ToolTextField field;
        field.name  = name;
        field.label = createTextRelative(
            ui, parent, {FieldLabelX, y, 0.305f, FieldH}, font, name, ToolTheme::mutedText, ToolTheme::smallTextScale);
        field.rect = createRectRelative(
            ui, parent, {FieldRectX, y + 0.002f, FieldRectW, FieldH - 0.004f}, ToolTheme::inputBackground, true);
        field.value = createTextRelative(
            ui, field.rect, {0.025f, 0.120f, 0.950f, 0.760f}, font, "", ToolTheme::text, ToolTheme::smallTextScale);
        setTextAlignment(ui, field.value, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
        return field;
    }

    ToolTextField createSearchFieldRelative(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, const std::string& name, BitmapFont* font)
    {
        ToolTextField field;
        field.name  = name;
        field.rect  = createRectRelative(ui, parent, rect, ToolTheme::inputBackground, true);
        field.label = createTextRelative(ui,
                                         field.rect,
                                         {SearchLabelX, 0.120f, 0.200f, 0.760f},
                                         font,
                                         name,
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);
        field.value = createTextRelative(ui,
                                         field.rect,
                                         {SearchValueX, 0.120f, 0.700f, 0.760f},
                                         font,
                                         "",
                                         ToolTheme::text,
                                         ToolTheme::smallTextScale);
        setTextAlignment(ui, field.label, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
        setTextAlignment(ui, field.value, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
        return field;
    }

    ToolPanelFrame createPanelFrameRelative(UiSystem&          ui,
                                            UiHandle           parent,
                                            const ToolRect&    rect,
                                            BitmapFont*        font,
                                            const std::string& title,
                                            const std::string& subtitle)
    {
        ToolPanelFrame frame;
        createRectRelative(ui,
                           parent,
                           {rect.x + 0.002f, rect.y + 0.004f, rect.width, rect.height},
                           {0.000f, 0.000f, 0.000f, 0.180f});
        frame.background = createRectRelative(ui, parent, rect, ToolTheme::paneBackground);
        frame.accent     = createRectRelative(ui, frame.background, {0.0f, 0.0f, 1.0f, 0.120f}, ToolTheme::headerBackground);
        createRectRelative(ui, frame.background, {0.0f, 0.118f, 1.0f, 0.004f}, ToolTheme::separator);
        createRectRelative(ui, frame.background, {0.0f, 0.0f, 1.0f, 0.003f}, ToolTheme::borderSubtle);
        frame.title      = createTextRelative(ui,
                                              frame.background,
                                              {0.025f, 0.024f, 0.560f, 0.075f},
                                              font,
                                              title,
                                              ToolTheme::text,
                                              ToolTheme::headerTextScale);
        frame.subtitle   = createTextRelative(ui,
                                              frame.background,
                                              {0.600f, 0.030f, 0.375f, 0.065f},
                                              font,
                                              subtitle,
                                              ToolTheme::mutedText,
                                              ToolTheme::smallTextScale);
        setTextAlignment(ui, frame.subtitle, UiHorizontalAlign::Right, UiVerticalAlign::Top);
        return frame;
    }

    ToolSectionHeader createSectionHeaderRelative(UiSystem&          ui,
                                                  UiHandle           parent,
                                                  const ToolRect&    rect,
                                                  BitmapFont*        font,
                                                  const std::string& label,
                                                  const std::string& summary)
    {
        ToolSectionHeader header;
        header.rect    = createRectRelative(ui, parent, rect, ToolTheme::sectionHeader, true);
        createRectRelative(ui, header.rect, {0.0f, 0.0f, 1.0f, 0.040f}, ToolTheme::borderSubtle);
        header.label   = createTextRelative(ui,
                                            header.rect,
                                            {0.038f, 0.130f, 0.507f, 0.740f},
                                            font,
                                            label,
                                            ToolTheme::text,
                                            ToolTheme::buttonTextScale);
        header.summary = createTextRelative(ui,
                                            header.rect,
                                            {0.560f, 0.150f, 0.415f, 0.700f},
                                            font,
                                            summary,
                                            ToolTheme::mutedText,
                                            ToolTheme::smallTextScale);
        setTextAlignment(ui, header.summary, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
        return header;
    }

    ToolStatusRow createStatusRowRelative(UiSystem&          ui,
                                          UiHandle           parent,
                                          const ToolRect&    rect,
                                          BitmapFont*        font,
                                          const std::string& label,
                                          const std::string& value)
    {
        ToolStatusRow row;
        row.label = createTextRelative(ui,
                                       parent,
                                       {rect.x, rect.y, rect.width * 0.360f, rect.height},
                                       font,
                                       label,
                                       ToolTheme::mutedText,
                                       ToolTheme::smallTextScale);
        row.value = createTextRelative(ui,
                                       parent,
                                       {rect.x + rect.width * 0.385f, rect.y, rect.width * 0.615f, rect.height},
                                       font,
                                       value,
                                       ToolTheme::text,
                                       ToolTheme::smallTextScale);
        setTextAlignment(ui, row.value, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
        return row;
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
            pressed ? ToolTheme::buttonPressed : (hovered ? ToolTheme::buttonHover : ToolTheme::buttonSecondary);
        setRectColor(ui, button.rect, buttonColor);
        setTextColor(ui, button.label, ToolTheme::text);
        setText(ui, button.label, label);
    }

    void updateToggleButton(UiSystem& ui, const ToolButton& button, const std::string& label, bool enabled)
    {
        const UiNode* node        = ui.findNode(button.rect);
        const bool    hovered     = node != nullptr && node->state.hovered;
        const bool    pressed     = node != nullptr && node->state.pressed;
        UiColor       buttonColor = enabled ? ToolTheme::toggleActive : ToolTheme::buttonSecondary;
        if (hovered)
            buttonColor = enabled ? ToolTheme::toggleHover : ToolTheme::buttonHover;
        if (pressed)
            buttonColor = ToolTheme::buttonPressed;

        setRectColor(ui, button.rect, buttonColor);
        setTextColor(ui, button.label, enabled ? ToolTheme::text : ToolTheme::mutedText);
        setText(ui, button.label, label);
    }

    void updateSlider(UiSystem& ui, const ToolSlider& slider, float value, UiColor fillColor)
    {
        const float t = slider.maxValue <= slider.minValue
                            ? 0.0f
                            : std::clamp((value - slider.minValue) / (slider.maxValue - slider.minValue), 0.0f, 1.0f);
        const UiNode* trackNode = ui.findNode(slider.track);
        const bool    hovered   = trackNode != nullptr && trackNode->state.hovered;
        setRectColor(ui, slider.track, hovered ? ToolTheme::sliderTrackHover : ToolTheme::sliderTrack);
        setRelativeRect(ui, slider.fill, {0.0f, 0.0f, t, 1.0f});
        setRectColor(ui, slider.fill, fillColor);
        setText(ui, slider.value, formatValue(value, slider.wholeNumber));
    }

    void updateTextField(UiSystem& ui, const ToolTextField& field, const std::string& value, bool focused)
    {
        const UiNode* node    = ui.findNode(field.rect);
        const bool    hovered = node != nullptr && node->state.hovered;
        UiColor       fill    = ToolTheme::inputBackground;
        if (hovered)
            fill = ToolTheme::inputHover;
        if (focused)
            fill = ToolTheme::inputActive;

        setRectColor(ui, field.rect, fill);
        setText(ui, field.value, value);
    }

    void
    updatePanelFrame(UiSystem& ui, const ToolPanelFrame& frame, const std::string& title, const std::string& subtitle)
    {
        setText(ui, frame.title, title);
        setText(ui, frame.subtitle, subtitle);
    }

    void updateSectionHeader(UiSystem&                ui,
                             const ToolSectionHeader& header,
                             const std::string&       label,
                             const std::string&       summary,
                             bool                     active)
    {
        const UiNode* node    = ui.findNode(header.rect);
        const bool    hovered = node != nullptr && node->state.hovered;
        UiColor       fill    = active ? ToolTheme::buttonActive : ToolTheme::sectionHeader;
        if (hovered && !active)
            fill = ToolTheme::sectionHeaderHover;

        setRectColor(ui, header.rect, fill);
        setText(ui, header.label, std::string(active ? "v " : "> ") + label);
        setText(ui, header.summary, summary);
    }

    void updateStatusRow(UiSystem& ui, const ToolStatusRow& row, const std::string& value)
    {
        setText(ui, row.value, value);
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
