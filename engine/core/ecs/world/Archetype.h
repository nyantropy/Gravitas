#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "ComponentSignature.h"
#include "Entity.h"

struct Archetype
{
    ComponentSignature signature{};
    std::vector<Entity> entities;
};

struct EntityLocation
{
    static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

    ComponentSignature signature{};
    uint32_t archetypeIndex = InvalidIndex;
    uint32_t rowIndex = InvalidIndex;

    bool isAssigned() const
    {
        return archetypeIndex != InvalidIndex && rowIndex != InvalidIndex;
    }
};
