#include "widgets/UiToolWidgets.h"

#include <variant>

#include "UiSystem.h"

namespace gts::ui
{
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
}
