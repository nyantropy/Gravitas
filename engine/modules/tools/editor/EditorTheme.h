#pragma once

#include "UiTypes.h"

namespace gts::tools
{
    struct EditorTypography
    {
        const char* fontAsset = "resources/fonts/gravitasfont.font.json";

        int regularWeight = 400;
        int mediumWeight  = 500;
        int boldWeight    = 700;

        float applicationTitleScale = 0.0108f;
        float panelTitleScale       = 0.0094f;
        float sectionHeaderScale    = 0.0086f;
        float bodyScale             = 0.0080f;
        float labelScale            = 0.0074f;
        float valueScale            = 0.0075f;
        float metadataScale         = 0.0068f;
        float statusScale           = 0.0069f;
        float diagnosticScale       = 0.0071f;
        float buttonScale           = 0.0074f;
        float iconScale             = 0.0102f;

        float titleScale  = applicationTitleScale;
        float headerScale = panelTitleScale;
        float smallScale  = metadataScale;
    };

    struct EditorSpacing
    {
        float xxs = 0.0015f;
        float xs  = 0.0030f;
        float sm  = 0.0050f;
        float md  = 0.0080f;
        float lg  = 0.0120f;
        float xl  = 0.0180f;
        float xxl = 0.0260f;

        float shellPadding    = 0.0060f;
        float panelInset      = 0.0180f;
        float panelGap        = 0.0055f;
        float toolbarGap      = 0.0060f;
        float toolbarGroupGap = 0.0130f;
        float sectionGap      = 0.0070f;
        float widgetGap       = 0.0040f;
        float propertyGap     = 0.0045f;
        float statusGap       = 0.0120f;

        UiThickness panelPadding   = {0.018f, 0.022f, 0.018f, 0.022f};
        UiThickness sectionPadding = {0.018f, 0.018f, 0.018f, 0.018f};
        UiThickness fieldPadding   = {0.020f, 0.105f, 0.020f, 0.105f};
    };

    struct EditorBorders
    {
        float hairlineWidth = 0.0008f;
        float thinWidth     = 0.0011f;
        float focusWidth    = 0.0018f;

        float radiusSmall  = 0.0025f;
        float radiusMedium = 0.0040f;
        float radiusLarge  = 0.0060f;
    };

    struct EditorShadow
    {
        UiColor color   = {0.000f, 0.000f, 0.000f, 0.420f};
        float   offsetX = 0.000f;
        float   offsetY = 0.006f;
        float   blur    = 0.018f;
    };

    struct EditorAnimation
    {
        float fastSeconds     = 0.080f;
        float standardSeconds = 0.140f;
        float slowSeconds     = 0.220f;
    };

    struct EditorWidgetDimensions
    {
        float rowHeight             = 0.032f;
        float compactRowHeight      = 0.027f;
        float propertyRowHeight     = 0.038f;
        float sliderHeight          = 0.028f;
        float buttonHeight          = 0.034f;
        float iconButtonSize        = 0.028f;
        float tabHeight             = 0.020f;
        float toolbarHeight         = 0.052f;
        float menuBarHeight         = 0.024f;
        float statusBarHeight       = 0.022f;
        float panelHeaderHeight     = 0.034f;
        float sidebarWidth          = 0.032f;
        float inspectorWidth        = 0.245f;
        float scrollbarSize         = 0.010f;
        float scrollbarMinThumbSize = 0.045f;
    };

