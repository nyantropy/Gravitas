#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI theme runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    bool near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    bool nearColor(const UiColor& lhs, const UiColor& rhs, float epsilon = 0.001f)
    {
        return near(lhs.r, rhs.r, epsilon)
            && near(lhs.g, rhs.g, epsilon)
            && near(lhs.b, rhs.b, epsilon)
            && near(lhs.a, rhs.a, epsilon);
    }

    UiLayoutSpec fillLayout()
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {0.0f, 0.0f};
        layout.anchorMax = {1.0f, 1.0f};
        return layout;
    }

    UiLayoutSpec fixedLayout(float width, float height)
    {
        UiLayoutSpec layout;
        layout.constraints.preferredWidth = {UiLayoutUnit::Normalized, width};
        layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, height};
        layout.constraints.horizontalAlignment = UiLayoutAlignment::Start;
        layout.constraints.verticalAlignment = UiLayoutAlignment::Start;
        return layout;
    }

    UiHandle createRect(UiSystem& ui, UiHandle parent, UiColor color)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, fixedLayout(0.20f, 0.20f));
        ui.setPayload(handle, UiRectData{color});
        return handle;
    }

    UiHandle createRect(UiSystem& ui, UiSurfaceId surface, UiHandle parent, UiColor color)
    {
        const UiHandle handle = ui.createNode(surface, UiNodeType::Rect, parent);
        ui.setLayout(surface, handle, fixedLayout(0.20f, 0.20f));
        ui.setPayload(surface, handle, UiRectData{color});
        return handle;
    }

    UiHandle createText(UiSystem& ui, UiHandle parent, const std::string& text)
    {
        UiLayoutSpec layout;
        layout.constraints.preferredWidth = {UiLayoutUnit::Content, 0.0f};
        layout.constraints.preferredHeight = {UiLayoutUnit::Content, 0.0f};
        layout.constraints.horizontalAlignment = UiLayoutAlignment::Start;
        layout.constraints.verticalAlignment = UiLayoutAlignment::Start;

        const UiHandle handle = ui.createNode(UiNodeType::Text, parent);
        ui.setLayout(handle, layout);
        ui.setPayload(handle, UiTextData{text, {}, {1.0f, 1.0f, 1.0f, 1.0f}, 0.01f});
        return handle;
    }

    const UiRectPrimitive* findRectPrimitive(const UiDocument& document, UiHandle handle)
    {
        for (const UiVisualPrimitive& primitive : document.getVisualList().primitives)
        {
            if (const auto* rect = std::get_if<UiRectPrimitive>(&primitive))
            {
                if (rect->source == handle)
                    return rect;
            }
        }
        return nullptr;
    }

    const UiTextPrimitive* findTextPrimitive(const UiDocument& document, UiHandle handle)
    {
        for (const UiVisualPrimitive& primitive : document.getVisualList().primitives)
        {
            if (const auto* text = std::get_if<UiTextPrimitive>(&primitive))
            {
                if (text->source == handle)
                    return text;
            }
        }
        return nullptr;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    void extract(UiSystem& ui, UiSurfaceId surface)
    {
        ui.extractSurfaceCommandsRef(surface, 800, 600);
    }

    void testSemanticTokensMetricsAndTypography()
    {
        UiTheme theme;
        theme.setColor("Accent", {0.2f, 0.4f, 0.6f, 1.0f});
        theme.setMetric("PaddingMedium", 0.125f);

        UiTypography body;
        body.scale = 0.031f;
        body.weight = 500;
        body.lineHeight = 1.2f;
        theme.setTypography("Body", body);

        require(nearColor(theme.colorOr("Accent", {}), {0.2f, 0.4f, 0.6f, 1.0f}),
                "semantic color lookup");
        require(near(theme.metricOr("PaddingMedium", 0.0f), 0.125f), "metric lookup");

        UiTypography resolved;
        require(theme.resolveTypography("Body", resolved), "typography lookup");
        require(near(resolved.scale, 0.031f), "typography scale");
        require(resolved.weight == 500, "typography weight");
    }

    void testStyleInheritanceStateAndFallback()
    {
        UiTheme theme;
        theme.setColor("BaseBg", {0.1f, 0.2f, 0.3f, 1.0f});
        theme.setColor("HoverBg", {0.4f, 0.5f, 0.6f, 1.0f});
        theme.setColor("Text", {0.8f, 0.9f, 1.0f, 1.0f});

        UiStyleClass button;
        button.base.backgroundColorToken = "BaseBg";
        button.base.foregroundColorToken = "Text";
        button.states[UiStyleState::Hover].backgroundColorToken = "HoverBg";
        theme.setStyleClass("Button", button);

        UiStyleClass primary;
        primary.parentClass = "Button";
        primary.base.opacity = 0.75f;
        theme.setStyleClass("PrimaryButton", primary);

        UiComputedStyle normal;
        require(theme.resolveStyle("PrimaryButton", UiStyleState::Normal, normal), "normal style");
        require(nearColor(normal.backgroundColor, {0.1f, 0.2f, 0.3f, 1.0f}), "inherited normal background");
        require(nearColor(normal.foregroundColor, {0.8f, 0.9f, 1.0f, 1.0f}), "inherited foreground");
        require(normal.hasOpacity && near(normal.opacity, 0.75f), "child style opacity");

        UiComputedStyle hover;
        require(theme.resolveStyle("PrimaryButton", UiStyleState::Hover, hover), "hover style");
        require(nearColor(hover.backgroundColor, {0.4f, 0.5f, 0.6f, 1.0f}), "state background");

        UiComputedStyle missing;
        require(!theme.resolveStyle("Missing", UiStyleState::Normal, missing), "missing style fallback");
    }

    void testThemeHierarchyUsesChildTokens()
    {
        UiTheme base;
        base.setColor("PanelBg", {0.1f, 0.1f, 0.1f, 1.0f});

        UiStyleClass panel;
        panel.base.backgroundColorToken = "PanelBg";
        base.setStyleClass("Panel", panel);

        UiTheme child;
        child.setParent(base);
        child.setColor("PanelBg", {0.7f, 0.2f, 0.1f, 1.0f});

        UiComputedStyle resolved;
        require(child.resolveStyle("Panel", UiStyleState::Normal, resolved), "parent style class lookup");
        require(nearColor(resolved.backgroundColor, {0.7f, 0.2f, 0.1f, 1.0f}),
                "parent style class resolved through child semantic token");
    }

    void testPanelSkinsAndStateSkins()
    {
        UiTheme theme;

        UiPanelSkin skin;
        skin.type = UiPanelSkinType::SolidColor;
        skin.color = {0.2f, 0.3f, 0.4f, 0.9f};
        skin.contentPadding = {0.01f, 0.02f, 0.03f, 0.04f};
        theme.setSkin("RaisedPanel", skin);

        UiStyleClass panel;
        panel.base.skinName = "RaisedPanel";
        theme.setStyleClass("Panel", panel);

        UiComputedStyle resolved;
        require(theme.resolveStyle("Panel", UiStyleState::Normal, resolved), "skin style lookup");
        require(resolved.hasSkin, "computed style skin");
        require(resolved.skin.type == UiPanelSkinType::SolidColor, "computed skin type");
        require(nearColor(resolved.skin.color, {0.2f, 0.3f, 0.4f, 0.9f}), "computed skin color");

        UiPanelStateSkin states;
        states.normal = skin;
        states.hover = skin;
        states.hover->color = {0.5f, 0.6f, 0.7f, 1.0f};
        theme.setPanelStateSkin("PanelStates", states);

        UiPanelStateSkin resolvedStates;
        require(theme.resolvePanelStateSkin("PanelStates", resolvedStates), "panel state skin lookup");
        require(resolvedStates.hover.has_value(), "panel state hover skin exists");
        require(nearColor(resolvedStates.hover->color, {0.5f, 0.6f, 0.7f, 1.0f}),
                "panel state hover skin");
    }

    void testThemeDrivenRenderingAndSwitching()
    {
        UiSystem ui(nullptr);

        UiTheme theme = defaultUiTheme();
        theme.setColor("DemoPanel", {0.8f, 0.1f, 0.2f, 1.0f});
        UiStyleClass panel;
        panel.base.backgroundColorToken = "DemoPanel";
        theme.setStyleClass("DemoPanel", panel);
        ui.setTheme(theme);

        UiLayoutSpec rootLayout = fillLayout();
        rootLayout.layoutMode = UiLayoutMode::Overlay;
        const UiHandle root = ui.createNode(UiNodeType::Container, ui.getRoot());
        ui.setLayout(root, rootLayout);

        const UiHandle styled = createRect(ui, root, {0.0f, 0.0f, 0.0f, 1.0f});
        ui.setStyleClass(styled, "DemoPanel");

        const UiHandle raw = createRect(ui, root, {0.2f, 0.7f, 0.3f, 1.0f});

        extract(ui);
        const UiRectPrimitive* styledRect = findRectPrimitive(ui.getDocument(), styled);
        const UiRectPrimitive* rawRect = findRectPrimitive(ui.getDocument(), raw);
        require(styledRect != nullptr, "styled rect primitive");
        require(rawRect != nullptr, "raw rect primitive");
        require(nearColor(styledRect->color, {0.8f, 0.1f, 0.2f, 1.0f}), "theme-driven rect color");
        require(nearColor(rawRect->color, {0.2f, 0.7f, 0.3f, 1.0f}), "payload color compatibility");

        theme.setColor("DemoPanel", {0.1f, 0.4f, 0.9f, 1.0f});
        ui.setTheme(theme);
        extract(ui);

        styledRect = findRectPrimitive(ui.getDocument(), styled);
        require(styledRect != nullptr, "styled rect primitive after switch");
        require(nearColor(styledRect->color, {0.1f, 0.4f, 0.9f, 1.0f}), "runtime theme switch");
    }

    void testSurfaceLocalThemes()
    {
        UiSystem ui(nullptr);

        UiSurfaceDesc surfaceDesc;
        surfaceDesc.name = "tool";
        surfaceDesc.order = 10;
        surfaceDesc.rect = {0.0f, 0.0f, 1.0f, 1.0f};
        const UiSurfaceId surface = ui.createSurface(surfaceDesc);

        UiStyleClass panelClass;
        panelClass.base.backgroundColorToken = "Panel";

        UiTheme defaultTheme = defaultUiTheme();
        defaultTheme.setColor("Panel", {0.9f, 0.0f, 0.0f, 1.0f});
        defaultTheme.setStyleClass("Panel", panelClass);
        ui.setTheme(defaultTheme);

        UiTheme toolTheme = defaultUiTheme();
        toolTheme.setColor("Panel", {0.0f, 0.0f, 0.9f, 1.0f});
        toolTheme.setStyleClass("Panel", panelClass);
        ui.setTheme(surface, toolTheme);

        const UiHandle defaultRect = createRect(ui, ui.getRoot(), {1.0f, 1.0f, 1.0f, 1.0f});
        ui.setStyleClass(defaultRect, "Panel");

        const UiHandle surfaceRect = createRect(ui, surface, ui.getRoot(surface), {1.0f, 1.0f, 1.0f, 1.0f});
        ui.setStyleClass(surface, surfaceRect, "Panel");

        extract(ui);
        extract(ui, surface);

        const UiRectPrimitive* defaultPrimitive = findRectPrimitive(ui.getDocument(), defaultRect);
        const UiDocument* surfaceDocument = ui.findDocument(surface);
        require(surfaceDocument != nullptr, "surface document");
        const UiRectPrimitive* surfacePrimitive = findRectPrimitive(*surfaceDocument, surfaceRect);

        require(defaultPrimitive != nullptr, "default surface primitive");
        require(surfacePrimitive != nullptr, "tool surface primitive");
        require(nearColor(defaultPrimitive->color, {0.9f, 0.0f, 0.0f, 1.0f}), "default surface theme");
        require(nearColor(surfacePrimitive->color, {0.0f, 0.0f, 0.9f, 1.0f}), "tool surface theme");
    }

    void testTypographyResolutionAffectsLayoutAndVisuals()
    {
        UiSystem ui(nullptr);
        UiTheme theme = defaultUiTheme();

        UiTypography textTypography;
        textTypography.scale = 0.050f;
        textTypography.weight = 400;
        textTypography.lineHeight = 1.0f;
        theme.setTypography("DemoText", textTypography);
        theme.setColor("DemoTextColor", {0.3f, 0.8f, 0.2f, 1.0f});

        UiStyleClass textStyle;
        textStyle.base.typographyName = "DemoText";
        textStyle.base.foregroundColorToken = "DemoTextColor";
        theme.setStyleClass("DemoText", textStyle);
        ui.setTheme(theme);

        UiLayoutSpec rootLayout = fillLayout();
        rootLayout.layoutMode = UiLayoutMode::Overlay;
        const UiHandle root = ui.createNode(UiNodeType::Container, ui.getRoot());
        ui.setLayout(root, rootLayout);

        const UiHandle text = createText(ui, root, "themed text");
        ui.setStyleClass(text, "DemoText");

        extract(ui);

        const UiNode* node = ui.findNode(text);
        require(node != nullptr, "text node");
        require(node->computedLayout.bounds.width > 0.20f, "typography scale affected measured width");

        const UiTextPrimitive* primitive = findTextPrimitive(ui.getDocument(), text);
        require(primitive != nullptr, "text primitive");
        require(near(primitive->scale, 0.050f), "typography scale in visual primitive");
        require(nearColor(primitive->color, {0.3f, 0.8f, 0.2f, 1.0f}), "typography foreground color");
    }

    void testExplicitNodeStyleOverridesThemeClass()
    {
        UiTheme theme = defaultUiTheme();
        theme.setColor("Background", {0.0f, 0.0f, 0.0f, 1.0f});

        UiStyleClass panel;
        panel.base.backgroundColorToken = "Background";
        theme.setStyleClass("Panel", panel);

        UiStyle style;
        style.styleClass = "Panel";
        style.useBackgroundColor = true;
        style.backgroundColor = {0.6f, 0.4f, 0.2f, 1.0f};

        UiComputedStyle resolved;
        require(theme.resolveNodeStyle(style, UiStateFlags{}, resolved), "node style override resolution");
        require(nearColor(resolved.backgroundColor, {0.6f, 0.4f, 0.2f, 1.0f}),
                "explicit node style overrides theme class");
    }
}

int main()
{
    testSemanticTokensMetricsAndTypography();
    testStyleInheritanceStateAndFallback();
    testThemeHierarchyUsesChildTokens();
    testPanelSkinsAndStateSkins();
    testThemeDrivenRenderingAndSwitching();
    testSurfaceLocalThemes();
    testTypographyResolutionAffectsLayoutAndVisuals();
    testExplicitNodeStyleOverridesThemeClass();
    return 0;
}
