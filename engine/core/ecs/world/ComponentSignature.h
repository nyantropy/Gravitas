#pragma once

#include <cstdint>
#include <limits>

struct ComponentSignature
{
    using Storage = uint64_t;

    static constexpr uint32_t BitCapacity =
        static_cast<uint32_t>(std::numeric_limits<Storage>::digits);

    Storage bits = 0;

    bool containsAll(ComponentSignature other) const
    {
        return (bits & other.bits) == other.bits;
    }

    void set(uint32_t bitIndex)
    {
        bits |= (Storage{1} << bitIndex);
    }

    void clear(uint32_t bitIndex)
    {
        bits &= ~(Storage{1} << bitIndex);
    }

    bool test(uint32_t bitIndex) const
    {
        return (bits & (Storage{1} << bitIndex)) != 0;
    }

    bool operator==(const ComponentSignature& other) const = default;
};

struct ComponentSignatureHash
{
    size_t operator()(const ComponentSignature& signature) const
    {
        return static_cast<size_t>(signature.bits);
    }
};
