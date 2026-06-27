#pragma once

#include "EditorTheme.h"
#include "UiTypes.h"

namespace gts::tools
{
    struct ToolTheme
    {
        static constexpr UiColor windowBackground   = DefaultEditorTheme.colors.windowBackground;
        static constexpr UiColor barBackground      = DefaultEditorTheme.colors.barBackground;
        static constexpr UiColor railBackground     = DefaultEditorTheme.colors.railBackground;
        static constexpr UiColor paneBackground     = DefaultEditorTheme.colors.panelBackground;
        static constexpr UiColor paneSurface        = DefaultEditorTheme.colors.panelSurface;
        static constexpr UiColor viewportBar        = DefaultEditorTheme.colors.toolbarBackground;
        static constexpr UiColor button             = DefaultEditorTheme.colors.button;
        static constexpr UiColor buttonHover        = DefaultEditorTheme.colors.buttonHover;
        static constexpr UiColor buttonPressed      = DefaultEditorTheme.colors.buttonPressed;
        static constexpr UiColor buttonActive       = DefaultEditorTheme.colors.buttonActive;
        static constexpr UiColor toggleActive       = DefaultEditorTheme.colors.toggleActive;
        static constexpr UiColor toggleHover        = DefaultEditorTheme.colors.toggleHover;
        static constexpr UiColor disabled           = DefaultEditorTheme.colors.disabled;
        static constexpr UiColor inputBackground    = DefaultEditorTheme.colors.inputBackground;
        static constexpr UiColor inputHover         = DefaultEditorTheme.colors.inputHover;
        static constexpr UiColor sliderTrack        = DefaultEditorTheme.colors.sliderTrack;
        static constexpr UiColor sectionHeader      = DefaultEditorTheme.colors.sectionHeader;
        static constexpr UiColor sectionHeaderHover = DefaultEditorTheme.colors.sectionHeaderHover;
        static constexpr UiColor border             = DefaultEditorTheme.colors.border;
        static constexpr UiColor focus              = DefaultEditorTheme.colors.focus;
        static constexpr UiColor selection          = DefaultEditorTheme.colors.selection;
        static constexpr UiColor accent             = DefaultEditorTheme.colors.accent;
        static constexpr UiColor accentSoft         = DefaultEditorTheme.colors.accentSoft;
        static constexpr UiColor text               = DefaultEditorTheme.colors.text;
        static constexpr UiColor mutedText          = DefaultEditorTheme.colors.mutedText;
        static constexpr UiColor statusText         = DefaultEditorTheme.colors.statusText;
        static constexpr UiColor info               = DefaultEditorTheme.colors.info;
        static constexpr UiColor success            = DefaultEditorTheme.colors.success;
        static constexpr UiColor warning            = DefaultEditorTheme.colors.warning;
        static constexpr UiColor error              = DefaultEditorTheme.colors.error;
        static constexpr UiColor axisX              = DefaultEditorTheme.colors.axisX;
        static constexpr UiColor axisY              = DefaultEditorTheme.colors.axisY;
        static constexpr UiColor axisZ              = DefaultEditorTheme.colors.axisZ;
        static constexpr UiColor alpha              = DefaultEditorTheme.colors.alpha;
        static constexpr UiColor rotation           = DefaultEditorTheme.colors.rotation;
        static constexpr UiColor scale              = DefaultEditorTheme.colors.scale;
        static constexpr UiColor secondaryAccent    = DefaultEditorTheme.colors.secondaryAccent;

        static constexpr float shellPadding     = DefaultEditorTheme.spacing.shellPadding;
        static constexpr float panelInset       = DefaultEditorTheme.spacing.panelInset;
        static constexpr float panelWidth       = 0.920f;
        static constexpr float headerTextScale  = DefaultEditorTheme.typography.headerScale;
        static constexpr float bodyTextScale    = DefaultEditorTheme.typography.bodyScale;
        static constexpr float smallTextScale   = DefaultEditorTheme.typography.smallScale;
        static constexpr float buttonTextScale  = DefaultEditorTheme.typography.buttonScale;
        static constexpr float railIconScale    = DefaultEditorTheme.typography.iconScale;
        static constexpr float titleTextScale   = DefaultEditorTheme.typography.titleScale;
        static constexpr float rowHeight        = DefaultEditorTheme.dimensions.rowHeight;
        static constexpr float compactRowHeight = DefaultEditorTheme.dimensions.compactRowHeight;
        static constexpr float sliderHeight     = DefaultEditorTheme.dimensions.sliderHeight;
    };
} // namespace gts::tools
