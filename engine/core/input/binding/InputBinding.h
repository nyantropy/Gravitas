#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class ModifierFlags : uint8_t
{
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
    Super = 1 << 3
};

inline ModifierFlags operator|(ModifierFlags lhs, ModifierFlags rhs)
{
    return static_cast<ModifierFlags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline ModifierFlags operator&(ModifierFlags lhs, ModifierFlags rhs)
{
    return static_cast<ModifierFlags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

inline ModifierFlags operator~(ModifierFlags value)
{
    return static_cast<ModifierFlags>(~static_cast<uint8_t>(value));
}

inline ModifierFlags& operator|=(ModifierFlags& lhs, ModifierFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool has(ModifierFlags value, ModifierFlags flag)
{
    return static_cast<uint8_t>(value & flag) != 0;
}

enum class ActivationMode : uint8_t
{
    Pressed,
    Released,
    Held,
    Repeated
};

enum class PausePolicy : uint8_t
{
    Gameplay,
    AlwaysActive
};

struct InputTrigger
{
    enum class Type : uint8_t
    {
        Key,
        MouseButton,
        GamepadButton,
        GamepadAxis
    };

    Type          type = Type::Key;
    int           code = 0;
    ModifierFlags modifiers = ModifierFlags::None;
    int           axisIndex = -1;
    float         deadzone = 0.15f;
    float         axisDirection = 1.0f;

    bool operator==(const InputTrigger& other) const;
    size_t hash() const;
};

struct InputBinding
{
    std::string    action;
    InputTrigger   trigger;
    ActivationMode mode        = ActivationMode::Pressed;
    std::string    context     = "";
    PausePolicy    pausePolicy = PausePolicy::Gameplay;
    bool           passthrough = false;

    bool operator==(const InputBinding& other) const;
};
