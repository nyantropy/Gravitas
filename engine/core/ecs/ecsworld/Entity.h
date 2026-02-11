#pragma once
#include <cstdint>

#include "Types.h"

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
