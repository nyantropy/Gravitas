#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace gts
{
    template <typename Enum>
    struct EnumName
    {
        Enum value;
        std::string_view name;
    };

    template <typename Enum, std::size_t N>
    constexpr std::optional<Enum> enumValue(const std::array<EnumName<Enum>, N>& names, std::string_view name)
    {
        for (const auto& entry : names)
            if (entry.name == name)
                return entry.value;
        return std::nullopt;
    }

    template <typename Enum, std::size_t N>
    constexpr std::optional<std::string_view> enumName(const std::array<EnumName<Enum>, N>& names, Enum value)
    {
        for (const auto& entry : names)
            if (entry.value == value)
                return entry.name;
        return std::nullopt;
    }
}
