#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ToolCommand.h"

namespace gts::tools
{
    enum class ToolPropertyKind
    {
        Label,
        ReadOnlyText,
        Bool,
        Float,
        Int,
        UInt,
        Enum
    };

    struct ToolPropertyEnumValue
    {
        std::string id;
        std::string label;
        int32_t value = 0;
    };

    struct ToolPropertyDescriptor
    {
        std::string id;
        std::string displayName;
        ToolPropertyKind kind = ToolPropertyKind::ReadOnlyText;
        bool enabled = true;
        bool readOnly = false;
        std::string tooltip;

        std::string textValue;
        bool boolValue = false;
        float floatValue = 0.0f;
        int32_t intValue = 0;
        uint32_t uintValue = 0;

        float minFloat = 0.0f;
        float maxFloat = 1.0f;
        float floatStep = 0.1f;
        int32_t minInt = 0;
        int32_t maxInt = 100;
        int32_t intStep = 1;
        uint32_t minUInt = 0;
        uint32_t maxUInt = 100;
        uint32_t uintStep = 1;

        std::vector<ToolPropertyEnumValue> enumValues;
        ToolCommand command;
    };

    struct ToolPropertySection
    {
        std::string title;
        std::vector<ToolPropertyDescriptor> properties;
    };
} // namespace gts::tools
