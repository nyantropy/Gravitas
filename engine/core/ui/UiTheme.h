#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "UiPanelSkin.h"

namespace UiThemeTokens
{
    inline constexpr const char* BackgroundPrimary = "BackgroundPrimary";
    inline constexpr const char* BackgroundSecondary = "BackgroundSecondary";
    inline constexpr const char* Panel = "Panel";
    inline constexpr const char* PanelRaised = "PanelRaised";
    inline constexpr const char* Accent = "Accent";
    inline constexpr const char* AccentHover = "AccentHover";
    inline constexpr const char* AccentPressed = "AccentPressed";
    inline constexpr const char* TextPrimary = "TextPrimary";
    inline constexpr const char* TextSecondary = "TextSecondary";
    inline constexpr const char* Warning = "Warning";
    inline constexpr const char* Danger = "Danger";
    inline constexpr const char* Success = "Success";
    inline constexpr const char* Selection = "Selection";
    inline constexpr const char* Highlight = "Highlight";
    inline constexpr const char* Focus = "Focus";
    inline constexpr const char* Shadow = "Shadow";
    inline constexpr const char* Separator = "Separator";
    inline constexpr const char* Tooltip = "Tooltip";
}

struct UiTypography
{
    std::string fontAsset;
    float scale = 0.02f;
    int weight = 400;
    float lineHeight = 1.0f;
    float letterSpacing = 0.0f;
    float paragraphSpacing = 0.0f;

    bool operator==(const UiTypography&) const = default;
};

struct UiStyleProperties
{
    std::string backgroundColorToken;
    std::string foregroundColorToken;
    std::string skinName;
    std::string typographyName;
    std::optional<UiColor> backgroundColor;
    std::optional<UiColor> foregroundColor;
    std::optional<UiPanelSkin> skin;
    std::optional<UiTypography> typography;
    std::optional<UiThickness> padding;
    std::optional<float> opacity;
    std::optional<float> borderWidth;
    std::optional<float> borderRadius;
    std::optional<UiColor> borderColor;
    std::optional<UiColor> shadowColor;
    std::optional<UiVec2> shadowOffset;
    std::optional<float> shadowBlur;

    bool operator==(const UiStyleProperties&) const = default;
};

struct UiStyleClass
{
    std::string parentClass;
    UiStyleProperties base;
    std::unordered_map<UiStyleState, UiStyleProperties> states;

    bool operator==(const UiStyleClass&) const = default;
};

struct UiComputedStyle
{
    bool hasBackgroundColor = false;
    bool hasForegroundColor = false;
    bool hasSkin = false;
    bool hasTypography = false;
    bool hasPadding = false;
    bool hasOpacity = false;
    bool hasBorderWidth = false;
    bool hasBorderRadius = false;
    bool hasBorderColor = false;
    bool hasShadowColor = false;
    bool hasShadowOffset = false;
    bool hasShadowBlur = false;
    UiColor backgroundColor;
    UiColor foregroundColor;
    UiPanelSkin skin;
    UiTypography typography;
    UiThickness padding;
    float opacity = 1.0f;
    float borderWidth = 0.0f;
    float borderRadius = 0.0f;
    UiColor borderColor;
    UiColor shadowColor;
    UiVec2 shadowOffset;
    float shadowBlur = 0.0f;
};

UiStyleState uiStyleStateFromFlags(const UiStateFlags& state);

class UiTheme
{
public:
    UiTheme();
    UiTheme(const UiTheme& other);
    UiTheme& operator=(const UiTheme& other);

    void setParent(const UiTheme& parent);
    void clearParent();
    const UiTheme* parent() const { return parentTheme.get(); }

    void setColor(const std::string& name, UiColor color);
    bool resolveColor(const std::string& name, UiColor& outColor) const;
    UiColor colorOr(const std::string& name, UiColor fallback) const;

    void setMetric(const std::string& name, float value);
    bool resolveMetric(const std::string& name, float& outValue) const;
    float metricOr(const std::string& name, float fallback) const;

    void setTypography(const std::string& name, const UiTypography& typography);
    bool resolveTypography(const std::string& name, UiTypography& outTypography) const;

    void setSkin(const std::string& name, const UiPanelSkin& skin);
    bool resolveSkin(const std::string& name, UiPanelSkin& outSkin) const;

    void setPanelStateSkin(const std::string& name, const UiPanelStateSkin& skin);
    bool resolvePanelStateSkin(const std::string& name, UiPanelStateSkin& outSkin) const;

    void setStyleClass(const std::string& name, const UiStyleClass& styleClass);
    const UiStyleClass* findStyleClass(const std::string& name) const;
    bool resolveStyle(const std::string& name, UiStyleState state, UiComputedStyle& outStyle) const;
    bool resolveNodeStyle(const UiStyle& style, const UiStateFlags& state, UiComputedStyle& outStyle) const;

    bool operator==(const UiTheme& rhs) const;
    bool operator!=(const UiTheme& rhs) const { return !(*this == rhs); }

private:
    bool resolveStyleInternal(const std::string& name,
                              UiStyleState state,
                              UiComputedStyle& outStyle,
                              int depth) const;
    bool resolveStyleClassFromTheme(const UiTheme& sourceTheme,
                                    const std::string& name,
                                    UiStyleState state,
                                    UiComputedStyle& outStyle,
                                    int depth) const;
    void applyProperties(const UiStyleProperties& properties, UiComputedStyle& outStyle) const;

    std::unique_ptr<UiTheme> parentTheme;
    std::unordered_map<std::string, UiColor> colors;
    std::unordered_map<std::string, float> metrics;
    std::unordered_map<std::string, UiTypography> typographies;
    std::unordered_map<std::string, UiPanelSkin> skins;
    std::unordered_map<std::string, UiPanelStateSkin> panelStateSkins;
    std::unordered_map<std::string, UiStyleClass> styleClasses;
};

UiTheme defaultUiTheme();
