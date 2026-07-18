#pragma once

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <variant>

#include "UiNode.h"
#include "UiSystem.h"
#include "UiWidget.h"

namespace gts::tools::toolui
{
    inline UiLayoutSpec rect(float x, float y, float w, float h)
    {
        UiLayoutSpec layout = gts::ui::fillLayout();
        layout.anchorMin = {x, y};
        layout.anchorMax = {x + w, y + h};
        return layout;
    }

    inline UiStateFlags visibleState(bool visible, bool enabled, bool interactable)
    {
        UiStateFlags state;
        state.visible = visible;
        state.enabled = enabled;
        state.interactable = interactable;
        return state;
    }

    inline std::string fileName(const std::string& path)
    {
        if (path.empty())
            return {};

        std::filesystem::path fsPath(path);
        const std::string name = fsPath.filename().string();
        return name.empty() ? path : name;
    }

    inline std::string stemName(const std::string& path)
    {
        if (path.empty())
            return {};

        std::filesystem::path fsPath(path);
        const std::string name = fsPath.stem().string();
        return name.empty() ? fileName(path) : name;
    }

    inline std::string compact(const std::string& text, size_t maxChars)
    {
        if (text.size() <= maxChars || maxChars < 8)
            return text;

        const size_t left = (maxChars - 3) / 2;
        const size_t right = maxChars - 3 - left;
        return text.substr(0, left) + "..." + text.substr(text.size() - right);
    }

    inline std::string fixed(float value, int precision = 2)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    }

    inline void setRectPayload(UiSystem& ui, UiSurfaceId surface, UiHandle handle, UiColor color)
    {
        if (handle != UI_INVALID_HANDLE)
            ui.setPayload(surface, handle, UiRectData{color});
    }

    inline void setPanelPayload(UiSystem& ui,
                                UiSurfaceId surface,
                                UiHandle handle,
                                UiColor fill,
                                UiColor border,
                                float borderThickness,
                                UiColor shadow = {0.0f, 0.0f, 0.0f, 0.0f},
                                UiVec2 shadowOffset = {})
    {
        if (handle == UI_INVALID_HANDLE)
            return;

        UiRectData data;
        data.color = fill;
        data.borderColor = border;
        data.borderThickness = borderThickness;
        data.shadowColor = shadow;
        data.shadowOffset = shadowOffset;
        ui.setPayload(surface, handle, data);
    }

    inline void setTextColor(UiSystem& ui, UiSurfaceId surface, UiHandle handle, UiColor color)
    {
        if (handle == UI_INVALID_HANDLE)
            return;

        const UiNode* node = ui.findNode(surface, handle);
        if (node == nullptr || node->type != UiNodeType::Text)
            return;

        UiTextData text = std::get<UiTextData>(node->payload);
        text.color = color;
        ui.setPayload(surface, handle, text);
    }
} // namespace gts::tools::toolui
