#include "InputKeyNames.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include "GtsKey.h"

namespace
{
    struct KeyNameEntry
    {
        int             keyCode;
        std::string_view name;
    };

    constexpr std::array<KeyNameEntry, 65> kKeyNames{{
        {static_cast<int>(GtsKey::A), "A"}, {static_cast<int>(GtsKey::B), "B"},
        {static_cast<int>(GtsKey::C), "C"}, {static_cast<int>(GtsKey::D), "D"},
        {static_cast<int>(GtsKey::E), "E"}, {static_cast<int>(GtsKey::F), "F"},
        {static_cast<int>(GtsKey::G), "G"}, {static_cast<int>(GtsKey::H), "H"},
        {static_cast<int>(GtsKey::I), "I"}, {static_cast<int>(GtsKey::J), "J"},
        {static_cast<int>(GtsKey::K), "K"}, {static_cast<int>(GtsKey::L), "L"},
        {static_cast<int>(GtsKey::M), "M"}, {static_cast<int>(GtsKey::N), "N"},
        {static_cast<int>(GtsKey::O), "O"}, {static_cast<int>(GtsKey::P), "P"},
        {static_cast<int>(GtsKey::Q), "Q"}, {static_cast<int>(GtsKey::R), "R"},
        {static_cast<int>(GtsKey::S), "S"}, {static_cast<int>(GtsKey::T), "T"},
        {static_cast<int>(GtsKey::U), "U"}, {static_cast<int>(GtsKey::V), "V"},
        {static_cast<int>(GtsKey::W), "W"}, {static_cast<int>(GtsKey::X), "X"},
        {static_cast<int>(GtsKey::Y), "Y"}, {static_cast<int>(GtsKey::Z), "Z"},
        {static_cast<int>(GtsKey::Digit0), "0"}, {static_cast<int>(GtsKey::Digit1), "1"},
        {static_cast<int>(GtsKey::Digit2), "2"}, {static_cast<int>(GtsKey::Digit3), "3"},
        {static_cast<int>(GtsKey::Digit4), "4"}, {static_cast<int>(GtsKey::Digit5), "5"},
        {static_cast<int>(GtsKey::Digit6), "6"}, {static_cast<int>(GtsKey::Digit7), "7"},
        {static_cast<int>(GtsKey::Digit8), "8"}, {static_cast<int>(GtsKey::Digit9), "9"},
        {static_cast<int>(GtsKey::Space), "SPACE"}, {static_cast<int>(GtsKey::Enter), "ENTER"},
        {static_cast<int>(GtsKey::Escape), "ESCAPE"}, {static_cast<int>(GtsKey::Backspace), "BACKSPACE"},
        {static_cast<int>(GtsKey::Tab), "TAB"}, {static_cast<int>(GtsKey::ArrowUp), "UP"},
        {static_cast<int>(GtsKey::ArrowDown), "DOWN"}, {static_cast<int>(GtsKey::ArrowLeft), "LEFT"},
        {static_cast<int>(GtsKey::ArrowRight), "RIGHT"}, {static_cast<int>(GtsKey::LeftShift), "LEFT_SHIFT"},
        {static_cast<int>(GtsKey::RightShift), "RIGHT_SHIFT"}, {static_cast<int>(GtsKey::LeftCtrl), "LEFT_CONTROL"},
        {static_cast<int>(GtsKey::RightCtrl), "RIGHT_CONTROL"}, {static_cast<int>(GtsKey::LeftAlt), "LEFT_ALT"},
        {static_cast<int>(GtsKey::RightAlt), "RIGHT_ALT"}, {static_cast<int>(GtsKey::LeftSuper), "LEFT_SUPER"},
        {static_cast<int>(GtsKey::RightSuper), "RIGHT_SUPER"}, {static_cast<int>(GtsKey::F1), "F1"},
        {static_cast<int>(GtsKey::F2), "F2"}, {static_cast<int>(GtsKey::F3), "F3"},
        {static_cast<int>(GtsKey::F4), "F4"}, {static_cast<int>(GtsKey::F5), "F5"},
        {static_cast<int>(GtsKey::F6), "F6"}, {static_cast<int>(GtsKey::F7), "F7"},
        {static_cast<int>(GtsKey::F8), "F8"}, {static_cast<int>(GtsKey::F9), "F9"},
        {static_cast<int>(GtsKey::F10), "F10"}, {static_cast<int>(GtsKey::F11), "F11"},
        {static_cast<int>(GtsKey::F12), "F12"}
    }};
}

std::string keyCodeToString(int keyCode)
{
    for (const auto& entry : kKeyNames)
    {
        if (entry.keyCode == keyCode)
            return std::string(entry.name);
    }

    if (keyCode >= static_cast<int>(GtsKey::F1) && keyCode <= static_cast<int>(GtsKey::F12))
        return "F" + std::to_string(keyCode - static_cast<int>(GtsKey::F1) + 1);

    return "UNKNOWN";
}

std::optional<int> stringToKeyCode(const std::string& name)
{
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::toupper(ch));
    });

    for (const auto& entry : kKeyNames)
    {
        if (entry.name == upper)
            return entry.keyCode;
    }

    if (upper.size() == 2 && upper[0] == 'F' && upper[1] >= '1' && upper[1] <= '9')
        return static_cast<int>(GtsKey::F1) + (upper[1] - '1');
    if (upper == "F10")
        return static_cast<int>(GtsKey::F10);
    if (upper == "F11")
        return static_cast<int>(GtsKey::F11);
    if (upper == "F12")
        return static_cast<int>(GtsKey::F12);

    return std::nullopt;
}
