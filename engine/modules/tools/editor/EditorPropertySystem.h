#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "EditorWidgets.h"
#include "UiTypes.h"

namespace gts::tools
{
    enum class EditorPropertyValueType
    {
        None,
        Bool,
        Int,
        UInt,
        Float,
        Text,
        Enum,
        Asset,
        Color,
        FloatRange,
        Tags,
        Curve,
        Gradient
    };

    struct EditorFloatRange
    {
        float low  = 0.0f;
        float high = 1.0f;
    };

    struct EditorPropertyValue
    {
        EditorPropertyValueType  type       = EditorPropertyValueType::None;
        bool                     boolValue  = false;
        int64_t                  intValue   = 0;
        uint64_t                 uintValue  = 0;
        float                    floatValue = 0.0f;
        std::string              textValue;
        UiColor                  colorValue = {1.0f, 1.0f, 1.0f, 1.0f};
        EditorFloatRange         rangeValue;
        std::vector<std::string> tags;
    };

    struct EditorEnumOption
    {
        std::string value;
        std::string label;
    };

    struct EditorPropertyLimits
    {
        bool  hasMin     = false;
        bool  hasMax     = false;
        bool  hasSoftMin = false;
        bool  hasSoftMax = false;
        float min        = 0.0f;
        float max        = 1.0f;
        float softMin    = 0.0f;
        float softMax    = 1.0f;
        float step       = 0.01f;
    };

    struct EditorPropertyVisibilityRule
    {
        std::string dependsOn;
        std::string equalsValue;
        bool        invert = false;
    };

    struct EditorPropertyMetadata
    {
        std::string                   id;
        std::string                   displayName;
        std::string                   description;
        std::string                   tooltip;
        std::string                   category;
        std::string                   group;
        EditorPropertyValueType       type = EditorPropertyValueType::Text;
        EditorPropertyValue           defaultValue;
        EditorPropertyLimits          limits;
        EditorPropertyVisibilityRule  visibilityRule;
        bool                          readOnly = false;
        std::string                   assetType;
        std::string                   units;
        std::vector<EditorEnumOption> enumOptions;
    };

    struct EditorPropertyDescriptor
    {
        EditorPropertyMetadata metadata;
        EditorPropertyValue    value;
    };

    inline std::string formatPropertyNumber(float value)
    {
        std::ostringstream out;
        out << std::fixed;
        if (std::abs(value) >= 10.0f)
            out.precision(3);
        else
            out.precision(2);
        out << value;
        return out.str();
    }

    inline std::string propertyValueToString(const EditorPropertyValue& value)
    {
        switch (value.type)
        {
        case EditorPropertyValueType::Bool:
            return value.boolValue ? "true" : "false";
        case EditorPropertyValueType::Int:
            return std::to_string(value.intValue);
        case EditorPropertyValueType::UInt:
            return std::to_string(value.uintValue);
        case EditorPropertyValueType::Float:
            return formatPropertyNumber(value.floatValue);
        case EditorPropertyValueType::Text:
        case EditorPropertyValueType::Enum:
        case EditorPropertyValueType::Asset:
            return value.textValue;
        case EditorPropertyValueType::Color:
            return formatPropertyNumber(value.colorValue.r) + ", " + formatPropertyNumber(value.colorValue.g) + ", " +
                   formatPropertyNumber(value.colorValue.b) + ", " + formatPropertyNumber(value.colorValue.a);
        case EditorPropertyValueType::FloatRange:
            return formatPropertyNumber(value.rangeValue.low) + " - " + formatPropertyNumber(value.rangeValue.high);
        case EditorPropertyValueType::Tags:
        {
            std::string text;
            for (size_t i = 0; i < value.tags.size(); ++i)
            {
                if (i > 0)
                    text += ", ";
                text += value.tags[i];
            }
            return text;
        }
        case EditorPropertyValueType::Curve:
            return "Curve";
        case EditorPropertyValueType::Gradient:
            return "Gradient";
        case EditorPropertyValueType::None:
            break;
        }
        return "";
    }

    inline std::string propertyValueToDisplayString(const EditorPropertyDescriptor& descriptor)
    {
        std::string text = propertyValueToString(descriptor.value);
        if (!descriptor.metadata.units.empty() && !text.empty())
            text += " " + descriptor.metadata.units;
        if (!descriptor.metadata.assetType.empty() && descriptor.metadata.type == EditorPropertyValueType::Asset)
            text += " [" + descriptor.metadata.assetType + "]";
        return text;
    }

    inline bool propertyValueEqualsDefault(const EditorPropertyDescriptor& descriptor)
    {
        return propertyValueToString(descriptor.value) == propertyValueToString(descriptor.metadata.defaultValue);
    }

