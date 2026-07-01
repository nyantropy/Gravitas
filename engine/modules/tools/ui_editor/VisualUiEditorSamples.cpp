#include "VisualUiEditorSamples.h"

#include <stdexcept>

#include "UiTheme.h"

namespace gts::tools
{
    namespace
    {
        UiWidgetAssetDefinition parseSample(const char* json)
        {
            UiWidgetAssetDefinition asset;
            UiSerializedValidationResult validation;
            if (!parseUiWidgetAssetDefinition(json, asset, &validation) || !validation.valid())
                throw std::runtime_error("Visual UI editor sample asset failed to parse");
            return asset;
        }
    }

    void registerVisualUiEditorSampleThemeClasses(UiTheme& theme)
    {
        theme.setStyleClass("Panel.Prompt", UiStyleClass{});
        theme.setStyleClass("Panel.InteractionPrompt", UiStyleClass{});
        theme.setStyleClass("Text.Prompt", UiStyleClass{});
        theme.setStyleClass("Text.InteractionPrompt", UiStyleClass{});
        theme.setStyleClass("Text.EditorPreview", UiStyleClass{});
        theme.setStyleClass("Text.Body", UiStyleClass{});
    }

    UiWidgetAssetDefinition createVisualUiEditorStatusPromptAsset()
    {
        return parseSample(R"json({
  "schema": 1,
  "id": "engine.status_prompt",
  "version": 1,
  "displayName": "Status Prompt",
  "description": "Reusable prompt shell used by the Visual UI Editor sample workflow.",
  "tags": ["prompt", "sample", "ui-editor"],
  "parameters": [
    {"name": "message", "type": "String", "default": "Ready"},
    {"name": "style", "type": "String", "default": "Panel.Prompt"},
    {"name": "textStyle", "type": "String", "default": "Text.Prompt"}
  ],
  "root": {
    "id": "prompt",
    "type": "Panel",
    "styleClass": "{{style}}",
    "layout": {
      "type": "Stack",
      "direction": "Vertical",
      "padding": {"left": {"unit": "Normalized", "value": 0.020}, "right": {"unit": "Normalized", "value": 0.020}, "top": {"unit": "Normalized", "value": 0.010}, "bottom": {"unit": "Normalized", "value": 0.010}},
      "constraints": {
        "preferredWidth": {"unit": "Normalized", "value": 0.42},
        "preferredHeight": {"unit": "Normalized", "value": 0.11}
      }
    },
    "semantics": {"role": "Status", "name": "{{message}}"},
    "children": [
      {
        "id": "label",
        "type": "Label",
        "text": "{{message}}",
        "styleClass": "{{textStyle}}",
        "horizontalAlign": "Center",
        "verticalAlign": "Middle",
        "layout": {
          "constraints": {
            "preferredHeight": {"unit": "Normalized", "value": 0.060}
          }
        },
        "semantics": {"role": "Label", "name": "{{message}}"}
      }
    ]
  }
})json");
    }

    UiWidgetAssetDefinition createVisualUiEditorInteractionPromptAsset()
    {
        return parseSample(R"json({
  "schema": 1,
  "id": "dungeon.interaction_prompt",
  "version": 1,
  "base": "engine.status_prompt",
  "displayName": "Interaction Prompt",
  "description": "Dungeon prompt asset used to demonstrate opening, editing, validating, previewing, and saving authored UI.",
  "tags": ["prompt", "dungeon", "interaction"],
  "parameters": [
    {"name": "message", "type": "String", "default": "Press E to interact"},
    {"name": "style", "type": "String", "default": "Panel.InteractionPrompt"},
    {"name": "textStyle", "type": "String", "default": "Text.InteractionPrompt"}
  ],
  "variants": [
    {
      "name": "keyboard",
      "displayName": "Keyboard",
      "parameters": {"message": "Press E to interact"}
    },
    {
      "name": "controller",
      "displayName": "Controller",
      "parameters": {"message": "Press A to interact"}
    }
  ],
  "root": {
    "id": "prompt",
    "type": "Panel",
    "styleClass": "{{style}}",
    "layout": {
      "type": "Stack",
      "direction": "Vertical",
      "padding": {"left": {"unit": "Normalized", "value": 0.020}, "right": {"unit": "Normalized", "value": 0.020}, "top": {"unit": "Normalized", "value": 0.010}, "bottom": {"unit": "Normalized", "value": 0.010}},
      "constraints": {
        "preferredWidth": {"unit": "Normalized", "value": 0.42},
        "preferredHeight": {"unit": "Normalized", "value": 0.11}
      }
    },
    "semantics": {"role": "Status", "name": "{{message}}"},
    "children": [
      {
        "id": "label",
        "type": "Label",
        "text": "{{message}}",
        "styleClass": "{{textStyle}}",
        "horizontalAlign": "Center",
        "verticalAlign": "Middle",
        "layout": {
          "constraints": {
            "preferredHeight": {"unit": "Normalized", "value": 0.060}
          }
        },
        "semantics": {"role": "Label", "name": "{{message}}"}
      }
    ]
  }
})json");
    }
}
