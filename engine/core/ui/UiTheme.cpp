#include "UiTheme.h"

#include <algorithm>

namespace
{
    constexpr int MaxStyleResolutionDepth = 32;

    UiPanelSkin solidSkin(UiColor color, UiThickness padding = {})
    {
        UiPanelSkin skin;
        skin.type = UiPanelSkinType::SolidColor;
        skin.color = color;
        skin.contentPadding = padding;
        return skin;
    }

    UiTypography typography(float scale, int weight, float lineHeight)
    {
        UiTypography result;
        result.scale = scale;
        result.weight = weight;
        result.lineHeight = lineHeight;
        return result;
    }
}

UiStyleState uiStyleStateFromFlags(const UiStateFlags& state)
{
    if (!state.enabled)
        return UiStyleState::Disabled;
    if (state.pressed)
        return UiStyleState::Pressed;
    if (state.hovered)
        return UiStyleState::Hover;
    if (state.focused)
        return UiStyleState::Focused;
    return UiStyleState::Normal;
}

UiTheme::UiTheme() = default;

UiTheme::UiTheme(const UiTheme& other)
{
    *this = other;
}

UiTheme& UiTheme::operator=(const UiTheme& other)
{
    if (this == &other)
        return *this;

    parentTheme.reset();
    if (other.parentTheme)
        parentTheme = std::make_unique<UiTheme>(*other.parentTheme);

    colors = other.colors;
    metrics = other.metrics;
    typographies = other.typographies;
    skins = other.skins;
    panelStateSkins = other.panelStateSkins;
    styleClasses = other.styleClasses;
    return *this;
}

void UiTheme::setParent(const UiTheme& parent)
{
    parentTheme = std::make_unique<UiTheme>(parent);
}

void UiTheme::clearParent()
{
    parentTheme.reset();
}

void UiTheme::setColor(const std::string& name, UiColor color)
{
    colors[name] = color;
}

bool UiTheme::resolveColor(const std::string& name, UiColor& outColor) const
{
    const auto it = colors.find(name);
    if (it != colors.end())
    {
        outColor = it->second;
        return true;
    }

    return parentTheme ? parentTheme->resolveColor(name, outColor) : false;
}

UiColor UiTheme::colorOr(const std::string& name, UiColor fallback) const
{
    UiColor color;
    return resolveColor(name, color) ? color : fallback;
}

void UiTheme::setMetric(const std::string& name, float value)
{
    metrics[name] = value;
}

bool UiTheme::resolveMetric(const std::string& name, float& outValue) const
{
    const auto it = metrics.find(name);
    if (it != metrics.end())
    {
        outValue = it->second;
        return true;
    }

    return parentTheme ? parentTheme->resolveMetric(name, outValue) : false;
}

float UiTheme::metricOr(const std::string& name, float fallback) const
{
    float value = 0.0f;
    return resolveMetric(name, value) ? value : fallback;
}

void UiTheme::setTypography(const std::string& name, const UiTypography& typography)
{
    typographies[name] = typography;
}

bool UiTheme::resolveTypography(const std::string& name, UiTypography& outTypography) const
{
    const auto it = typographies.find(name);
    if (it != typographies.end())
    {
        outTypography = it->second;
        return true;
    }

    return parentTheme ? parentTheme->resolveTypography(name, outTypography) : false;
}

void UiTheme::setSkin(const std::string& name, const UiPanelSkin& skin)
{
    skins[name] = skin;
}

bool UiTheme::resolveSkin(const std::string& name, UiPanelSkin& outSkin) const
{
    const auto it = skins.find(name);
    if (it != skins.end())
    {
        outSkin = it->second;
        return true;
    }

    return parentTheme ? parentTheme->resolveSkin(name, outSkin) : false;
}

void UiTheme::setPanelStateSkin(const std::string& name, const UiPanelStateSkin& skin)
{
    panelStateSkins[name] = skin;
}

bool UiTheme::resolvePanelStateSkin(const std::string& name, UiPanelStateSkin& outSkin) const
{
    const auto it = panelStateSkins.find(name);
    if (it != panelStateSkins.end())
    {
        outSkin = it->second;
        return true;
    }

    return parentTheme ? parentTheme->resolvePanelStateSkin(name, outSkin) : false;
}

