#pragma once

#include <any>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

// Opaque bit identity. Owners outside core assign participation categories.
enum class EcsSystemGroup : uint64_t
{
};

using EcsSystemMask = uint64_t;

constexpr EcsSystemMask toMask(EcsSystemGroup group)
{
    return static_cast<EcsSystemMask>(group);
}

constexpr EcsSystemMask operator|(EcsSystemGroup lhs, EcsSystemGroup rhs)
{
    return toMask(lhs) | toMask(rhs);
}

constexpr EcsSystemMask operator|(EcsSystemMask lhs, EcsSystemGroup rhs)
{
    return lhs | toMask(rhs);
}

constexpr EcsSystemMask operator|(EcsSystemGroup lhs, EcsSystemMask rhs)
{
    return toMask(lhs) | rhs;
}

constexpr bool containsSystemGroup(EcsSystemMask mask, EcsSystemGroup group)
{
    return (mask & toMask(group)) != 0;
}

struct EcsSystemTimingSample
{
    std::string_view name;
    EcsSystemGroup   group          = EcsSystemGroup{};
    uint32_t         instanceIndex  = 0;
    float            updateMs       = 0.0f;
    float            commandFlushMs = 0.0f;
};

// A value-owned selection. Core interprets only its identity and mask. Optional
// typed policy data travels with the same stack entry; there is no parallel stack.
struct EcsExecutionSelection
{
    std::string   id;
    EcsSystemMask enabledSystems = std::numeric_limits<EcsSystemMask>::max();

    EcsExecutionSelection() = default;
    EcsExecutionSelection(std::string id, EcsSystemMask mask) : id(std::move(id)), enabledSystems(mask) {}

    template <typename Policy>
    EcsExecutionSelection(std::string id, EcsSystemMask mask, Policy policy)
        : id(std::move(id)), enabledSystems(mask), policyData(std::move(policy))
    {
    }

    bool contains(EcsSystemGroup group) const
    {
        return containsSystemGroup(enabledSystems, group);
    }

    template <typename Policy> const Policy* policy() const
    {
        return std::any_cast<Policy>(&policyData);
    }

    private:
    std::any policyData;
};