    inline const EditorPropertyDescriptor*
    findPropertyDescriptor(const std::vector<EditorPropertyDescriptor>& descriptors, const std::string& id)
    {
        const auto it = std::find_if(descriptors.begin(),
                                     descriptors.end(),
                                     [&](const EditorPropertyDescriptor& descriptor)
                                     {
                                         return descriptor.metadata.id == id;
                                     });
        return it == descriptors.end() ? nullptr : &*it;
    }

    inline bool isPropertyVisible(const EditorPropertyDescriptor&              descriptor,
                                  const std::vector<EditorPropertyDescriptor>& descriptors)
    {
        const EditorPropertyVisibilityRule& rule = descriptor.metadata.visibilityRule;
        if (rule.dependsOn.empty())
            return true;

        const EditorPropertyDescriptor* dependency = findPropertyDescriptor(descriptors, rule.dependsOn);
        if (dependency == nullptr)
            return false;

        const bool matches = propertyValueToString(dependency->value) == rule.equalsValue;
        return rule.invert ? !matches : matches;
    }

    inline EditorPropertyWidgetKind choosePropertyWidget(const EditorPropertyMetadata& metadata)
    {
        switch (metadata.type)
        {
        case EditorPropertyValueType::Bool:
            return EditorPropertyWidgetKind::Bool;
        case EditorPropertyValueType::Int:
        case EditorPropertyValueType::UInt:
            return EditorPropertyWidgetKind::Integer;
        case EditorPropertyValueType::Float:
            return EditorPropertyWidgetKind::Float;
        case EditorPropertyValueType::Enum:
            return EditorPropertyWidgetKind::Enum;
        case EditorPropertyValueType::Asset:
            return EditorPropertyWidgetKind::Asset;
        case EditorPropertyValueType::Color:
            return EditorPropertyWidgetKind::Color;
        case EditorPropertyValueType::FloatRange:
            return EditorPropertyWidgetKind::Range;
        case EditorPropertyValueType::Tags:
            return EditorPropertyWidgetKind::Tags;
        case EditorPropertyValueType::Curve:
            return EditorPropertyWidgetKind::Curve;
        case EditorPropertyValueType::Gradient:
            return EditorPropertyWidgetKind::Gradient;
        case EditorPropertyValueType::Text:
        case EditorPropertyValueType::None:
            break;
        }
        return EditorPropertyWidgetKind::Text;
    }

    inline EditorPropertySpec toPropertySpec(const EditorPropertyDescriptor&              descriptor,
                                             const std::vector<EditorPropertyDescriptor>& descriptors)
    {
        EditorPropertySpec spec;
        spec.id = descriptor.metadata.id;
        spec.displayName =
            descriptor.metadata.displayName.empty() ? descriptor.metadata.id : descriptor.metadata.displayName;
        spec.value = propertyValueToDisplayString(descriptor);
        spec.tooltip =
            descriptor.metadata.tooltip.empty() ? descriptor.metadata.description : descriptor.metadata.tooltip;
        spec.category = descriptor.metadata.category;
        spec.widget   = choosePropertyWidget(descriptor.metadata);
        spec.readOnly = descriptor.metadata.readOnly;
        spec.modified = !propertyValueEqualsDefault(descriptor);
        spec.visible  = isPropertyVisible(descriptor, descriptors);
        return spec;
    }

    inline std::vector<EditorPropertySpec> buildPropertySpecs(const std::vector<EditorPropertyDescriptor>& descriptors)
    {
        std::vector<EditorPropertySpec> specs;
        specs.reserve(descriptors.size());
        for (const EditorPropertyDescriptor& descriptor : descriptors)
        {
            EditorPropertySpec spec = toPropertySpec(descriptor, descriptors);
            if (spec.visible)
                specs.push_back(spec);
        }
        return specs;
    }

    inline EditorWidgetHandles createMetadataPropertyGrid(UiSystem&                                    ui,
                                                          UiHandle                                     parent,
                                                          const ToolRect&                              rect,
                                                          const std::vector<EditorPropertyDescriptor>& descriptors,
                                                          BitmapFont*                                  font,
                                                          const EditorTheme& theme = DefaultEditorTheme)
    {
        return createEditorPropertyGrid(ui, parent, rect, buildPropertySpecs(descriptors), font, theme);
    }

    inline EditorPropertyValue boolPropertyValue(bool value)
    {
        EditorPropertyValue property;
        property.type      = EditorPropertyValueType::Bool;
        property.boolValue = value;
        return property;
    }

    inline EditorPropertyValue floatPropertyValue(float value)
    {
        EditorPropertyValue property;
        property.type       = EditorPropertyValueType::Float;
        property.floatValue = value;
        return property;
    }

    inline EditorPropertyValue textPropertyValue(EditorPropertyValueType type, std::string value)
    {
        EditorPropertyValue property;
        property.type      = type;
        property.textValue = std::move(value);
        return property;
    }
} // namespace gts::tools