    struct EditorColors
    {
        UiColor windowBackground    = {0.025f, 0.029f, 0.034f, 0.995f};
        UiColor workspaceBackground = {0.031f, 0.036f, 0.043f, 1.000f};
        UiColor viewportBackground  = {0.021f, 0.026f, 0.038f, 1.000f};
        UiColor menuBarBackground   = {0.030f, 0.034f, 0.039f, 0.990f};
        UiColor toolbarBackground   = {0.044f, 0.050f, 0.058f, 0.985f};
        UiColor statusBarBackground = {0.028f, 0.033f, 0.038f, 0.995f};
        UiColor sidebarBackground   = {0.034f, 0.039f, 0.046f, 1.000f};
        UiColor panelBackground     = {0.048f, 0.055f, 0.064f, 0.985f};
        UiColor panelSurface        = {0.060f, 0.068f, 0.078f, 0.985f};
        UiColor panelSurfaceRaised  = {0.071f, 0.080f, 0.092f, 0.995f};
        UiColor panelInset          = {0.037f, 0.043f, 0.050f, 0.985f};
        UiColor cardBackground      = {0.055f, 0.063f, 0.073f, 0.980f};
        UiColor groupBackground     = {0.045f, 0.052f, 0.061f, 0.960f};
        UiColor overlay             = {0.015f, 0.018f, 0.023f, 0.875f};

        UiColor headerBackground      = {0.056f, 0.064f, 0.074f, 0.990f};
        UiColor headerActive          = {0.071f, 0.083f, 0.098f, 1.000f};
        UiColor footerBackground      = statusBarBackground;
        UiColor separator             = {0.125f, 0.142f, 0.160f, 0.620f};
        UiColor border                = {0.145f, 0.164f, 0.186f, 0.700f};
        UiColor borderSubtle          = {0.090f, 0.105f, 0.123f, 0.560f};
        UiColor highlight             = {0.550f, 0.660f, 0.750f, 0.120f};

        UiColor button          = {0.069f, 0.078f, 0.090f, 0.970f};
        UiColor buttonSecondary = {0.057f, 0.065f, 0.076f, 0.930f};
        UiColor buttonHover     = {0.096f, 0.111f, 0.130f, 1.000f};
        UiColor buttonPressed   = {0.130f, 0.151f, 0.176f, 1.000f};
        UiColor buttonActive    = {0.106f, 0.226f, 0.322f, 1.000f};
        UiColor toggleActive    = {0.096f, 0.245f, 0.362f, 1.000f};
        UiColor toggleHover     = {0.124f, 0.300f, 0.435f, 1.000f};
        UiColor disabled        = {0.045f, 0.051f, 0.060f, 0.620f};

        UiColor inputBackground    = {0.034f, 0.039f, 0.047f, 1.000f};
        UiColor inputHover         = {0.050f, 0.059f, 0.070f, 1.000f};
        UiColor inputActive        = {0.060f, 0.080f, 0.098f, 1.000f};
        UiColor sliderTrack        = {0.036f, 0.043f, 0.052f, 1.000f};
        UiColor sliderTrackHover   = {0.055f, 0.066f, 0.080f, 1.000f};
        UiColor scrollbarTrack     = {0.032f, 0.037f, 0.044f, 0.700f};
        UiColor scrollbarThumb     = {0.150f, 0.178f, 0.205f, 0.900f};
        UiColor sectionHeader      = {0.050f, 0.057f, 0.066f, 1.000f};
        UiColor sectionHeaderHover = {0.076f, 0.090f, 0.108f, 1.000f};

        UiColor focus         = {0.240f, 0.560f, 0.810f, 1.000f};
        UiColor selection     = {0.112f, 0.260f, 0.380f, 1.000f};
        UiColor selectionSoft = {0.090f, 0.190f, 0.280f, 0.740f};
        UiColor hover         = {0.072f, 0.085f, 0.102f, 1.000f};
        UiColor pressed       = {0.096f, 0.115f, 0.138f, 1.000f};
        UiColor accent        = {0.210f, 0.550f, 0.780f, 1.000f};
        UiColor accentSoft    = {0.115f, 0.245f, 0.345f, 1.000f};
        UiColor accentMuted   = {0.135f, 0.215f, 0.280f, 1.000f};
        UiColor primaryAccent = accent;