void UiTheme::setStyleClass(const std::string& name, const UiStyleClass& styleClass)
{
    styleClasses[name] = styleClass;
}

const UiStyleClass* UiTheme::findStyleClass(const std::string& name) const
{
    const auto it = styleClasses.find(name);
    if (it != styleClasses.end())
        return &it->second;

    return parentTheme ? parentTheme->findStyleClass(name) : nullptr;
}

bool UiTheme::resolveStyle(const std::string& name, UiStyleState state, UiComputedStyle& outStyle) const
{
    return resolveStyleInternal(name, state, outStyle, 0);
}

bool UiTheme::resolveNodeStyle(const UiStyle& style, const UiStateFlags& state, UiComputedStyle& outStyle) const
{
    bool resolved = false;
    const UiStyleState effectiveState = style.useStyleState
        ? style.styleState
        : uiStyleStateFromFlags(state);

    if (!style.styleClass.empty())
        resolved = resolveStyle(style.styleClass, effectiveState, outStyle);

    if (style.useBackgroundColor)
    {
        outStyle.hasBackgroundColor = true;
        outStyle.backgroundColor = style.backgroundColor;
        resolved = true;
    }

    if (style.useForegroundColor)
    {
        outStyle.hasForegroundColor = true;
        outStyle.foregroundColor = style.foregroundColor;
        resolved = true;
    }

    if (style.useOpacity)
    {
        outStyle.hasOpacity = true;
        outStyle.opacity = style.opacity;
        resolved = true;
    }

    return resolved;
}

bool UiTheme::operator==(const UiTheme& rhs) const
{
    const bool parentsEqual =
        (!parentTheme && !rhs.parentTheme) ||
        (parentTheme && rhs.parentTheme && *parentTheme == *rhs.parentTheme);

    return parentsEqual &&
        colors == rhs.colors &&
        metrics == rhs.metrics &&
        typographies == rhs.typographies &&
        skins == rhs.skins &&
        panelStateSkins == rhs.panelStateSkins &&
        styleClasses == rhs.styleClasses;
}

bool UiTheme::resolveStyleInternal(const std::string& name,
                                   UiStyleState state,
                                   UiComputedStyle& outStyle,
                                   int depth) const
{
    if (depth > MaxStyleResolutionDepth)
        return false;

    bool resolved = parentTheme
        ? resolveStyleClassFromTheme(*parentTheme, name, state, outStyle, depth + 1)
        : false;

    const auto it = styleClasses.find(name);
    if (it == styleClasses.end())
        return resolved;

    const UiStyleClass& styleClass = it->second;
    if (!styleClass.parentClass.empty())
    {
        UiComputedStyle parentStyle;
        if (resolveStyleInternal(styleClass.parentClass, state, parentStyle, depth + 1))
            outStyle = parentStyle;
    }

    applyProperties(styleClass.base, outStyle);

    const auto stateIt = styleClass.states.find(state);
    if (stateIt != styleClass.states.end())
        applyProperties(stateIt->second, outStyle);

    return true;
}

bool UiTheme::resolveStyleClassFromTheme(const UiTheme& sourceTheme,
                                         const std::string& name,
                                         UiStyleState state,
                                         UiComputedStyle& outStyle,
                                         int depth) const
{
    if (depth > MaxStyleResolutionDepth)
        return false;

    bool resolved = sourceTheme.parentTheme
        ? resolveStyleClassFromTheme(*sourceTheme.parentTheme, name, state, outStyle, depth + 1)
        : false;

    const auto it = sourceTheme.styleClasses.find(name);
    if (it == sourceTheme.styleClasses.end())
        return resolved;

    const UiStyleClass& styleClass = it->second;
    if (!styleClass.parentClass.empty())
    {
        UiComputedStyle parentStyle;
        if (resolveStyleInternal(styleClass.parentClass, state, parentStyle, depth + 1))
        {
            outStyle = parentStyle;
            resolved = true;
        }
    }

    applyProperties(styleClass.base, outStyle);

    const auto stateIt = styleClass.states.find(state);
    if (stateIt != styleClass.states.end())
        applyProperties(stateIt->second, outStyle);

    return true;
}

