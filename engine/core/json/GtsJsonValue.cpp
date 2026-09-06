#include "GtsJsonValue.h"

#include <cmath>
#include <limits>

const GtsJsonValue* GtsJsonValue::find(std::string_view key) const
{
    const auto* object = tryObject();
    if (object == nullptr)
        return nullptr;
    for (const auto& [name, member] : *object)
        if (name == key)
            return &member;
    return nullptr;
}

const GtsJsonValue* GtsJsonValue::at(size_t index) const
{
    const auto* array = tryArray();
    return array != nullptr && index < array->size() ? &(*array)[index] : nullptr;
}

std::optional<bool> GtsJsonValue::tryBool() const
{
    if (!isBool())
        return std::nullopt;
    return asBool();
}

std::optional<std::string> GtsJsonValue::tryString() const
{
    if (!isString())
        return std::nullopt;
    return asString();
}

std::optional<double> GtsJsonValue::tryNumber() const
{
    if (!isNumber())
        return std::nullopt;
    const double number = asNumber();
    if (!std::isfinite(number))
        return std::nullopt;
    return number;
}

std::optional<int32_t> GtsJsonValue::tryInt32() const
{
    if (isInteger())
    {
        const int64_t number = asInteger();
        if (number < std::numeric_limits<int32_t>::min() || number > std::numeric_limits<int32_t>::max())
            return std::nullopt;
        return static_cast<int32_t>(number);
    }
    if (isUnsignedInteger())
    {
        const uint64_t number = asUnsignedInteger();
        if (number > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            return std::nullopt;
        return static_cast<int32_t>(number);
    }
    const auto number = tryNumber();
    if (!number || *number < std::numeric_limits<int32_t>::min() || *number > std::numeric_limits<int32_t>::max() ||
        std::trunc(*number) != *number)
        return std::nullopt;
    return static_cast<int32_t>(*number);
}

std::optional<uint32_t> GtsJsonValue::tryUInt32() const
{
    if (isInteger())
    {
        const int64_t number = asInteger();
        if (number < std::numeric_limits<uint32_t>::min() || number > std::numeric_limits<uint32_t>::max())
            return std::nullopt;
        return static_cast<uint32_t>(number);
    }
    if (isUnsignedInteger())
    {
        const uint64_t number = asUnsignedInteger();
        if (number > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))
            return std::nullopt;
        return static_cast<uint32_t>(number);
    }
    const auto number = tryNumber();
    if (!number || *number < std::numeric_limits<uint32_t>::min() || *number > std::numeric_limits<uint32_t>::max() ||
        std::trunc(*number) != *number)
        return std::nullopt;
    return static_cast<uint32_t>(*number);
}

std::optional<float> GtsJsonValue::tryFloat() const
{
    const auto number = tryNumber();
    if (!number || std::abs(*number) > std::numeric_limits<float>::max())
        return std::nullopt;
    return static_cast<float>(*number);
}

std::optional<uint64_t> GtsJsonValue::tryUInt64() const
{
    if (isUnsignedInteger())
        return asUnsignedInteger();
    if (isInteger())
    {
        const int64_t number = asInteger();
        if (number < 0)
            return std::nullopt;
        return static_cast<uint64_t>(number);
    }
    const auto number = tryNumber();
    if (!number || *number < 0 || *number >= 0x1p64 || std::trunc(*number) != *number)
        return std::nullopt;
    return static_cast<uint64_t>(*number);
}

std::optional<uint64_t> GtsJsonValue::findUInt64(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryUInt64();
}

const GtsJsonValue::Array* GtsJsonValue::tryArray() const
{
    return std::get_if<Array>(&value);
}

const GtsJsonValue::Object* GtsJsonValue::tryObject() const
{
    return std::get_if<Object>(&value);
}

std::optional<bool> GtsJsonValue::findBool(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryBool();
}

std::optional<std::string> GtsJsonValue::findString(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryString();
}

std::optional<double> GtsJsonValue::findNumber(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryNumber();
}

std::optional<int32_t> GtsJsonValue::findInt32(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryInt32();
}

std::optional<uint32_t> GtsJsonValue::findUInt32(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryUInt32();
}

std::optional<float> GtsJsonValue::findFloat(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? std::nullopt : member->tryFloat();
}

const GtsJsonValue::Array* GtsJsonValue::findArray(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? nullptr : member->tryArray();
}

const GtsJsonValue::Object* GtsJsonValue::findObject(std::string_view key) const
{
    const auto* member = find(key);
    return member == nullptr ? nullptr : member->tryObject();
}
