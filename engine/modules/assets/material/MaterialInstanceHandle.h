#pragma once
#include <cstdint>
#include <functional>

struct MaterialInstanceHandle
{
    uint32_t id         = 0;
    uint32_t generation = 0;

    bool valid() const
    {
        return id != 0;
    }
};

inline bool operator==(MaterialInstanceHandle lhs, MaterialInstanceHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

inline bool operator!=(MaterialInstanceHandle lhs, MaterialInstanceHandle rhs)
{
    return !(lhs == rhs);
}

namespace std
{
    template <> struct hash<MaterialInstanceHandle>
    {
        size_t operator()(MaterialInstanceHandle handle) const noexcept
        {
            return (static_cast<size_t>(handle.id) << 32u) ^ static_cast<size_t>(handle.generation);
        }
    };

} // namespace std
