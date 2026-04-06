#pragma once

#include <optional>
#include <cstdint>

struct QueueFamilyIndices 
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete(bool requirePresentFamily = true) const
    {
        return graphicsFamily.has_value() && (!requirePresentFamily || presentFamily.has_value());
    }
};
