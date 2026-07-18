#pragma once

#include "UiTypes.h"

namespace gts::tools
{
    struct EditorTypography
    {
        const char* fontAsset = "resources/fonts/editor_sans.font.json";

        int regularWeight = 400;
        int mediumWeight  = 500;
        int boldWeight    = 700;

        float applicationTitleScale = 0.0118f;
        float panelTitleScale       = 0.0108f;
        float sectionHeaderScale    = 0.0098f;
        float bodyScale             = 0.0092f;
        float labelScale            = 0.0088f;
        float valueScale            = 0.0088f;
        float metadataScale         = 0.0084f;
        float statusScale           = 0.0082f;
        float diagnosticScale       = 0.0082f;
        float buttonScale           = 0.0086f;
        float iconScale             = 0.0090f;

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

        float shellPadding    = 0.0045f;
        float panelInset      = 0.0140f;
        float panelGap        = 0.0040f;
        float toolbarGap      = 0.0050f;
        float toolbarGroupGap = 0.0100f;
        float sectionGap      = 0.0060f;
        float widgetGap       = 0.0034f;
        float propertyGap     = 0.0038f;
        float statusGap       = 0.0100f;

        UiThickness panelPadding   = {0.018f, 0.022f, 0.018f, 0.022f};
        UiThickness sectionPadding = {0.018f, 0.018f, 0.018f, 0.018f};
        UiThickness fieldPadding   = {0.020f, 0.105f, 0.020f, 0.105f};
    };

    struct EditorBorders
    {
        float hairlineWidth = 0.0008f;
        float thinWidth     = 0.0011f;
        float focusWidth    = 0.0018f;

        float radiusSmall  = 0.0042f;
        float radiusMedium = 0.0062f;
        float radiusLarge  = 0.0080f;
    };

    struct EditorShadow
    {
        UiColor color   = {0.000f, 0.000f, 0.000f, 0.420f};
        float   offsetX = 0.000f;
        float   offsetY = 0.006f;
        float   blur    = 0.016f;
    };

    struct EditorAnimation
    {
        float fastSeconds     = 0.080f;
        float standardSeconds = 0.140f;
        float slowSeconds     = 0.220f;
    };

    struct EditorWidgetDimensions
    {
        float rowHeight             = 0.030f;
        float compactRowHeight      = 0.025f;
        float propertyRowHeight     = 0.033f;
        float sliderHeight          = 0.028f;
        float buttonHeight          = 0.034f;
        float iconButtonSize        = 0.028f;
        float tabHeight             = 0.020f;
        float toolbarHeight         = 0.052f;
        float menuBarHeight         = 0.024f;
        float statusBarHeight       = 0.022f;
        float panelHeaderHeight     = 0.032f;
        float sidebarWidth          = 0.032f;
        float inspectorWidth        = 0.245f;
        float scrollbarSize         = 0.010f;
        float scrollbarMinThumbSize = 0.045f;
    };

    struct EditorColors
    {
        UiColor windowBackground    = {0.007f, 0.009f, 0.012f, 1.000f};
        UiColor workspaceBackground = {0.014f, 0.018f, 0.024f, 1.000f};
        UiColor viewportBackground  = {0.010f, 0.014f, 0.020f, 1.000f};
        UiColor menuBarBackground   = {0.011f, 0.014f, 0.018f, 0.998f};
        UiColor toolbarBackground   = {0.023f, 0.029f, 0.037f, 0.996f};
        UiColor statusBarBackground = {0.011f, 0.014f, 0.018f, 0.998f};
        UiColor sidebarBackground   = {0.024f, 0.031f, 0.041f, 1.000f};
        UiColor panelBackground     = {0.033f, 0.041f, 0.052f, 0.988f};
        UiColor panelSurface        = {0.045f, 0.055f, 0.068f, 0.992f};
        UiColor panelSurfaceRaised  = {0.062f, 0.074f, 0.090f, 0.998f};
        UiColor panelInset          = {0.014f, 0.019f, 0.027f, 0.998f};
        UiColor cardBackground      = {0.047f, 0.057f, 0.071f, 0.992f};
        UiColor groupBackground     = {0.030f, 0.038f, 0.049f, 0.970f};
        UiColor overlay             = {0.010f, 0.013f, 0.019f, 0.900f};
        UiColor toolbarGroupBackground = {0.036f, 0.045f, 0.057f, 0.960f};
        UiColor viewportOverlayBackground = {0.018f, 0.023f, 0.031f, 0.860f};

