#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

struct ComponentSignature
{
    using Storage = uint64_t;

    static constexpr uint32_t BitsPerWord  = 64;
    static constexpr uint32_t WordCount    = 4;
    static constexpr uint32_t BitCapacity  = BitsPerWord * WordCount;

    std::array<Storage, WordCount> words{};

    bool containsAll(ComponentSignature other) const
    {
        for (uint32_t i = 0; i < WordCount; ++i)
        {
            if ((words[i] & other.words[i]) != other.words[i])
                return false;
        }
        return true;
    }

    void set(uint32_t bitIndex)
    {
        const uint32_t wordIndex = bitIndex / BitsPerWord;
        const uint32_t bitOffset = bitIndex % BitsPerWord;
        words[wordIndex] |= (Storage{1} << bitOffset);
    }

    void clear(uint32_t bitIndex)
    {
        const uint32_t wordIndex = bitIndex / BitsPerWord;
        const uint32_t bitOffset = bitIndex % BitsPerWord;
        words[wordIndex] &= ~(Storage{1} << bitOffset);
    }

    bool test(uint32_t bitIndex) const
    {
        const uint32_t wordIndex = bitIndex / BitsPerWord;
        const uint32_t bitOffset = bitIndex % BitsPerWord;
        return (words[wordIndex] & (Storage{1} << bitOffset)) != 0;
    }

    bool operator==(const ComponentSignature& other) const = default;
};

struct ComponentSignatureHash
{
    size_t operator()(const ComponentSignature& signature) const
    {
        size_t hash = 1469598103934665603ull;
        for (ComponentSignature::Storage word : signature.words)
        {
            hash ^= static_cast<size_t>(word);
            hash *= 1099511628211ull;
        }
        return hash;
    }
};
