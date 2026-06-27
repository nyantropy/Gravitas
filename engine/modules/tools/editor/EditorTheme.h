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

        float titleScale  = 0.0108f;
        float headerScale = 0.0105f;
        float bodyScale   = 0.0088f;
        float smallScale  = 0.0078f;
        float buttonScale = 0.0082f;
        float iconScale   = 0.0110f;
    };

    struct EditorSpacing
    {
        float xxs = 0.002f;
        float xs  = 0.004f;
        float sm  = 0.006f;
        float md  = 0.010f;
        float lg  = 0.016f;
        float xl  = 0.024f;

        float shellPadding = 0.006f;
        float panelInset   = 0.040f;
        float panelGap     = 0.010f;

        UiThickness panelPadding = {0.025f, 0.030f, 0.025f, 0.030f};
        UiThickness fieldPadding = {0.025f, 0.120f, 0.025f, 0.120f};
    };

    struct EditorBorders
    {
        float hairlineWidth = 0.0010f;
        float thinWidth     = 0.0015f;
        float focusWidth    = 0.0020f;

        float radiusSmall  = 0.003f;
        float radiusMedium = 0.005f;
        float radiusLarge  = 0.008f;
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
        float rowHeight             = 0.036f;
        float compactRowHeight      = 0.031f;
        float sliderHeight          = 0.030f;
        float buttonHeight          = 0.040f;
        float tabHeight             = 0.016f;
        float toolbarHeight         = 0.024f;
        float statusBarHeight       = 0.022f;
        float sidebarWidth          = 0.030f;
        float inspectorWidth        = 0.170f;
        float scrollbarSize         = 0.010f;
        float scrollbarMinThumbSize = 0.045f;
    };

    struct EditorColors
    {
        UiColor windowBackground  = {0.014f, 0.017f, 0.020f, 0.990f};
        UiColor barBackground     = {0.025f, 0.029f, 0.034f, 0.980f};
        UiColor toolbarBackground = {0.018f, 0.021f, 0.025f, 0.900f};
        UiColor railBackground    = {0.020f, 0.024f, 0.029f, 0.990f};
        UiColor panelBackground   = {0.027f, 0.031f, 0.036f, 0.980f};
        UiColor panelSurface      = {0.038f, 0.044f, 0.050f, 0.960f};
        UiColor overlay           = {0.010f, 0.012f, 0.015f, 0.860f};

        UiColor button        = {0.067f, 0.077f, 0.088f, 0.960f};
        UiColor buttonHover   = {0.092f, 0.105f, 0.118f, 1.000f};
        UiColor buttonPressed = {0.118f, 0.137f, 0.150f, 1.000f};
        UiColor buttonActive  = {0.105f, 0.172f, 0.210f, 1.000f};
        UiColor toggleActive  = {0.115f, 0.205f, 0.155f, 1.000f};
        UiColor toggleHover   = {0.145f, 0.260f, 0.195f, 1.000f};
        UiColor disabled      = {0.040f, 0.046f, 0.052f, 0.720f};

        UiColor inputBackground    = {0.045f, 0.052f, 0.060f, 1.000f};
        UiColor inputHover         = {0.060f, 0.070f, 0.080f, 1.000f};
        UiColor sliderTrack        = {0.055f, 0.063f, 0.071f, 1.000f};
        UiColor sectionHeader      = {0.032f, 0.038f, 0.044f, 1.000f};
        UiColor sectionHeaderHover = {0.092f, 0.105f, 0.118f, 1.000f};

        UiColor border     = {0.135f, 0.150f, 0.165f, 0.880f};
        UiColor focus      = {0.260f, 0.560f, 0.780f, 1.000f};
        UiColor selection  = {0.105f, 0.172f, 0.210f, 1.000f};
        UiColor hover      = {0.092f, 0.105f, 0.118f, 1.000f};
        UiColor accent     = {0.260f, 0.560f, 0.780f, 1.000f};
        UiColor accentSoft = {0.180f, 0.330f, 0.430f, 1.000f};

        UiColor text       = {0.880f, 0.920f, 0.950f, 1.000f};
        UiColor mutedText  = {0.580f, 0.650f, 0.700f, 1.000f};
        UiColor statusText = {0.720f, 0.780f, 0.830f, 1.000f};

        UiColor info    = {0.300f, 0.680f, 0.860f, 1.000f};
        UiColor success = {0.200f, 0.720f, 0.360f, 1.000f};
        UiColor warning = {0.880f, 0.620f, 0.220f, 1.000f};
        UiColor error   = {0.880f, 0.220f, 0.240f, 1.000f};

        UiColor axisX           = {0.880f, 0.280f, 0.300f, 1.000f};
        UiColor axisY           = {0.300f, 0.760f, 0.420f, 1.000f};
        UiColor axisZ           = {0.250f, 0.520f, 0.920f, 1.000f};
        UiColor alpha           = {0.720f, 0.760f, 0.820f, 1.000f};
        UiColor rotation        = {0.800f, 0.580f, 0.240f, 1.000f};
        UiColor scale           = {0.580f, 0.660f, 0.860f, 1.000f};
        UiColor secondaryAccent = {0.700f, 0.540f, 0.860f, 1.000f};
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
