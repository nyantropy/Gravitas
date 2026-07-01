#pragma once

#include "UiPackageRuntime.h"
#include "UiWidgetAsset.h"

class UiTheme;

namespace gts::tools
{
    void registerVisualUiEditorSampleThemeClasses(UiTheme& theme);
    UiWidgetAssetDefinition createVisualUiEditorStatusPromptAsset();
    UiWidgetAssetDefinition createVisualUiEditorInteractionPromptAsset();
    UiPackageDesc createVisualUiEditorEngineUiPackage();
    UiPackageDesc createVisualUiEditorGameUiPackage();
}