void UiTheme::applyProperties(const UiStyleProperties& properties, UiComputedStyle& outStyle) const
{
    UiColor color;
    if (!properties.backgroundColorToken.empty() && resolveColor(properties.backgroundColorToken, color))
    {
        outStyle.hasBackgroundColor = true;
        outStyle.backgroundColor = color;
    }
    if (properties.backgroundColor)
    {
        outStyle.hasBackgroundColor = true;
        outStyle.backgroundColor = *properties.backgroundColor;
    }

    if (!properties.foregroundColorToken.empty() && resolveColor(properties.foregroundColorToken, color))
    {
        outStyle.hasForegroundColor = true;
        outStyle.foregroundColor = color;
    }
    if (properties.foregroundColor)
    {
        outStyle.hasForegroundColor = true;
        outStyle.foregroundColor = *properties.foregroundColor;
    }

    UiPanelSkin resolvedSkin;
    if (!properties.skinName.empty() && resolveSkin(properties.skinName, resolvedSkin))
    {
        outStyle.hasSkin = true;
        outStyle.skin = resolvedSkin;
    }
    if (properties.skin)
    {
        outStyle.hasSkin = true;
        outStyle.skin = *properties.skin;
    }

    UiTypography typography;
    if (!properties.typographyName.empty() && resolveTypography(properties.typographyName, typography))
    {
        outStyle.hasTypography = true;
        outStyle.typography = typography;
    }
    if (properties.typography)
    {
        outStyle.hasTypography = true;
        outStyle.typography = *properties.typography;
    }

    if (properties.padding)
    {
        outStyle.hasPadding = true;
        outStyle.padding = *properties.padding;
    }

    if (properties.opacity)
    {
        outStyle.hasOpacity = true;
        outStyle.opacity = std::clamp(*properties.opacity, 0.0f, 1.0f);
    }

    if (properties.borderWidth)
    {
        outStyle.hasBorderWidth = true;
        outStyle.borderWidth = std::max(0.0f, *properties.borderWidth);
    }

    if (properties.borderRadius)
    {
        outStyle.hasBorderRadius = true;
        outStyle.borderRadius = std::max(0.0f, *properties.borderRadius);
    }

    if (properties.borderColor)
    {
        outStyle.hasBorderColor = true;
        outStyle.borderColor = *properties.borderColor;
    }

    if (properties.shadowColor)
    {
        outStyle.hasShadowColor = true;
        outStyle.shadowColor = *properties.shadowColor;
    }

    if (properties.shadowOffset)
    {
        outStyle.hasShadowOffset = true;
        outStyle.shadowOffset = *properties.shadowOffset;
    }

    if (properties.shadowBlur)
    {
        outStyle.hasShadowBlur = true;
        outStyle.shadowBlur = std::max(0.0f, *properties.shadowBlur);
    }
}