        UiColor text         = {0.885f, 0.910f, 0.940f, 1.000f};
        UiColor textPrimary  = text;
        UiColor textSecondary = {0.640f, 0.695f, 0.750f, 1.000f};
        UiColor mutedText     = textSecondary;
        UiColor textDisabled  = {0.400f, 0.450f, 0.505f, 1.000f};
        UiColor statusText    = {0.690f, 0.745f, 0.800f, 1.000f};
        UiColor icon          = {0.725f, 0.775f, 0.825f, 1.000f};
        UiColor iconMuted     = {0.470f, 0.525f, 0.585f, 1.000f};

        UiColor info    = {0.250f, 0.575f, 0.820f, 1.000f};
        UiColor success = {0.230f, 0.700f, 0.380f, 1.000f};
        UiColor warning = {0.890f, 0.645f, 0.210f, 1.000f};
        UiColor error   = {0.900f, 0.245f, 0.265f, 1.000f};

        UiColor graphBackground   = {0.034f, 0.041f, 0.050f, 1.000f};
        UiColor graphGridMajor    = {0.190f, 0.230f, 0.270f, 0.240f};
        UiColor graphGridMinor    = {0.165f, 0.195f, 0.230f, 0.120f};
        UiColor timelineTrack     = {0.044f, 0.052f, 0.061f, 1.000f};
        UiColor timelineRuler     = {0.060f, 0.072f, 0.084f, 1.000f};
        UiColor curveBlue         = {0.235f, 0.565f, 0.880f, 1.000f};
        UiColor curveRed          = {0.900f, 0.235f, 0.240f, 1.000f};
        UiColor curveGreen        = {0.260f, 0.720f, 0.380f, 1.000f};
        UiColor curveYellow       = {0.910f, 0.690f, 0.150f, 1.000f};
        UiColor curveViolet       = {0.650f, 0.500f, 0.840f, 1.000f};

        UiColor axisX           = curveRed;
        UiColor axisY           = curveGreen;
        UiColor axisZ           = curveBlue;
        UiColor alpha           = {0.720f, 0.760f, 0.820f, 1.000f};
        UiColor rotation        = curveYellow;
        UiColor scale           = {0.520f, 0.650f, 0.920f, 1.000f};
        UiColor secondaryAccent = curveViolet;

        UiColor menuBackground    = {0.046f, 0.054f, 0.064f, 0.980f};
        UiColor popupBackground   = {0.042f, 0.050f, 0.060f, 0.985f};
        UiColor tooltipBackground = {0.030f, 0.036f, 0.044f, 0.970f};

        UiColor barBackground  = menuBarBackground;
        UiColor railBackground = sidebarBackground;
    };

    struct EditorIconSet
    {
        const char* save      = "save";
        const char* reload    = "reload";
        const char* search    = "search";
        const char* add       = "add";
        const char* remove    = "remove";
        const char* duplicate = "duplicate";
        const char* copy      = "copy";
        const char* paste     = "paste";
        const char* undo      = "undo";
        const char* redo      = "redo";
        const char* play      = "play";
        const char* pause     = "pause";
        const char* reset     = "reset";
        const char* settings  = "settings";
        const char* hierarchy = "hierarchy";
        const char* inspector = "inspector";
        const char* graph     = "graph";
        const char* timeline  = "timeline";
        const char* assets    = "assets";
        const char* console   = "console";
        const char* warning   = "warning";
        const char* error     = "error";
        const char* success   = "success";
    };

    struct EditorTheme
    {
        EditorTypography       typography;
        EditorSpacing          spacing;
        EditorBorders          borders;
        EditorShadow           shadow;
        EditorAnimation        animation;
        EditorWidgetDimensions dimensions;
        EditorColors           colors;
        EditorIconSet          icons;
    };

    inline constexpr EditorTheme DefaultEditorTheme{};

    inline constexpr const EditorTheme& defaultEditorTheme()
    {
        return DefaultEditorTheme;
    }
} // namespace gts::tools