        UiColor headerBackground      = {0.038f, 0.047f, 0.058f, 0.998f};
        UiColor headerActive          = {0.056f, 0.069f, 0.085f, 1.000f};
        UiColor footerBackground      = statusBarBackground;
        UiColor separator             = {0.130f, 0.158f, 0.190f, 0.650f};
        UiColor border                = {0.230f, 0.270f, 0.320f, 0.820f};
        UiColor borderSubtle          = {0.130f, 0.158f, 0.190f, 0.680f};
        UiColor highlight             = {0.640f, 0.760f, 0.880f, 0.145f};

        UiColor button          = {0.040f, 0.050f, 0.063f, 0.985f};
        UiColor buttonSecondary = {0.030f, 0.038f, 0.048f, 0.930f};
        UiColor buttonHover     = {0.074f, 0.090f, 0.109f, 1.000f};
        UiColor buttonPressed   = {0.100f, 0.123f, 0.148f, 1.000f};
        UiColor buttonActive    = {0.050f, 0.230f, 0.360f, 1.000f};
        UiColor toggleActive    = {0.050f, 0.250f, 0.385f, 1.000f};
        UiColor toggleHover     = {0.070f, 0.300f, 0.455f, 1.000f};
        UiColor disabled        = {0.020f, 0.024f, 0.030f, 0.660f};

        UiColor inputBackground    = {0.014f, 0.019f, 0.026f, 1.000f};
        UiColor inputHover         = {0.041f, 0.050f, 0.061f, 1.000f};
        UiColor inputActive        = {0.050f, 0.065f, 0.082f, 1.000f};
        UiColor sliderTrack        = {0.026f, 0.032f, 0.039f, 1.000f};
        UiColor sliderTrackHover   = {0.048f, 0.059f, 0.071f, 1.000f};
        UiColor scrollbarTrack     = {0.020f, 0.025f, 0.031f, 0.760f};
        UiColor scrollbarThumb     = {0.160f, 0.190f, 0.220f, 0.940f};
        UiColor sectionHeader      = {0.030f, 0.038f, 0.049f, 1.000f};
        UiColor sectionHeaderHover = {0.050f, 0.062f, 0.077f, 1.000f};

        UiColor focus         = {0.260f, 0.610f, 0.880f, 1.000f};
        UiColor selection     = {0.040f, 0.180f, 0.285f, 1.000f};
        UiColor selectionSoft = {0.035f, 0.120f, 0.185f, 0.860f};
        UiColor rowBackground = {0.028f, 0.035f, 0.045f, 0.985f};
        UiColor rowHover      = {0.055f, 0.068f, 0.084f, 1.000f};
        UiColor rowSelected   = {0.052f, 0.195f, 0.310f, 1.000f};
        UiColor rowAccent     = {0.230f, 0.590f, 0.880f, 1.000f};
        UiColor inspectorRowBackground = {0.027f, 0.034f, 0.044f, 0.980f};
        UiColor hover         = {0.068f, 0.082f, 0.100f, 1.000f};
        UiColor pressed       = {0.095f, 0.116f, 0.140f, 1.000f};
        UiColor accent        = {0.230f, 0.590f, 0.880f, 1.000f};
        UiColor accentSoft    = {0.095f, 0.230f, 0.335f, 1.000f};
        UiColor accentMuted   = {0.125f, 0.220f, 0.290f, 1.000f};
        UiColor primaryAccent = accent;

        UiColor text         = {0.910f, 0.930f, 0.955f, 1.000f};
        UiColor textPrimary  = text;
        UiColor textSecondary = {0.620f, 0.675f, 0.735f, 1.000f};
        UiColor mutedText     = textSecondary;
        UiColor textDisabled  = {0.350f, 0.395f, 0.450f, 1.000f};
        UiColor statusText    = {0.700f, 0.755f, 0.810f, 1.000f};
        UiColor icon          = {0.735f, 0.785f, 0.835f, 1.000f};
        UiColor iconMuted     = {0.440f, 0.495f, 0.555f, 1.000f};

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