UiTheme defaultUiTheme()
{
    UiTheme theme;
    theme.setColor(UiThemeTokens::BackgroundPrimary, {0.025f, 0.029f, 0.034f, 1.0f});
    theme.setColor(UiThemeTokens::BackgroundSecondary, {0.040f, 0.046f, 0.054f, 1.0f});
    theme.setColor(UiThemeTokens::Panel, {0.050f, 0.058f, 0.068f, 0.96f});
    theme.setColor(UiThemeTokens::PanelRaised, {0.070f, 0.080f, 0.094f, 0.98f});
    theme.setColor(UiThemeTokens::Accent, {0.210f, 0.550f, 0.780f, 1.0f});
    theme.setColor(UiThemeTokens::AccentHover, {0.260f, 0.620f, 0.860f, 1.0f});
    theme.setColor(UiThemeTokens::AccentPressed, {0.150f, 0.410f, 0.620f, 1.0f});
    theme.setColor(UiThemeTokens::TextPrimary, {0.900f, 0.925f, 0.950f, 1.0f});
    theme.setColor(UiThemeTokens::TextSecondary, {0.640f, 0.695f, 0.750f, 1.0f});
    theme.setColor(UiThemeTokens::Warning, {0.890f, 0.645f, 0.210f, 1.0f});
    theme.setColor(UiThemeTokens::Danger, {0.900f, 0.245f, 0.265f, 1.0f});
    theme.setColor(UiThemeTokens::Success, {0.230f, 0.700f, 0.380f, 1.0f});
    theme.setColor(UiThemeTokens::Selection, {0.112f, 0.260f, 0.380f, 1.0f});
    theme.setColor(UiThemeTokens::Highlight, {0.550f, 0.660f, 0.750f, 0.12f});
    theme.setColor(UiThemeTokens::Focus, {0.240f, 0.560f, 0.810f, 1.0f});
    theme.setColor(UiThemeTokens::Shadow, {0.0f, 0.0f, 0.0f, 0.42f});
    theme.setColor(UiThemeTokens::Separator, {0.125f, 0.142f, 0.160f, 0.62f});
    theme.setColor(UiThemeTokens::Tooltip, {0.030f, 0.036f, 0.044f, 0.97f});

    theme.setMetric("PaddingSmall", 0.006f);
    theme.setMetric("PaddingMedium", 0.012f);
    theme.setMetric("PaddingLarge", 0.020f);
    theme.setMetric("GapSmall", 0.004f);
    theme.setMetric("GapMedium", 0.008f);
    theme.setMetric("GapLarge", 0.016f);
    theme.setMetric("BorderRadius", 0.004f);
    theme.setMetric("ButtonHeight", 0.034f);
    theme.setMetric("IconSize", 0.028f);
    theme.setMetric("ScrollbarWidth", 0.010f);
    theme.setMetric("WindowPadding", 0.018f);

    theme.setTypography("Heading", typography(0.026f, 700, 1.15f));
    theme.setTypography("Subheading", typography(0.021f, 600, 1.15f));
    theme.setTypography("Body", typography(0.018f, 400, 1.20f));
    theme.setTypography("Caption", typography(0.014f, 400, 1.15f));
    theme.setTypography("Code", typography(0.015f, 400, 1.20f));
    theme.setTypography("Dialogue", typography(0.023f, 400, 1.20f));
    theme.setTypography("Choice", typography(0.020f, 500, 1.18f));
    theme.setTypography("Tooltip", typography(0.015f, 400, 1.15f));
    theme.setTypography("Debug", typography(0.013f, 400, 1.10f));
    theme.setTypography("Editor", typography(0.008f, 400, 1.12f));

    theme.setSkin("Panel", solidSkin(theme.colorOr(UiThemeTokens::Panel, {0.05f, 0.058f, 0.068f, 0.96f})));
    theme.setSkin("PanelRaised", solidSkin(theme.colorOr(UiThemeTokens::PanelRaised, {0.07f, 0.08f, 0.094f, 0.98f})));
    theme.setSkin("Tooltip", solidSkin(theme.colorOr(UiThemeTokens::Tooltip, {0.03f, 0.036f, 0.044f, 0.97f})));

    UiStyleClass panel;
    panel.base.skinName = "Panel";
    panel.base.foregroundColorToken = UiThemeTokens::TextPrimary;
    theme.setStyleClass("Panel", panel);

    UiStyleClass label;
    label.base.foregroundColorToken = UiThemeTokens::TextPrimary;
    label.base.typographyName = "Body";
    theme.setStyleClass("Text.Body", label);

    UiStyleClass button;
    button.base.backgroundColorToken = UiThemeTokens::PanelRaised;
    button.base.foregroundColorToken = UiThemeTokens::TextPrimary;
    button.base.typographyName = "Body";
    button.states[UiStyleState::Hover].backgroundColorToken = UiThemeTokens::AccentHover;
    button.states[UiStyleState::Pressed].backgroundColorToken = UiThemeTokens::AccentPressed;
    button.states[UiStyleState::Disabled].backgroundColor = {0.045f, 0.051f, 0.060f, 0.62f};
    theme.setStyleClass("Button", button);

    UiStyleClass separator;
    separator.base.backgroundColorToken = UiThemeTokens::Separator;
    theme.setStyleClass("Separator", separator);

    UiStyleClass progressTrack;
    progressTrack.base.backgroundColorToken = UiThemeTokens::BackgroundSecondary;
    theme.setStyleClass("ProgressBar.Track", progressTrack);

    UiStyleClass progressFill;
    progressFill.base.backgroundColorToken = UiThemeTokens::Accent;
    theme.setStyleClass("ProgressBar.Fill", progressFill);

    return theme;
}
