#pragma once
#include <cstdint>
#include <limits>

#include "EntityTypes.h"

struct Entity 
{
    entity_id_type id;

    bool operator==(const Entity& other) const
    {
        return id == other.id;
    }

    bool operator!=(const Entity& other) const
    {
        return id != other.id;
    }
};

inline constexpr Entity INVALID_ENTITY = Entity{std::numeric_limits<entity_id_type>::max()};
