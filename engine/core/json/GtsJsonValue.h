#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

struct GtsJsonValue
{
    using Array  = std::vector<GtsJsonValue>;
    using Object = std::vector<std::pair<std::string, GtsJsonValue>>;
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    std::variant<std::monostate, bool, double, std::string, Array, Object, int64_t, uint64_t> value;

    GtsJsonValue() = default;
    GtsJsonValue(bool data) : value(data) {}
    GtsJsonValue(double data) : value(data) {}
    template <std::signed_integral T> GtsJsonValue(T data) : value(static_cast<int64_t>(data)) {}
    template <std::unsigned_integral T> GtsJsonValue(T data) : value(static_cast<uint64_t>(data)) {}
    GtsJsonValue(const char* data) : value(std::string(data)) {}
    GtsJsonValue(std::string data) : value(std::move(data)) {}
    GtsJsonValue(Array data) : value(std::move(data)) {}
    GtsJsonValue(Object data) : value(std::move(data)) {}

    Type type() const
    {
        return isNumber() ? Type::Number : static_cast<Type>(value.index());
    }
    bool isNull() const
    {
        return std::holds_alternative<std::monostate>(value);
    }
    bool isBool() const
    {
        return std::holds_alternative<bool>(value);
    }
    bool isNumber() const
    {
        return std::holds_alternative<double>(value) || isInteger() || isUnsignedInteger();
    }
    bool isInteger() const
    {
        return std::holds_alternative<int64_t>(value);
    }
    bool isUnsignedInteger() const
    {
        return std::holds_alternative<uint64_t>(value);
    }
    bool isString() const
    {
        return std::holds_alternative<std::string>(value);
    }
    bool isArray() const
    {
        return std::holds_alternative<Array>(value);
    }
    bool isObject() const
    {
        return std::holds_alternative<Object>(value);
    }

    bool asBool() const
    {
        return std::get<bool>(value);
    }
    double asNumber() const
    {
        if (isInteger())
            return static_cast<double>(asInteger());
        if (isUnsignedInteger())
            return static_cast<double>(asUnsignedInteger());
        return std::get<double>(value);
    }
    int64_t asInteger() const
    {
        return std::get<int64_t>(value);
    }
    uint64_t asUnsignedInteger() const
    {
        return std::get<uint64_t>(value);
    }
    const std::string& asString() const
    {
        return std::get<std::string>(value);
    }
    const Array& asArray() const
    {
        return std::get<Array>(value);
    }
    const Object& asObject() const
    {
        return std::get<Object>(value);
    }
    const GtsJsonValue*        find(std::string_view key) const;
    std::optional<bool>        tryBool() const;
    std::optional<bool>        findBool(std::string_view key) const;
    std::optional<std::string> tryString() const;
    std::optional<std::string> findString(std::string_view key) const;
    std::optional<double>      tryNumber() const;
    std::optional<double>      findNumber(std::string_view key) const;
    std::optional<int32_t>     tryInt32() const;
    std::optional<int32_t>     findInt32(std::string_view key) const;
    std::optional<uint32_t>    tryUInt32() const;
    std::optional<uint32_t>    findUInt32(std::string_view key) const;
    std::optional<uint64_t>    tryUInt64() const;
    std::optional<uint64_t>    findUInt64(std::string_view key) const;
    std::optional<float>       tryFloat() const;
    std::optional<float>       findFloat(std::string_view key) const;
    const Array*               tryArray() const;
    const Object*              tryObject() const;
    const Array*               findArray(std::string_view key) const;
    const Object*              findObject(std::string_view key) const;
    const GtsJsonValue*        at(size_t index) const;
};
