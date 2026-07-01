#pragma once

#include "UiWidgetAsset.h"

class UiTheme;

namespace gts::tools
{
    void registerVisualUiEditorSampleThemeClasses(UiTheme& theme);
    UiWidgetAssetDefinition createVisualUiEditorStatusPromptAsset();
    UiWidgetAssetDefinition createVisualUiEditorInteractionPromptAsset();
}
