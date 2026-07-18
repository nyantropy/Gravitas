#pragma once

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <variant>

#include "UiNode.h"
#include "UiSystem.h"
#include "UiWidget.h"
#include "ToolTheme.h"

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
                                UiVec2 shadowOffset = {},
                                float cornerRadius = 0.0f)
    {
        if (handle == UI_INVALID_HANDLE)
            return;

        UiRectData data;
        data.color = fill;
        data.borderColor = border;
        data.borderThickness = borderThickness;
        data.shadowColor = shadow;
        data.shadowOffset = shadowOffset;
        data.cornerRadius = cornerRadius;
        ui.setPayload(surface, handle, data);
    }

    enum class SurfaceRole
    {
        Pane,
        Raised,
        Inset,
        ToolbarGroup,
        Overlay,
        Control,
        ActiveControl,
        SelectedRow,
        SectionHeader,
        Chrome
    };

    struct SurfaceStyle
    {
        UiColor fill = ToolTheme::paneSurface;
        UiColor border = ToolTheme::borderSubtle;
        UiColor shadow = {0.0f, 0.0f, 0.0f, 0.0f};
        UiVec2 shadowOffset = {};
        float borderThickness = ToolTheme::panelBorderWidth;
        float cornerRadius = ToolTheme::radiusMedium;
    };

    inline SurfaceStyle surfaceStyle(SurfaceRole role)
    {
        SurfaceStyle style;
        switch (role)
        {
            case SurfaceRole::Pane:
                style.fill = ToolTheme::paneBackground;
                style.border = ToolTheme::border;
                style.shadow = ToolTheme::shadow;
                style.shadowOffset = {0.0f, ToolTheme::panelShadowOffset};
                style.cornerRadius = ToolTheme::panelRadius;
                break;
            case SurfaceRole::Raised:
                style.fill = DefaultEditorTheme.colors.panelSurfaceRaised;
                style.border = ToolTheme::border;
                style.shadow = ToolTheme::shadow;
                style.shadowOffset = {0.0f, ToolTheme::panelShadowOffset};
                style.cornerRadius = ToolTheme::panelRadius;
                break;
            case SurfaceRole::Inset:
                style.fill = ToolTheme::panelInset;
                style.border = ToolTheme::borderSubtle;
                style.cornerRadius = ToolTheme::radiusMedium;
                break;
            case SurfaceRole::ToolbarGroup:
                style.fill = ToolTheme::toolbarGroupBackground;
                style.border = ToolTheme::borderSubtle;
                style.shadow = {0.0f, 0.0f, 0.0f, 0.220f};
                style.shadowOffset = {0.0f, ToolTheme::panelShadowOffset * 0.45f};
                style.cornerRadius = ToolTheme::radiusMedium;
                break;
            case SurfaceRole::Overlay:
                style.fill = ToolTheme::viewportOverlay;
                style.border = ToolTheme::border;
                style.shadow = {0.0f, 0.0f, 0.0f, 0.320f};
                style.shadowOffset = {0.0f, ToolTheme::panelShadowOffset * 0.55f};
                style.cornerRadius = ToolTheme::overlayRadius;
                break;
            case SurfaceRole::Control:
                style.fill = ToolTheme::button;
                style.border = ToolTheme::borderSubtle;
                style.cornerRadius = ToolTheme::controlRadius;
                break;
            case SurfaceRole::ActiveControl:
                style.fill = ToolTheme::buttonActive;
                style.border = ToolTheme::accent;
                style.cornerRadius = ToolTheme::controlRadius;
                break;
            case SurfaceRole::SelectedRow:
                style.fill = ToolTheme::rowSelected;
                style.border = ToolTheme::accent;
                style.cornerRadius = ToolTheme::rowRadius;
                break;
            case SurfaceRole::SectionHeader:
                style.fill = ToolTheme::sectionHeader;
                style.border = ToolTheme::borderSubtle;
                style.cornerRadius = ToolTheme::radiusSmall;
                break;
            case SurfaceRole::Chrome:
                style.fill = ToolTheme::barBackground;
                style.border = ToolTheme::borderSubtle;
                style.cornerRadius = 0.0f;
                break;
        }
        return style;
    }

    inline void setSurfacePayload(UiSystem& ui,
                                  UiSurfaceId surface,
                                  UiHandle handle,
                                  SurfaceRole role)
    {
        const SurfaceStyle style = surfaceStyle(role);
        setPanelPayload(ui,
                        surface,
                        handle,
                        style.fill,
                        style.border,
                        style.borderThickness,
                        style.shadow,
                        style.shadowOffset,
                        style.cornerRadius);
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
